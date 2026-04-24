#!/usr/bin/env wolframscript
(* run.wls -- Run every *.wlt file in this directory under TestReport
   and report status to stdout.  Exits 1 if any test failed.

   Invoked from `make wl-test`.  Expects to be run from the repo root
   (the Makefile target cd's there before calling wolframscript).
*)

PacletDirectoryLoad["wl/THVMLink"];
Needs["THVMLink`"];

report  = TestReport[FileNames["*.wlt", "wl/THVMLink/Tests"]];
nPass   = report["TestsSucceededCount"];
nFail   = report["TestsFailedCount"];
failed  = Select[Values[report["TestResults"]], #["Outcome"] =!= "Success" &];

WriteString["stdout",
    "wl tests: ", ToString[nPass], " passed, ", ToString[nFail], " failed\n"];

Do[
    WriteString["stdout",
        "  FAIL  ", r["TestID"], "  (", r["Outcome"], ")\n",
        "         expected: ", ToString[r["ExpectedOutput"], InputForm], "\n",
        "         actual:   ", ToString[r["ActualOutput"],   InputForm], "\n"
    ],
    {r, failed}
];

Exit[If[nFail > 0, 1, 0]]
