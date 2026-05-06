#!/usr/bin/env wolframscript
(* SAT solver v3 -- AOT-on-Metal IC reducer (iter Z).

   v1_church.wls runs the Church-bool SAT formula via the CPU IC
   reducer; v3 routes the same formula through TAOTRun + Method ->
   "Metal".  The kernel performs the IC interactions (app_lam,
   app_sup, dup_lam, dup_sup, dup_num) on the shared book heap in MSL,
   leaves a SUP-tree of unreduced APPs at the root, and the host
   migrates book->dyn before TCollapse walks the tree.

   This iter is correctness-only.  Performance is not the goal:
   single-thread MSL runs the reducer sequentially; the win at this
   layer comes when iter Z+1 lands the parallel cnf+collapse shader,
   which walks the SUP-tree's N independent leaves on N GPU threads.

   Usage: THVM_BACKEND=metal wolframscript -f v3_aot_metal.wls
*)

PacletDirectoryAdd[FileNameJoin[{Directory[], "wl"}]];
Needs["THVMLink`"];
TInitContext[];
print[a___] := WriteString["stdout", StringRiffle[ToString /@ {a}, " "], "\n"];

ctx = TContextNew["metal"];
If[ ctx === 0,
    print["SKIP: Metal context unavailable"];
    Exit[0]];

(* ---- Church bools as inline TLams.  No TRef needed: the AOT emit
   inlines TRef bodies recursively (same effect as alo_realize for the
   closed-term case).  We just build T/F freshly per use; the kernel's
   DUP-LAM via auto_dup handles sharing within reduction. ---- *)

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

(* ---- Run a CNF through the AOT-Metal IC reducer.
   Wraps the formula in a single dummy outer LAM so TAOTRun can pass
   one TNum arg (the host wrapper's iter-D code path requires >= 1
   arg slot in some builds).  Returns the leaf-value list. ---- *)

defCounter = 0;
freshDefName[] := (defCounter += 1; "v3_def_" <> ToString[defCounter]);

satCheckMetal[cnf_List, nVars_Integer] := Module[{name, r, leaves},
    name = freshDefName[];
    (* Build the entire formula INSIDE the metal context so all its
       heap-loc references live in ctx's HEAP.  Cross-context body
       construction silently produces invalid TTerms. *)
    TInContext[ctx, Module[{vars, litTerm, clauseTerm, formula, body},
        vars = Table[TSup[10000 + i, mkT[], mkF[]], {i, nVars}];
        litTerm[lit_] := If[lit > 0, vars[[lit]], TBoolNot[vars[[-lit]]]];
        clauseTerm[lits_] := Fold[TBoolOr[#1, #2] &, litTerm /@ lits];
        formula = Fold[TBoolAnd[#1, #2] &, clauseTerm /@ cnf];
        body    = TLam[ig, boolToNum[formula]];
        TDef[name, body]
    ]];
    r      = TInContext[ctx, TAOTRun[name, {TNum[0]}, Method -> "Metal"]];
    leaves = TInContext[ctx, FromTTerm /@ TCollapse[r]];
    AnyTrue[leaves, # === 1 &]
];

(* ---- Tests vs WL ground truth ---- *)

testCases = {
    {"sat-trivial",       {{1}},                       1, True},
    {"unsat-x-and-not-x", {{1}, {-1}},                 1, False},
    {"sat-3var",          {{1, 2}, {-1, 3}, {-2, -3}}, 3, True},
    {"unsat-3var",        {{1}, {-1, 2}, {-2}, {2}},   2, False},
    {"sat-pigeon-2",      {{1, 2}, {-1, -2}},          2, True},
    {"sat-x-or-y-or-z",   {{1, 2, 3}},                 3, True}
};

print["=== Correctness vs WL SatisfiableQ ==="];
nOk = 0; nFail = 0;
Do[Module[{name = tc[[1]], cnf = tc[[2]], nv = tc[[3]], expected = tc[[4]],
            ours, syms, wlAns},
    ours = satCheckMetal[cnf, nv];
    syms = Table[Symbol["xv3" <> ToString[i]], {i, nv}];
    wlAns = SatisfiableQ[
        And @@ (Or @@ # & /@
            Map[If[# > 0, syms[[#]], Not[syms[[-#]]]] &, cnf, {2}]),
        syms];
    If[ours === expected && wlAns === expected,
        nOk += 1;
        print[StringPadRight[name, 22],
            " ours=", ours, " wl=", wlAns, " expected=", expected, " OK"]
        ,
        nFail += 1;
        print[StringPadRight[name, 22],
            " ours=", ours, " wl=", wlAns, " expected=", expected, " FAIL"]
    ]
], {tc, testCases}];

print[""];
print["passed: ", nOk, " / ", Length[testCases]];
If[nFail > 0, Exit[1]];
