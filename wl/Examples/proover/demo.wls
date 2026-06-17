#!/usr/bin/env wolframscript
(* demo.wls -- the TFindProof -> ProofObject -> validation workflow.

   Shows that a thvm proof carries TWO independent validators:

     1. INTERNAL  ProofObject["ProofFunction"][]  replays every rewrite step
                  in the Wolfram kernel and returns Success[...] / Failure[...].

     2. EXTERNAL  ProofToLean[po] (the Wolfram`LeanLink` paclet) transpiles the
                  ProofObject into a Lean 4 environment; LeanState[env][
                  "Complete"] re-derives the goal in Lean's trusted kernel.

   The external leg is skipped with a note if LeanLink is not installed.
   Run from anywhere:  wolframscript -f demo.wls
*)

scriptDir = DirectoryName[$InputFileName];
repoRoot = FileNameDrop[scriptDir, -3];   (* wl/Examples/proover -> repo root *)

PacletDirectoryLoad[FileNameJoin[{repoRoot, "wl", "THVMLink"}]];
Get["WolframInstitute`THVMLink`"];

haveLean = ! FailureQ @ Quiet @ Check[Needs["Wolfram`LeanLink`"], $Failed];

(* a few small equational theorems on thvm's own completion engine *)
problems = {
    {"a == c from {a==b, b==c}", HoldForm[TFindProof[a == c, {a == b, b == c}]]},
    {"LeftInverse / GroupAxioms", HoldForm[TFindProof["LeftInverse", "GroupAxioms"]]}
};

Do[
    Module[{label = p[[1]], po},
        Print["================================================================"];
        Print["THEOREM: ", label];
        po = ReleaseHold[p[[2]]];
        If[ Head[po] =!= ProofObject,
            Print["  TFindProof did not return a ProofObject; skipping."]; Continue[]
        ];
        Print["  ProofObject length: ", po["ProofLength"]];

        (* 1. internal validation *)
        Print["  [internal] ProofFunction[] = ", po["ProofFunction"][]];

        (* 2. external validation via LeanLink *)
        If[ haveLean,
            Module[{env, complete},
                env = Wolfram`LeanLink`ProofToLean[po];
                complete = Wolfram`LeanLink`LeanState[env["FinalGoal"]]["Complete"];
                Print["  [external] Lean constants: ", Keys[env]];
                Print["  [external] LeanState[FinalGoal][\"Complete\"] = ", complete]
            ],
            Print["  [external] Wolfram`LeanLink` not installed; skipping Lean re-check."]
        ]
    ],
    {p, problems}
];
Print["================================================================"];
Print["Done."];
