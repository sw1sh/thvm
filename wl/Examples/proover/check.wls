#!/usr/bin/env wolframscript
(* check.wls -- run the ProoVer checker over a corpus of TSTP proofs.

   Usage:
       wolframscript -f check.wls [corpusDir]

   corpusDir defaults to the bundled ./corpus.  Every *_proof.p under it is
   checked; the SZS verdict and the bad-step reason are printed, and the
   ProoVer score is tallied (correct/ vs incorrect/ subtrees are the gold
   labels, mirroring the competition's 50/50 split).
*)

scriptDir = DirectoryName[$InputFileName];
Get[FileNameJoin[{scriptDir, "proover.wl"}]];

corpusDir = If[Length[$ScriptCommandLine] >= 2,
    $ScriptCommandLine[[2]],
    FileNameJoin[{scriptDir, "corpus"}]
];

proofs = FileNames["*_proof.p", corpusDir, Infinity];
If[ proofs === {},
    Print["No *_proof.p files under ", corpusDir]; Exit[1]
];

(* gold label from the path: .../correct/... is valid, .../incorrect/... is buggy *)
goldOf[f_] := Which[
    StringContainsQ[f, "/correct/"], "valid",
    StringContainsQ[f, "/incorrect/"], "buggy",
    True, "unknown"
];

(* ProoVer scoring matrix *)
scoreOf[gold_, szs_] := Switch[{gold, szs},
    {"buggy", "FailedVerified"}, 2,
    {"valid", "Verified"}, 1,
    {_, "NotVerified"}, 0,
    {"valid", "FailedVerified"}, -1,
    {"buggy", "Verified"}, -10,
    _, 0
];

total = 0;
Print[StringPadRight["proof", 24], StringPadRight["gold", 8], StringPadRight["verdict", 16], "score  reason"];
Do[
    Module[{res = ProoVer`ProoVerCheck[f], gold = goldOf[f], sc},
        sc = scoreOf[gold, res["SZS"]];
        total += sc;
        Print[
            StringPadRight[FileNameTake[f], 24],
            StringPadRight[gold, 8],
            StringPadRight[res["SZS"], 16],
            StringPadRight[ToString[sc], 7],
            res["Reason"]
        ]
    ],
    {f, Sort[proofs]}
];
Print["---"];
Print["total score: ", total, "  over ", Length[proofs], " proofs"];
