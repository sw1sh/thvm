#!/usr/bin/env wolframscript
(* run.wls -- Run every *.wlt file in this directory under TestReport
   and report status to stdout.  Exits 1 if any regular test failed.

   Invoked from `make wl-test`.  Expects to be run from the repo root
   (the Makefile target cd's there before calling wolframscript).

   Files named `pending_*.wlt` are run as INFORMATIONAL only: their
   passes and failures are reported, but failures do not flip the
   overall exit code.  Use them to stage tests that document
   behaviour expected from a future commit.
*)

PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];   (* Get not Needs: avoids Global`TInit shadow *)

(* Library tests live under wl/THVMLink/Tests; example-helper
   tests (wl/Examples/_lib/Tests) ride along the same runner so
   they're exercised by `make wl-test`.  Each example test Get's
   its own helper at the top of the file. *)
allFiles     = Join[
    FileNames["*.wlt", "wl/THVMLink/Tests"],
    FileNames["*.wlt", "wl/Examples/_lib/Tests"]
];
pendingFiles = Select[allFiles, StringStartsQ[FileBaseName[#], "pending_"] &];
regularFiles = Complement[allFiles, pendingFiles];

runReport[files_List] := If[ files === {}, Null, TestReport[files]]

reg = runReport[regularFiles];
pen = runReport[pendingFiles];

regPass = If[reg === Null, 0, reg["TestsSucceededCount"]];
regFail = If[reg === Null, 0, reg["TestsFailedCount"]];
penPass = If[pen === Null, 0, pen["TestsSucceededCount"]];
penFail = If[pen === Null, 0, pen["TestsFailedCount"]];

WriteString["stdout",
    "wl tests: ", ToString[regPass], " passed, ",
                  ToString[regFail], " failed\n"];

Do[
    WriteString["stdout",
        "  FAIL  ", r["TestID"], "  (", r["Outcome"], ")\n",
        "         expected: ", ToString[r["ExpectedOutput"], InputForm], "\n",
        "         actual:   ", ToString[r["ActualOutput"],   InputForm], "\n"
    ],
    {r, If[reg === Null, {}, Select[Values[reg["TestResults"]], #["Outcome"] =!= "Success" &]]}
];

If[ pen =!= Null,
    WriteString["stdout",
        "wl pending tests: ", ToString[penPass], " passed, ",
                              ToString[penFail], " failed (informational)\n"];
    Do[
        WriteString["stdout",
            "  pend  ", r["TestID"], "  (", r["Outcome"], ")\n"
        ],
        {r, Select[Values[pen["TestResults"]], #["Outcome"] =!= "Success" &]}
    ]
];

Exit[If[regFail > 0, 1, 0]]
