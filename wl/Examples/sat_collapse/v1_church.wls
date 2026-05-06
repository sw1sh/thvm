#!/usr/bin/env wolframscript
(* SAT solver v1 -- Church-encoded booleans + SUP-distribution via
   APP-SUP commute (the Victor Taelin recipe).

   Variables become SUP^L_i { T, F } with distinct labels per
   variable; AND / OR are APP chains over the Church bools so SUP
   propagates through the formula via interact_app_sup; TCollapse
   enumerates the leaves; sat-check = "any leaf reduces to NUM(1)?". *)

PacletDirectoryAdd[FileNameJoin[{Directory[], "wl"}]];
Needs["THVMLink`"];
TInitContext[];
print[a___] := WriteString["stdout", StringRiffle[ToString /@ {a}, " "], "\n"];

(* ---- Church-bool primitives, registered as TDefs so each use
   alo-realizes a fresh dyn copy.  The TLam binders use Module-fresh
   symbols so they don't shadow each other. ---- *)

setupBoolPrims[] := (
    With[{t = Module[{ts}, ts], f = Module[{fs}, fs]},
         TDef["T_", TLam[t, TLam[f, t]]]];
    With[{t = Module[{ts}, ts], f = Module[{fs}, fs]},
         TDef["F_", TLam[t, TLam[f, f]]]];
);

(* Public refs.  Use TRef["T_"] each time (NOT a single TLam) so each
   substitution site gets a fresh LAM copy via APP-REF. *)
TT := TRef["T_"]
FF := TRef["F_"]

(* (And a b) = (a (\x.x) (\x.F) b) -- if a then b else F. *)
TBoolAnd[a_, b_] := With[{
    x = Module[{xs}, xs], y = Module[{ys}, ys]
},
    TApp[TApp[TApp[a, TLam[x, x]], TLam[y, FF]], b]
]

(* (Or a b) = (a (\x.T) (\x.x) b) -- if a then T else b. *)
TBoolOr[a_, b_] := With[{
    x = Module[{xs}, xs], y = Module[{ys}, ys]
},
    TApp[TApp[TApp[a, TLam[x, TT]], TLam[y, y]], b]
]

(* (Not a) = \t.\f.(a f t) -- swap branches. *)
TBoolNot[a_] := With[{
    t = Module[{ts}, ts], f = Module[{fs}, fs]
},
    TLam[t, TLam[f, TApp[TApp[a, f], t]]]
]

(* Reduce a Church bool to NUM(0/1) for inspection / sat-check. *)
boolToNum[b_] := TApp[TApp[b, TNum[1]], TNum[0]]

(* ---- SAT solver ---- *)

(* satCheck[cnf, nVars] returns True if the CNF is satisfiable.
   `cnf` = list of clauses; each clause = list of literals (positive
   var index = positive lit, negative = negation).  Builds Church-
   encoded formula with each var as SUP^i{T, F}, collapses, looks
   for any NUM(1) leaf. *)
satCheck[cnf_List, nVars_Integer] := Module[{
    setupBoolPrims, vars, litTerm, clauseTerm, formula, leaves
},
    setupBoolPrims := (
        With[{t = Module[{ts}, ts], f = Module[{fs}, fs]},
             TDef["T_", TLam[t, TLam[f, t]]]];
        With[{t = Module[{ts}, ts], f = Module[{fs}, fs]},
             TDef["F_", TLam[t, TLam[f, f]]]];
    );
    setupBoolPrims;
    (* Each variable: SUP at a distinct label (10000 + i). *)
    vars = Table[TSup[10000 + i, TRef["T_"], TRef["F_"]], {i, nVars}];
    litTerm[lit_] :=
        If[lit > 0, vars[[lit]], TBoolNot[vars[[-lit]]]];
    clauseTerm[lits_] := Fold[TBoolOr[#1, #2] &, litTerm /@ lits];
    formula = Fold[TBoolAnd[#1, #2] &, clauseTerm /@ cnf];
    leaves = FromTTerm /@ TCollapse[boolToNum[formula]];
    AnyTrue[leaves, # === 1 &]
]

(* ---- Tests vs WL ground truth ---- *)

testCases = {
    {"sat-trivial",  {{1}},                       1, True},
    {"unsat-x-and-not-x",
                     {{1}, {-1}},                 1, False},
    {"sat-3var",     {{1, 2}, {-1, 3}, {-2, -3}}, 3, True},
    {"unsat-3var",   {{1}, {-1, 2}, {-2}, {2}},   2, False},
    {"sat-pigeon-2", {{1, 2}, {-1, -2}},          2, True},
    {"sat-x-or-y-or-z",
                     {{1, 2, 3}},                 3, True}
};

print["=== Correctness check vs WL ==="];
Do[Module[{name = tc[[1]], cnf = tc[[2]], nv = tc[[3]], expected = tc[[4]],
            ours, syms, wlForm, wlAns},
    ours = satCheck[cnf, nv];
    syms = Table[Symbol["x" <> ToString[i]], {i, nv}];
    wlForm = And @@ (Or @@ # & /@
        Map[If[# > 0, syms[[#]], Not[syms[[-#]]]] &, cnf, {2}]);
    wlAns = SatisfiableQ[wlForm, syms];
    print[StringPadRight[name, 22],
        " ours=", ours, " wl=", wlAns, " expected=", expected,
        If[ours === expected && wlAns === expected, " OK", " FAIL"]]
], {tc, testCases}]

(* ---- Benchmark vs WL SatisfiableQ on small random 3-CNF ----

   The earlier cnf_dp wall (V=4 / C=7+ producing stuck APP-of-DP
   leaves) is gone -- landed the HVM4 DP/BJ split (plain DPs fire
   at wnf, book templates carry BJ, alo_realize unfolds BJ -> fresh
   dyn DP).  See CNF_DIVERGENCE.md and README.md.  v1 is now correct
   through V=9 / C=15. *)
print[""];
print["=== Bench (small structured CNFs) ==="];

(* Hand-picked sat / unsat instances + tiny random ones with low
   clause ratio so the per-var auto-dup chain stays shallow. *)
benchCases = {
    {"sat-3c-3v",   {{1, 2, 3}, {-1, 2}, {-2, 3}}, 3},
    {"unsat-3c-2v", {{1}, {-1, 2}, {-2}}, 2},
    {"sat-4c-3v",   {{1, 2}, {-1, 3}, {-2, -3}, {1, -2}}, 3},
    {"sat-5c-4v",   {{1, 2}, {-1, 3}, {-2, 4}, {3, 4}, {-3, -4}}, 4}
};
cnfToWL[cnf_, syms_] := And @@ (Or @@ # & /@
    Map[If[# > 0, syms[[#]], Not[syms[[-#]]]] &, cnf, {2}]);

Do[Module[{name = bc[[1]], cnf = bc[[2]], nv = bc[[3]],
            syms, ours, wl, tOurs, tWl},
    syms = Table[Symbol["x" <> ToString[i]], {i, nv}];
    tOurs = First @ AbsoluteTiming[ours = satCheck[cnf, nv]];
    tWl   = First @ AbsoluteTiming[wl = SatisfiableQ[cnfToWL[cnf, syms], syms]];
    print[StringPadRight[name, 16],
        " thvm:", NumberForm[tOurs * 1000., {7, 2}], "ms",
        " WL:", NumberForm[tWl * 1000., {7, 2}], "ms",
        " sat=", ours, "/", wl,
        " ratio:", NumberForm[tOurs / Max[tWl, 0.0001], {5, 2}],
        If[ours === wl, " OK", " FAIL"]]
], {bc, benchCases}]
