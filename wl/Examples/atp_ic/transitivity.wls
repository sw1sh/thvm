#!/usr/bin/env wolframscript
(* atp_ic/transitivity.wls -- toy IC-native equational ATP.

   Goal: prove `a == c` from `{a == b, b == c}` by reducing a TDef
   whose normal form is a SUP-tree of NUM(0)/NUM(1) leaves; True
   iff any leaf is NUM(1).

   Encoding:
     - Each atom -> a NUM ID (a=1, b=2, c=3).
     - Conjecture -> initial pair (lhsAtom, rhsAtom).
     - Each (axiom k, direction d) -> a unary rewrite function
         r_k_d : NUM -> NUM
         r_k_d[v] = if v == oldVal then newVal else v
       where (oldVal, newVal) is the directed axiom edge.
     - One step picks a rewrite function (SUP^L over all r_k_d)
       and a side (SUP^L over NUM(0)=L, NUM(1)=R) and applies
       the function to the chosen side.
     - D steps stacked.  Final check: `lhsD == rhsD` -> NUM(1)/0.
     - Reduce + collapse + AnyTrue[#, # === 1 &] = is conjecture
       provable in <= D steps?

   Limitation (toy): atoms-only.  Substitutivity / pattern axioms
   come in milestone 2+ per docs/plans/atp_ic_native.md. *)

PacletDirectoryAdd[FileNameJoin[{Directory[], "wl"}]];
Needs["THVMLink`"];
TInitContext[];

print[a___] := WriteString["stdout", StringRiffle[ToString /@ {a}, " "], "\n"];

(* ---------------------------------------------------------------- *)
(* Encoder                                                          *)
(* ---------------------------------------------------------------- *)

(* Inputs: axioms + conjecture as {lhs, rhs} pairs (atoms as
   bare symbols).  Pair form avoids `==` auto-evaluating
   tautologies like `a == a` away.

   Collect all atoms, assign each a positive Int ID. *)
collectAtoms[axioms_, conjecture_] := Module[{atoms},
    atoms = DeleteDuplicates @ Flatten @ Append[axioms, conjecture];
    AssociationThread[atoms -> Range[Length[atoms]]]
]

(* Register one (axiom, direction) rewrite function as a TDef so
   each substitution site (each SUP-leaf, each step) APP-REFs a
   fresh copy.  Inline TLams shared across SUP branches blow up
   under DUP-SUP commute. *)
defineRewriteFn[name_String, old_Integer, new_Integer] := With[{
    v = Module[{vs}, vs]
},
    TDef[name, TLam[v, TIfZero[TOp2["==", v, TNum[old]],
                               v,
                               TNum[new]]]]
]

(* For each axiom, register fwd + bwd as TDefs and return the list
   of TRef["axN_fwd"], TRef["axN_bwd"]. *)
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

(* SUP^baseLab over a flat list of values (right-associated):
     sup[ts] = TSup[L1, t1, TSup[L2, t2, TSup[L3, t3, t4]]]
   Each nested SUP gets baseLab + offset so DUP-SUP labels are
   distinct.  Single-element list returns the element unwrapped. *)
nestedSup[baseLab_Integer, ts_List] := Module[{n = Length[ts]},
    Which[
        n === 1, ts[[1]],
        n === 2, TSup[baseLab, ts[[1]], ts[[2]]],
        True, TSup[baseLab, ts[[1]],
                   nestedSup[baseLab + 1, Rest[ts]]]
    ]
]

(* Both-sides rewriting.  Each step picks one side AND one
   rewrite; the chosen side gets the rewrite, the other side
   passes through.  Now safe to use TOp2[==, SUP, SUP] at the
   final check thanks to the OP2-SUP / OP2-NUM-SUP runtime
   interactions.

   Note: TOp2["=="] returns 1 if equal, 0 otherwise.
   TIfZero[n, A, B] returns A when n=0 (sides differ -> apply
   the rewrite), B when n!=0 (sides match the choice ->
   passthrough).  Operands swapped (sideChoice on left of the
   ==) so the SUP is left-of-OP2 even though OP2-NUM-SUP also
   handles the right-of-OP2 case now. *)
sideUpdate[v_, mySideNum_, sideChoice_, rChoice_] :=
    (* TOp2["=="] returns 1 if equal else 0.
       TIfZero[n, A, B] returns A if n=0 else B.
       Sides differ (n=0) -> A = passthrough.
       Sides match (n=1) -> B = apply rewrite. *)
    TIfZero[TOp2["==", sideChoice, mySideNum],
            v,
            TApp[rChoice, v]]

(* Build the depth-D search Term.

   Step shape (both sides):
     lhs_{i+1} = sideUpdate[lhs_i, NUM(0), sideSup_i, rSup_i]
     rhs_{i+1} = sideUpdate[rhs_i, NUM(1), sideSup_i, rSup_i]

   Final:
     TOp2["==", lhs_D, rhs_D]   (yields NUM(1) iff equal)

   rSup + sideSup share the same cell across the lhs and rhs
   sideUpdates (same labels per step) so DUP-SUP annihilation
   correlates the choice across sides ("one rewrite event per
   step").  Each step uses fresh labels so DUPs across steps
   commute (independent choices per step) instead of annihilating. *)
$supBase = 100;

stepFold[rFns_, n_][acc_, stepIdx_] := Module[{
    lhs = acc[[1]], rhs = acc[[2]], labCur = acc[[3]],
    rSup, sideSup
},
    rSup = nestedSup[labCur, rFns];
    sideSup = TSup[labCur + n, TNum[0], TNum[1]];
    {
        sideUpdate[lhs, TNum[0], sideSup, rSup],
        sideUpdate[rhs, TNum[1], sideSup, rSup],
        labCur + n + 1
    }
]

buildSearchTerm[conjecture_, axioms_, depth_Integer] := Module[{
    atomMap, lhs0, rhs0, rFns, n, final, lhsD, rhsD
},
    atomMap = collectAtoms[axioms, conjecture];
    lhs0 = TNum[atomMap[conjecture[[1]]]];
    rhs0 = TNum[atomMap[conjecture[[2]]]];
    rFns = allRewriteRefs[axioms, atomMap];
    n = Length[rFns];
    final = Fold[stepFold[rFns, n],
        {lhs0, rhs0, $supBase}, Range[depth]];
    lhsD = final[[1]];
    rhsD = final[[2]];
    TOp2["==", lhsD, rhsD]
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

(* Pair-form input: axioms = list of {lhs, rhs}, conjecture = {lhs, rhs}. *)
cases = {
    <|"Name" -> "transitivity-3 (depth=2)",
      "Conjecture" -> {a, c},
      "Axioms" -> {{a, b}, {b, c}},
      "Depth" -> 2,
      "Expected" -> True|>,
    <|"Name" -> "trivial reflexivity (depth=0)",
      "Conjecture" -> {a, a},
      "Axioms" -> {},
      "Depth" -> 0,
      "Expected" -> True|>,
    <|"Name" -> "symmetry (depth=1)",
      "Conjecture" -> {b, a},
      "Axioms" -> {{a, b}},
      "Depth" -> 1,
      "Expected" -> True|>,
    <|"Name" -> "unprovable (depth=2)",
      "Conjecture" -> {a, d},
      "Axioms" -> {{a, b}, {b, c}},
      "Depth" -> 2,
      "Expected" -> False|>,
    (* Tests both-sides rewriting: closure {a==b, c==d} doesn't
       give a==d via LHS-only.  Need to rewrite both sides:
       a -> b on LHS (no, no proof either way); need
       a==a (depth 0)?  Pick: prove c==a from {a==b, b==c}.
       Path: rewrite RHS: a -> b -> c, gives c==c.  Or LHS:
       c -> b -> a, gives a==a.  Either side works.  At
       depth=2 with both sides we get more paths than LHS-only. *)
    <|"Name" -> "rhs-only-path (depth=2)",
      "Conjecture" -> {c, a},
      "Axioms" -> {{a, b}, {b, c}},
      "Depth" -> 2,
      "Expected" -> True|>
};

print["=== IC-native ATP toy ==="];
results = Map[
    Function[case,
        Module[{got},
            got = icProvable[case["Conjecture"], case["Axioms"], case["Depth"]];
            print[StringPadRight[case["Name"], 35],
                "  got=", got,
                "  expected=", case["Expected"],
                If[ got === case["Expected"], "  OK", "  FAIL"]];
            got === case["Expected"]
        ]
    ],
    cases
];
print[""];
print["Pass: ", Count[results, True], "/", Length[cases]];
