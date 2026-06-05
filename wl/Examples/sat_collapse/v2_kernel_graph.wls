#!/usr/bin/env wolframscript
(* SAT solver v2 -- compute-graph codegen + Metal rendering.
   (NOT the TAOTRun AOT path -- see notes below.)

   Different mental model from v1_church.wls (collapse-based):
   v2 brute-forces all 2^V assignments simultaneously as a vectorised
   tensor program.  Each variable's column is a precomputed [2^V]
   bit-pattern tensor; clauses combine them via TUOpAdd / TUOpMul /
   TUOpNeg into a final [2^V] tensor of 0/1; satisfying assignments
   are the indices where the result is 1.  TRealize materializes the
   UOP graph into UOP_KERNEL nodes which the Metal backend renders
   into MSL and dispatches.

   Why NOT TAOTRun + Method -> "Metal":
   That path (src/aot/metal_emit.c, iter D-G coverage) compiles a
   TDef'd body composed of TNum literals + TOp2 folds + TMat over
   N scalar (Term) args into one MSL kernel that returns one scalar
   Term per launch.  For SAT brute force that means 2^V launches,
   each ~190us, structurally wrong: V=16 -> 12 s of overhead alone.
   The compute-graph path runs the WHOLE 2^V evaluation in a chain
   of large-tensor kernels instead -- one launch per fused
   elementwise op, not 2^V launches per scalar.

   Status: correct through V=20.  Wins over baseline.wls (per-
   assignment serial loop) handily but trails WL CDCL by 8-600x as
   V grows.  Bottleneck is kernel fusion: every TUOpAdd / TUOpMul /
   TUOpNeg currently fires as its OWN Metal launch (~190us each).
   With C clauses x ~5 elementwise ops per clause we pay ~5C launches
   per evaluation; V=20 / C=80 = 400 launches just for the formula.
   Kernel-fusion across elementwise chains (the autotune-ladder
   campaign in bench/autotune-ladder/) would close most of that gap.

   Usage: DEV=metal wolframscript -f v2_kernel_graph.wls
*)

PacletDirectoryAdd[FileNameJoin[{Directory[], "wl"}]];
Needs["THVMLink`"];
If[ Environment["DEV"] === $Failed || Environment["DEV"] === "",
    SetEnvironment["DEV" -> "metal"]];

ctx = TContextNew["metal"];
If[ ctx === 0,
    Print["SKIP: Metal context unavailable"];
    Exit[0]];

(* Build the per-variable column tensor for nVars in [2^V, V] order:
   row k is the binary vector of integer k (LSB = var 1).  Column j
   is the bit pattern of var j across all assignments. *)
varColumns[nVars_] := Table[
    Module[{patt = Table[BitAnd[BitShiftRight[k, j - 1], 1], {k, 0, 2^nVars - 1}]},
        TTensorCreate[NumericArray[patt, "Real32"]]
    ],
    {j, nVars}];

(* Literal -> [2^V] f32 tensor of 0/1.
   Positive literal: just the column.  Negative: 1 - column. *)
litTensor[lit_, cols_, oneVec_] :=
    If[ lit > 0,
        cols[[lit]],
        TUOpAdd[oneVec, TUOpNeg[cols[[-lit]]]]];

(* Clause: 1 - prod_i(1 - lit_i) -- at least one literal true. *)
clauseTensor[lits_List, cols_, oneVec_] := Module[{negs, prod},
    negs = Table[TUOpAdd[oneVec, TUOpNeg[litTensor[l, cols, oneVec]]], {l, lits}];
    prod = Fold[TUOpMul[#1, #2] &, First[negs], Rest[negs]];
    TUOpAdd[oneVec, TUOpNeg[prod]]
];

(* Formula: prod_clauses(clause_value) -- all clauses must be true. *)
formulaTensor[cnf_, cols_, oneVec_] := Module[{cs},
    cs = clauseTensor[#, cols, oneVec] & /@ cnf;
    Fold[TUOpMul[#1, #2] &, First[cs], Rest[cs]]
];

(* Run the formula on the active backend; return host list of 0/1.
   Each call allocates fresh tensors -- TReset across calls leaves the
   kernel-program cache referencing stale buffers, so we just lean on
   GC + the runtime hash-cons.  Deterministic: same CNF -> same vec. *)
satEvalAll[cnf_, nVars_] := TInContext[ctx, Module[{cols, oneVec, formula, result},
    cols    = varColumns[nVars];
    oneVec  = TTensorCreate[NumericArray[Table[1.0, 2^nVars], "Real32"]];
    formula = formulaTensor[cnf, cols, oneVec];
    result  = TRealize[formula];
    Round @ Normal @ TTensorData[result]
]];

(* Decode hits from the [2^V] result vector. *)
satSolutions[cnf_, nVars_] := Module[{vec},
    vec = satEvalAll[cnf, nVars];
    Select[Range[0, 2^nVars - 1], vec[[# + 1]] === 1 &]
];

(* ---- Correctness: compare against WL SatisfiableQ on small inputs. ---- *)

Print["=== Correctness ==="];
testCases = {
    {"sat-trivial",     {{1}},                                     1, True},
    {"unsat-x-and-not-x", {{1}, {-1}},                             1, False},
    {"sat-3var",        {{1, 2}, {-1, 3}, {-2, -3}},               3, True},
    {"unsat-3var",      {{1}, {-1, 2}, {-2}, {2}},                 2, False},
    {"sat-pigeon-2",    {{1, 2}, {-1, -2}},                        2, True},
    {"sat-x-or-y-or-z", {{1, 2, 3}},                               3, True}
};
Do[Module[{name = tc[[1]], cnf = tc[[2]], nv = tc[[3]], expected = tc[[4]],
            ourSat, syms, wlAns},
    ourSat = Length[satSolutions[cnf, nv]] > 0;
    syms = Table[Symbol["xz" <> ToString[i]], {i, nv}];
    wlAns = SatisfiableQ[
        And @@ (Or @@ # & /@
            Map[If[# > 0, syms[[#]], Not[syms[[-#]]]] &, cnf, {2}]),
        syms];
    Print[StringPadRight[name, 22],
        " ours=", ourSat, " wl=", wlAns, " expected=", expected,
        If[ourSat === expected && wlAns === expected, " OK", " FAIL"]]
], {tc, testCases}];

(* ---- Bench: for V in {8, 12, 16, 20}, time thvm-Metal vs WL. ---- *)

Print[""];
Print["=== Bench thvm-Metal vs WL SatisfiabilityInstances ==="];
SeedRandom[42];
randomCnf[nv_, nc_] := Table[
    Module[{vs = RandomSample[Range[nv], 3]},
        vs * RandomChoice[{-1, 1}, 3]
    ], {nc}];

Do[Module[{nv, nc, cnf, syms, ourSols, wlSols, tThvm, tWl},
    nv = sz; nc = Round[sz * 4];
    cnf = randomCnf[nv, nc];
    syms = Table[Symbol["xb" <> ToString[i]], {i, nv}];
    (* Warm: one dispatch to materialize kernels. *)
    satEvalAll[cnf, nv];
    tThvm = First @ AbsoluteTiming[ourSols = satSolutions[cnf, nv]];
    tWl   = First @ AbsoluteTiming[
        wlSols = SatisfiabilityInstances[
            And @@ (Or @@ # & /@
                Map[If[# > 0, syms[[#]], Not[syms[[-#]]]] &, cnf, {2}]),
            syms, 2^nv]
    ];
    Print["V=", nv, " C=", nc,
        "  thvm:", NumberForm[tThvm * 1000., {7, 2}], "ms",
        "  WL:",   NumberForm[tWl * 1000., {7, 2}],   "ms",
        "  thvm sols=", Length[ourSols],
        "  WL sols=",   Length[wlSols],
        "  ratio=", NumberForm[tThvm / Max[tWl, 0.0001], {5, 2}]]
], {sz, {6, 8, 10, 12, 14, 16, 18, 20}}];
