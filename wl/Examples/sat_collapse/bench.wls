PacletDirectoryLoad[FileNameJoin[{Directory[], "wl"}]];
Needs["THVMLink`"];
TInitContext[];
print[a___] := WriteString["stdout", StringRiffle[ToString /@ {a}, " "], "\n"];

satEval[cnf_List, vec_List] := Block[{litVal, clauseVal, formula},
    litVal[lit_] := If[ lit > 0, TNum[ vec[[lit]] ], TNum[ 1 - vec[[-lit]] ] ];
    clauseVal[lits_] := Fold[
        TOp2["-", TNum[1],
            TOp2["*",
                TOp2["-", TNum[1], #1],
                TOp2["-", TNum[1], litVal[#2]]]] &,
        litVal[First[lits]],
        Rest[lits]
    ];
    formula = Fold[TOp2["*", #1, #2] &,
        clauseVal[First[cnf]],
        clauseVal /@ Rest[cnf]
    ];
    TTermVal @ TWnf[formula]
]

satSolveBrute[cnf_List, nVars_Integer] := Select[
    Tuples[{0, 1}, nVars],
    satEval[cnf, #] === 1 &
]

(* Generate a random 3-CNF instance *)
SeedRandom[42];
randomCnf[nVars_, nClauses_] := Table[
    Module[{vars = RandomSample[Range[nVars], 3]},
        vars * RandomChoice[{-1, 1}, 3]
    ],
    {nClauses}
]

(* Convert CNF to WL boolean formula for SatisfiabilityInstances *)
cnfToWL[cnf_List, syms_List] := And @@ (Or @@ # & /@
    Map[If[# > 0, syms[[#]], Not[syms[[-#]]]] &, cnf, {2}])

(* Bench *)
Do[Module[{nv = sz, nc = Round[sz * 3.5], cnf, syms, ourSols, wlSols,
            tThvm, tWl},
    cnf = randomCnf[nv, nc];
    syms = Table[Symbol["x" <> ToString[i]], {i, nv}];
    
    (* Time thvm brute *)
    tThvm = First @ AbsoluteTiming[ourSols = satSolveBrute[cnf, nv]];
    
    (* Time WL SatisfiabilityInstances *)
    tWl = First @ AbsoluteTiming[
        wlSols = SatisfiabilityInstances[cnfToWL[cnf, syms], syms, 2^nv]
    ];
    
    print["V=", nv, " C=", nc,
        " | thvm:", NumberForm[tThvm * 1000., {6, 2}], "ms",
        " | WL: ", NumberForm[tWl * 1000., {6, 2}], "ms",
        " | thvm sols:", Length[ourSols], " WL sols:", Length[wlSols],
        " | ratio:", NumberForm[tThvm/tWl, {4, 2}]]
], {sz, {4, 6, 8, 10, 12}}]
