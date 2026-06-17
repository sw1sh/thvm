#!/usr/bin/env wolframscript
(* Iter Z+2 profiling: break down where time goes for each AOT-Metal
   path -- separates emit/compile from kernel dispatch from collapse.
   Helps decide where iter Z+3+ should focus.

   Three paths timed per case:
     Z   = TDef + TAOTRun (Method->Metal) -- single-thread MSL
           reducer, host migration to dyn, host TCollapse.  CORRECT
           when not stuck on APP-of-DP.
     Z+1 = TDef + TAOTRun (KEEP_BOOK=1) + TAOTIcCollapse (parallel
           grid=2^depth).  FAST but non-deterministic, often stuck.
     CPU = pure CPU IC: build formula via TLam etc., TCollapse.
           Reference correctness for small V.

   Run: DEV=metal wolframscript -f bench/sat_collapse/profile_breakdown.wls *)

PacletDirectoryAdd[FileNameJoin[{Directory[], "wl"}]];
Needs["WolframInstitute`THVMLink`"];
TInitContext[];
ctx = TContextNew["metal"];
If[ ctx === 0, Print["SKIP: Metal context unavailable"]; Exit[0]];

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

defCounter = 0;
freshDefName[] := (defCounter += 1; "brk_def_" <> ToString[defCounter]);

(* Path Z: single-thread MSL kernel + host migrate + CPU TCollapse.
   Time slices: build TDef body, kernel dispatch (incl. MSL emit on
   first run + migration), host CPU TCollapse. *)
$tBuildZ = $tKernelZ = $tCollapseZ = 0;
$satZ = False; $nLeavesZ = 0;
runZ[cnf_, nv_] := (
    SetEnvironment["THVM_AOT_METAL_KEEP_BOOK" -> "0"];
    Module[{name = freshDefName[]},
        $tBuildZ = First @ AbsoluteTiming[
            TInContext[ctx, Module[{vars, lit, cl, formula, body},
                vars = Table[TSup[10000 + i, mkT[], mkF[]], {i, nv}];
                lit[l_] := If[l > 0, vars[[l]], TBoolNot[vars[[-l]]]];
                cl[lits_] := Fold[TBoolOr[#1, #2] &, lit /@ lits];
                formula = Fold[TBoolAnd[#1, #2] &, cl /@ cnf];
                body = TLam[ig, boolToNum[formula]];
                TDef[name, body]
            ]]];
        $tKernelZ = First @ AbsoluteTiming[
            $rZ = TInContext[ctx, TAOTRun[name, {TNum[0]}, Method -> "Metal"]]];
        $tCollapseZ = First @ AbsoluteTiming[
            $leavesZ = TInContext[ctx, TCollapse[$rZ]]];
        $satZ = AnyTrue[$leavesZ, (TTermTag[#] === $TagNUM &&
            TTermVal[#] === 1) &];
        $nLeavesZ = Length[$leavesZ];
    ];
);

(* Path Z+1: kernel + parallel collapse on GPU. *)
$tBuildZ1 = $tKernelZ1 = $tCollapseZ1 = 0;
$satZ1 = False; $nLeavesZ1 = 0;
runZ1[cnf_, nv_] := (
    SetEnvironment["THVM_AOT_METAL_KEEP_BOOK" -> "1"];
    Module[{name = freshDefName[], depth = nv + 5},
        $tBuildZ1 = First @ AbsoluteTiming[
            TInContext[ctx, Module[{vars, lit, cl, formula, body},
                vars = Table[TSup[10000 + i, mkT[], mkF[]], {i, nv}];
                lit[l_] := If[l > 0, vars[[l]], TBoolNot[vars[[-l]]]];
                cl[lits_] := Fold[TBoolOr[#1, #2] &, lit /@ lits];
                formula = Fold[TBoolAnd[#1, #2] &, cl /@ cnf];
                body = TLam[ig, boolToNum[formula]];
                TDef[name, body]
            ]]];
        $tKernelZ1 = First @ AbsoluteTiming[
            $rZ1 = TInContext[ctx, TAOTRun[name, {TNum[0]}, Method -> "Metal"]]];
        $tCollapseZ1 = First @ AbsoluteTiming[
            $leavesZ1 = TInContext[ctx, TAOTIcCollapse[$rZ1, depth]]];
        $satZ1 = AnyTrue[$leavesZ1, (TTermTag[#] === $TagNUM &&
            TTermVal[#] === 1) &];
        $nLeavesZ1 = Length[$leavesZ1];
    ];
    SetEnvironment["THVM_AOT_METAL_KEEP_BOOK" -> "0"];
);

(* Path CPU: pure CPU IC. *)
$tCpu = 0; $satCpu = False; $nLeavesCpu = 0;
runCpu[cnf_, nv_] := (
    TReset[];
    Module[{vars, lit, cl, formula, leaves},
        $tCpu = First @ AbsoluteTiming[
            vars = Table[TSup[10000 + i, mkT[], mkF[]], {i, nv}];
            lit[l_] := If[l > 0, vars[[l]], TBoolNot[vars[[-l]]]];
            cl[lits_] := Fold[TBoolOr[#1, #2] &, lit /@ lits];
            formula = Fold[TBoolAnd[#1, #2] &, cl /@ cnf];
            leaves = TCollapse[boolToNum[formula]]
        ];
        $satCpu = AnyTrue[leaves, (TTermTag[#] === $TagNUM &&
            TTermVal[#] === 1) &];
        $nLeavesCpu = Length[leaves];
    ];
);

(* WL ground truth. *)
runWL[cnf_, nv_] := Module[{syms, t, sat},
    syms = Table[Symbol["xb" <> ToString[i]], {i, nv}];
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

fmt[t_] := StringPadLeft[ToString @ NumberForm[t*1000., {7, 2}], 8] <> "ms";

Print["v c | path Z (build / kernel / collapse / sat) | path Z+1 (build / kernel / collapse / sat) | CPU IC (total / sat) | WL (total / sat)"];
Print[StringJoin @@ Table["-", {130}]];

(* Limited to V<=3 because the iter Z per-def MSL emit produces
   ~10K-line shaders for V>=4 / C>=8, and xcrun metallib takes
   minutes (or hangs) compiling them.  This is itself an iter Z+2
   finding -- the def emit needs structural sharing or a runtime
   allocator pattern instead of inlined cell layouts to scale. *)
cases = {{2,4}, {3,6}};
$v=0;$c=0;$cnf={};$tWL=0;$satWL=False;
Do[
    $v = case[[1]]; $c = case[[2]];
    SeedRandom[$v*100 + $c];
    $cnf = randomCnf[$v, $c];
    runZ[$cnf, $v];
    runZ1[$cnf, $v];
    runCpu[$cnf, $v];
    {$tWL, $satWL} = runWL[$cnf, $v];
    Print["v=", $v, " c=", $c];
    Print["  Z   build=", fmt[$tBuildZ],
        " kernel=", fmt[$tKernelZ],
        " collapse=", fmt[$tCollapseZ],
        "  sat=", $satZ, " (n=", $nLeavesZ, ")"];
    Print["  Z+1 build=", fmt[$tBuildZ1],
        " kernel=", fmt[$tKernelZ1],
        " collapse=", fmt[$tCollapseZ1],
        "  sat=", $satZ1, " (n=", $nLeavesZ1, ")"];
    Print["  CPU total=", fmt[$tCpu],
        "  sat=", $satCpu, " (n=", $nLeavesCpu, ")"];
    Print["  WL  total=", fmt[$tWL],
        "  sat=", $satWL];
    Print[""];
, {case, cases}];
