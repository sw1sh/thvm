#!/usr/bin/env wolframscript
(* Profile iter Z+1 (parallel collapse on Metal) vs WL SatisfiableQ
   across V/C scales.  Reports kernel time + dispatch shape per case.

   *Known-issue (iter Z+1 v1)*: results are non-deterministic.  N
   threads parallel-walk the same SUP-tree in BOOK_HEAP and race on
   the shared dup-cells (DUP-LAM/DUP-SUP both see "no SUB", both
   alloc, last write wins).  Apple GPU lacks 64-bit device atomics
   so Term-sized SUB-bit transitions can't be made atomic without
   re-splitting Term as a 32+32-bit pair or per-thread heap arenas.
   Iter Z+2 should address.

   Per-CNF correctness today: smaller V (2-4) tend to find the right
   answer; larger V often mismatches because too few threads reach
   NUM-typed leaves before getting stuck on APP-of-DP chains.  Same
   stuck shape that CNF_DIVERGENCE.md describes; the per-thread wnf
   state machine inherits the limitation.

   Run: THVM_BACKEND=metal wolframscript -f bench/sat_collapse/profile_aot_metal.wls *)

PacletDirectoryAdd[FileNameJoin[{Directory[], "wl"}]];
Needs["THVMLink`"];
TInitContext[];
print[a___] := WriteString["stdout", StringRiffle[ToString /@ {a}, " "], "\n"];

ctx = TContextNew["metal"];
If[ ctx === 0, print["SKIP: Metal context unavailable"]; Exit[0]];

mkT[] := With[{t = Module[{ts}, ts], f = Module[{fs}, fs]},
    TLam[t, TLam[f, t]]];
mkF[] := With[{t = Module[{ts}, ts], f = Module[{fs}, fs]},
    TLam[t, TLam[f, f]]];
boolToNum[b_] := TApp[TApp[b, TNum[1]], TNum[0]];
TBoolAnd[a_, b_] := With[{x = Module[{xs}, xs], y = Module[{ys}, ys]},
    TApp[TApp[TApp[a, TLam[x, x]], TLam[y, mkF[]]], b]];
TBoolOr[a_, b_] := With[{x = Module[{xs}, xs], y = Module[{ys}, ys]},
    TApp[TApp[TApp[a, TLam[x, mkT[]]], TLam[y, y]], b]];
TBoolNot[a_] := With[{t = Module[{ts}, ts], f = Module[{fs}, fs]},
    TLam[t, TLam[f, TApp[TApp[a, f], t]]]];
buildFormula[cnf_, nVars_] := Module[{vars, litTerm, clauseTerm, formula},
    vars = Table[TSup[10000 + i, mkT[], mkF[]], {i, nVars}];
    litTerm[lit_] := If[lit > 0, vars[[lit]], TBoolNot[vars[[-lit]]]];
    clauseTerm[lits_] := Fold[TBoolOr[#1, #2] &, litTerm /@ lits];
    formula = Fold[TBoolAnd[#1, #2] &, clauseTerm /@ cnf];
    boolToNum[formula]
];

defCounter = 0;
freshDefName[] := (defCounter += 1; "prof_def_" <> ToString[defCounter]);

(* Path C: iter Z+1 parallel collapse.  depth = nVars + 1 covers V
   variable SUPs + 1 from the boolToNum APP-SUP commute. *)
(* Top-level globals so runC can mutate without nested scoping issues. *)
$tBuild = 0; $tKernel = 0; $tCollapse = 0; $depth = 0;
$satC = False; $nLeaves = 0; $defLast = "";

runC[cnf_, nv_] := (
    $depth = nv + 5;   (* +5 covers APP-SUP commutes through boolToNum + AND/OR layers *)
    SetEnvironment["THVM_AOT_METAL_KEEP_BOOK" -> "1"];
    (* Snapshot book_next BEFORE this iter's def + collapse; restore
       after so subsequent iters get the same starting headroom (the
       prior iter's collapse arena is no longer needed once we read
       its result Terms here).  Must be inside TInContext[ctx, ...]
       because each ctx owns its own BOOK_NEXT. *)
    $bookNextSnap = TInContext[ctx, THVMLink`Private`$bookPosFn[]];
    $defLast = freshDefName[];
    $tBuild = First @ AbsoluteTiming[
        TInContext[ctx, Module[{vars, litTerm, clauseTerm, formula, body},
            vars = Table[TSup[10000 + i, mkT[], mkF[]], {i, nv}];
            litTerm[lit_] := If[lit > 0, vars[[lit]], TBoolNot[vars[[-lit]]]];
            clauseTerm[lits_] := Fold[TBoolOr[#1, #2] &, litTerm /@ lits];
            formula = Fold[TBoolAnd[#1, #2] &, clauseTerm /@ cnf];
            body    = TLam[ig, boolToNum[formula]];
            TDef[$defLast, body]
        ]]
    ];
    $tKernel = First @ AbsoluteTiming[
        $ker = TInContext[ctx, TAOTRun[$defLast, {TNum[0]}, Method -> "Metal"]]
    ];
    $tCollapse = First @ AbsoluteTiming[
        $col = TInContext[ctx, TAOTIcCollapse[$ker, $depth]]
    ];
    SetEnvironment["THVM_AOT_METAL_KEEP_BOOK" -> "0"];
    (* Diagnostic: count NUM leaves, ERA-iter-cap leaves, ERA-stack-cap
       leaves, plain ERA, and stuck-other (APP/SUP/etc.).  Avoid
       FromTTerm on stuck leaves -- some have invalid heap loc fields. *)
    $satC    = AnyTrue[$col, (TTermTag[#] === $TagNUM && TTermVal[#] === 1) &];
    $nLeaves = Length[$col];
    $nNum    = Length @ Select[$col, TTermTag[#] === $TagNUM &];
    $nIterCap  = Length @ Select[$col,
        TTermTag[#] === $TagERA && TTermExt[#] === 16^^FFFFE &];
    $nStackCap = Length @ Select[$col,
        TTermTag[#] === $TagERA && TTermExt[#] === 16^^FFFFF &];
    $nEra      = Length @ Select[$col,
        TTermTag[#] === $TagERA && TTermExt[#] === 0 &];
    $nStuck = $nLeaves - $nNum - $nIterCap - $nStackCap - $nEra;
    $stuckTags = Counts[TTermTag /@ Select[$col, TTermTag[#] =!= $TagNUM &]];
    (* Restore book_next so the next iter starts with a clean headroom. *)
    TInContext[ctx, THVMLink`Private`$bookSetNextFn[$bookNextSnap]];
);

(* Path D: WL SatisfiableQ (CDCL ground truth). *)
runD[cnf_, nv_] := Module[{syms, t, sat},
    syms = Table[Symbol["xpD" <> ToString[i]], {i, nv}];
    t = First @ AbsoluteTiming[
        sat = SatisfiableQ[
            And @@ (Or @@ # & /@
                Map[If[# > 0, syms[[#]], Not[syms[[-#]]]] &, cnf, {2}]),
            syms]];
    {t, sat}
];

SeedRandom[42];
randomCnf[v_, c_] := Table[
    Module[{vs = RandomSample[Range[v], Min[3, v]]},
        vs * RandomChoice[{-1, 1}, Length[vs]]
    ], {c}];

print["v   c   build  kernel  collapse(threads)   total   WL ms    sat (C/D)  agree?"];
print[StringJoin @@ Table["-", {84}]];

cases = {
    {2,  4}, {3,  6}, {4,  8}, {5, 10}, {6, 12}, {7, 14},
    {8, 16}, {9, 18}, {10, 20}
};

$v = 0; $c = 0; $cnf = {}; $td = 0; $satD = False; $total = 0;
Do[
    $v = case[[1]];
    $c = case[[2]];
    SeedRandom[$v*100 + $c];
    $cnf = randomCnf[$v, $c];
    runC[$cnf, $v];
    {$td, $satD} = runD[$cnf, $v];
    $total = $tBuild + $tKernel + $tCollapse;
    fmt[t_] := StringPadLeft[ToString @ NumberForm[t*1000., {6, 2}], 7] <> "ms";
    print[StringPadRight[ToString[$v], 4],
        StringPadRight[ToString[$c], 4],
        fmt[$tBuild], "  ", fmt[$tKernel], "  ",
        fmt[$tCollapse], " (n=", $nLeaves, " num=", $nNum, " stk=", $nStuck, " iter-cap=", $nIterCap, " stk-cap=", $nStackCap, " era=", $nEra, ")  total=",
        fmt[$total], "  WL=", fmt[$td],
        "  ", $satC, "/", $satD, "  ",
        If[$satC === $satD, "OK", "MISMATCH"],
        "  stuck-tags=", $stuckTags
    ];
, {case, cases}];
