#!/usr/bin/env wolframscript
(* findproof.wls -- FindProof: the CASC/UEQ entry point. Behaves like any CASC
   participant system: a TPTP problem in, an SZS status + TPTP CNFRefutation out.

       wolframscript -f findproof.wls <problem.p> [timeLimitSeconds]

   <problem.p> and the time limit are the %s and %d of the CASC run command.
   All the work -- parsing, proving, and rendering the proof as SZS+TPTP -- is
   done by TFindProof's "TPTP" return spec, so nothing about the problem is
   hardcoded and the same command runs on any unit-equational problem.

   The paclet is loaded FIRST: Get["WolframInstitute`THVMLink`"] changes
   $Context, so argument symbols must be bound after it to stay in one context.
*)

PacletDirectoryLoad[FileNameJoin[{FileNameDrop[DirectoryName[$InputFileName], -3], "wl", "THVMLink"}]];
Get["WolframInstitute`THVMLink`"];

argv = Rest[$ScriptCommandLine];
If[ argv === {},
    Print["usage: wolframscript -f findproof.wls <problem.p> [seconds]"]; Exit[2]
];
problemFile = argv[[1]];
timeLimit = If[Length[argv] >= 2, Quiet @ Check[ToExpression[argv[[2]]], 300.], 300.];

result = TFindProof[File[problemFile], "TPTP", TimeConstraint -> timeLimit];
If[ StringQ[result],
    Print[result],
    Print["% SZS status GaveUp for ", FileNameTake[problemFile]]
];
