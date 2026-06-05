PacletDirectoryAdd[FileNameJoin[{Directory[], "wl"}]];
Needs["THVMLink`"];
TInitContext[];
(* Simple SAT solver: brute-force enumerate assignments, evaluate
   formula per assignment using TWnf reduction.  Demonstrates IC
   reduction on boolean formulas; benchmark baseline. *)

(* CNF formula representation in WL:
     {clause1, clause2, ...}
   where each clause is a list of literals {lit1, lit2, ...}.
   A literal is a positive integer i (variable x_i) or negative -i
   (negation NOT x_i).  Variables are 1-indexed. *)

(* Build a TTerm formula given a CNF and a variable assignment vec.
   vec is a List of 0/1 of length V.  Result reduces to NUM(0/1). *)
satEval[cnf_List, vec_List] := Block[{
    litVal, clauseVal, formula
},
    litVal[lit_] :=
        If[ lit > 0, TNum[ vec[[lit]] ], TNum[ 1 - vec[[-lit]] ] ];
    (* Clause = OR of literals.  OR(a, b) = 1 - (1-a)(1-b). *)
    clauseVal[lits_] := Fold[
        Function[{accum, lit}, TOp2["-", TNum[1],
            TOp2["*",
                TOp2["-", TNum[1], accum],
                TOp2["-", TNum[1], litVal[lit]]
            ]]],
        litVal[First[lits]],
        Rest[lits]
    ];
    (* Formula = AND of clauses.  AND(a, b) = a*b. *)
    formula = Fold[TOp2["*", #1, #2] &,
        clauseVal[First[cnf]],
        clauseVal /@ Rest[cnf]
    ];
    TTermVal @ TWnf[formula]
]

(* Enumerate all 2^V assignments and check each.  Returns list of
   sat assignments. *)
satSolveBrute[cnf_List, nVars_Integer] := Module[{
    allVecs = Tuples[{0, 1}, nVars]
},
    Select[allVecs, satEval[cnf, #] === 1 &]
]

(* Smoke test: (x1 OR x2) AND (NOT x1 OR x3) AND (NOT x2 OR NOT x3) *)
testCnf = {{1, 2}, {-1, 3}, {-2, -3}};
Print["Test formula: (x1 v x2) AND (~x1 v x3) AND (~x2 v ~x3)"];
Print["  satisfying assignments:"];
sols = satSolveBrute[testCnf, 3];
Do[Print["    x1=", a[[1]], " x2=", a[[2]], " x3=", a[[3]]], {a, sols}];
Print["  count:", Length[sols]];

(* Compare with WL's SatisfiableInstances *)
Print[""];
Print["WL ground truth via SatisfiableQ:"];
syms = Table[ToExpression["x" <> ToString[i]], {i, 3}];
formula = And[
    Or[syms[[1]], syms[[2]]],
    Or[Not[syms[[1]]], syms[[3]]],
    Or[Not[syms[[2]]], Not[syms[[3]]]]
];
ourSols = sols;
wlSols = SatisfiabilityInstances[formula, syms, 8];
wlVecs = Boole /@ wlSols;
Print["  WL solutions:", wlVecs];
Print["  Same set:", Sort[ourSols] === Sort[wlVecs]];
