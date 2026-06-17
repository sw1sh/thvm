#!/usr/bin/env wolframscript
(* atp_ic/substitutivity.wls -- IC-native ATP, atomic axioms applied
   to structured conjectures.

   Generalizes transitivity.wls from "two atoms" to "two flat
   structured terms with a shared head and N atomic positions".

   Conjecture form: {head[atoms_lhs...], head[atoms_rhs...]} where
   the heads match (else the conjecture is vacuously False under
   atomic-axiom-only proofs -- atomic rewrites can't change a
   function head).

   Atomic axioms only: each {old, new} pair rewrites a single
   atomic position.

   State: list of N NUMs, where N = (#atoms_lhs + #atoms_rhs).
   Indexed 0..N-1; positions [0, K-1] are LHS and [K, N-1] are RHS
   for K = #atoms_lhs.

   Per step: pick (rewrite_fn, position) via a 2D SUP fan-out, apply
   the rewrite at the chosen position only; other positions pass
   through.

   Final check: structurally equal iff (heads match) AND (every
   position-i lhs atom equals position-i rhs atom).  Heads are
   constant in the encoding (axioms can't touch them), so the only
   thing to check is per-position atom equality.  Combine via
   OP2[*, ...] -- works now that OP2-NUM-SUP is in the runtime. *)

PacletDirectoryLoad[FileNameJoin[{Directory[], "wl", "THVMLink"}]];
Needs["WolframInstitute`THVMLink`"];
TInitContext[];

(* ---------------------------------------------------------------- *)
(* Encoder                                                          *)
(* ---------------------------------------------------------------- *)

(* Extract a list of atomic positions from one side of a conjecture.
   Bare atom -> {atom} (1-position).  head[a, b, c] -> {a, b, c}.
   Returns the head symbol as a separate value. *)
sideToAtoms[expr_Symbol] := {Null, {expr}}
sideToAtoms[expr_] := {Head[expr], List @@ expr}

(* Collect every atomic symbol appearing in axioms + conjecture
   (excluding heads), assign each a positive Int ID.  Returns
   <|sym -> id|>. *)
collectAtoms[axioms_, conjecture_] := Module[{
    axAtoms, lhsRes, rhsRes, atoms
},
    axAtoms = Flatten[axioms];
    lhsRes = sideToAtoms[conjecture[[1]]];
    rhsRes = sideToAtoms[conjecture[[2]]];
    atoms = DeleteDuplicates @ Join[axAtoms, lhsRes[[2]], rhsRes[[2]]];
    AssociationThread[atoms -> Range[Length[atoms]]]
]

(* Define one (axiom, direction) rewrite as a TDef'd lambda. *)
defineRewriteFn[name_String, old_Integer, new_Integer] := With[{
    v = Module[{vs}, vs]
},
    TDef[name, TLam[v, TIfZero[TOp2["==", v, TNum[old]],
                               v,
                               TNum[new]]]]
]

(* Register fwd + bwd refs per axiom, return the flat list. *)
allRewriteRefs[axioms_, atomMap_] := Flatten @ Table[
    Module[{ax = axioms[[i]], oldId, newId,
            fwdName, bwdName},
        oldId = atomMap[ax[[1]]];
        newId = atomMap[ax[[2]]];
        fwdName = "atp_ax" <> ToString[i] <> "_fwd";
        bwdName = "atp_ax" <> ToString[i] <> "_bwd";
        defineRewriteFn[fwdName, oldId, newId];
        defineRewriteFn[bwdName, newId, oldId];
        {TRef[fwdName], TRef[bwdName]}
    ],
    {i, Length[axioms]}
]

(* Right-associated nested SUP over a flat list of values. *)
nestedSup[baseLab_Integer, ts_List] := Module[{n = Length[ts]},
    Which[
        n === 1, ts[[1]],
        n === 2, TSup[baseLab, ts[[1]], ts[[2]]],
        True, TSup[baseLab, ts[[1]],
                   nestedSup[baseLab + 1, Rest[ts]]]
    ]
]

(* sideUpdate analog generalized to N positions: apply the rewrite
   at position v only if posChoice picks myPos.  posChoice is a SUP
   over NUM(0)..NUM(N-1); myPos is the static integer position of
   this atom. *)
posUpdate[v_, myPosNum_, posChoice_, rChoice_] :=
    TIfZero[TOp2["==", posChoice, myPosNum],
            v,
            TApp[rChoice, v]]

$supBase = 100;

(* Per-step Fold: each step picks one rewrite + one position from
   {0..N-1}.  Returns updated atoms list (one updated, rest
   passthrough) + the new label cursor. *)
stepFold[rFns_, n_, nPos_][acc_, stepIdx_] := Module[{
    atoms = acc[[1]], labCur = acc[[2]],
    rSup, posSup
},
    rSup = nestedSup[labCur, rFns];
    posSup = nestedSup[labCur + n,
        Table[TNum[i], {i, 0, nPos - 1}]];
    {
        Table[
            posUpdate[atoms[[i + 1]], TNum[i], posSup, rSup],
            {i, 0, nPos - 1}
        ],
        labCur + n + nPos
    }
]

(* AND-combine N TOp2["=="] results via a left-fold of TOp2["*"]:
   eq_0 * eq_1 * ... * eq_{n-1} = 1 iff all are 1. *)
allEqual[lhsAtoms_List, rhsAtoms_List] := Module[{
    pairs = Transpose[{lhsAtoms, rhsAtoms}],
    eqs
},
    eqs = TOp2["==", #[[1]], #[[2]]] & /@ pairs;
    Fold[TOp2["*", #1, #2] &, First[eqs], Rest[eqs]]
]

buildSearchTerm[conjecture_, axioms_, depth_Integer] := Module[{
    atomMap, lhsHead, lhsAtomSyms, rhsHead, rhsAtomSyms,
    lhsAtomNums, rhsAtomNums, allAtomNums, kLhs, nPos,
    rFns, n, final, atomsD, lhsD, rhsD
},
    {lhsHead, lhsAtomSyms} = sideToAtoms[conjecture[[1]]];
    {rhsHead, rhsAtomSyms} = sideToAtoms[conjecture[[2]]];
    If[ lhsHead =!= rhsHead,
        (* Atomic axioms can't change function heads, so different
           heads = vacuously unprovable.  Encode as TNum[0] so the
           collapse picks no NUM(1) leaf. *)
        Return[TNum[0]]
    ];
    atomMap = collectAtoms[axioms, conjecture];
    lhsAtomNums = TNum[atomMap[#]] & /@ lhsAtomSyms;
    rhsAtomNums = TNum[atomMap[#]] & /@ rhsAtomSyms;
    allAtomNums = Join[lhsAtomNums, rhsAtomNums];
    kLhs = Length[lhsAtomNums];
    nPos = Length[allAtomNums];
    rFns = allRewriteRefs[axioms, atomMap];
    n = Length[rFns];
    final = Fold[stepFold[rFns, n, nPos],
        {allAtomNums, $supBase}, Range[depth]];
    atomsD = final[[1]];
    lhsD = atomsD[[1 ;; kLhs]];
    rhsD = atomsD[[kLhs + 1 ;; nPos]];
    allEqual[lhsD, rhsD]
]

(* ---------------------------------------------------------------- *)
(* Driver                                                            *)
(* ---------------------------------------------------------------- *)

icProvable[conjecture_, axioms_, depth_Integer] := Module[{
    term, leaves
},
    term = buildSearchTerm[conjecture, axioms, depth];
    leaves = FromTTerm /@ TCollapse[term];
    AnyTrue[leaves, # === 1 &]
]

(* ---------------------------------------------------------------- *)
(* Tests                                                             *)
(* ---------------------------------------------------------------- *)

(* Pair-form input: axioms = list of {old, new}, conjecture =
   {lhsExpr, rhsExpr}. *)
cases = {
    <|"Name" -> "f-substitutivity-1pos (depth=1)",
      "Conjecture" -> {f[a], f[b]},
      "Axioms" -> {{a, b}},
      "Depth" -> 1,
      "Expected" -> True|>,
    <|"Name" -> "f-binary-2pos (depth=2)",
      "Conjecture" -> {f[a, b], f[c, d]},
      "Axioms" -> {{a, c}, {b, d}},
      "Depth" -> 2,
      "Expected" -> True|>,
    <|"Name" -> "f-via-trans (depth=2)",
      "Conjecture" -> {f[a], f[c]},
      "Axioms" -> {{a, b}, {b, c}},
      "Depth" -> 2,
      "Expected" -> True|>,
    <|"Name" -> "head-mismatch (vacuous)",
      "Conjecture" -> {f[a], g[a]},
      "Axioms" -> {{a, b}},
      "Depth" -> 2,
      "Expected" -> False|>,
    <|"Name" -> "atom-only-trans-3 (depth=2, regression)",
      "Conjecture" -> {a, c},
      "Axioms" -> {{a, b}, {b, c}},
      "Depth" -> 2,
      "Expected" -> True|>,
    <|"Name" -> "atom-only-unprovable (depth=2, regression)",
      "Conjecture" -> {a, d},
      "Axioms" -> {{a, b}, {b, c}},
      "Depth" -> 2,
      "Expected" -> False|>
};

Print["=== IC-native substitutivity toy ==="];
results = Map[
    Function[case,
        Module[{got},
            got = icProvable[case["Conjecture"], case["Axioms"], case["Depth"]];
            Print[StringPadRight[case["Name"], 40],
                "  got=", got,
                "  expected=", case["Expected"],
                If[ got === case["Expected"], "  OK", "  FAIL"]];
            got === case["Expected"]
        ]
    ],
    cases
];
Print[""];
Print["Pass: ", Count[results, True], "/", Length[cases]];
