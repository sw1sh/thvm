#!/usr/bin/env wolframscript
(* szs.wls -- the ProoVer competition entry point: check ONE TSTP proof and
   emit its SZS verdict line. This is the single fixed command the organizers
   run per problem.

       wolframscript -f szs.wls <proof.p>
       => % SZS status Verified for <proof.p>      (or FailedVerified / NotVerified)
*)

Get[FileNameJoin[{DirectoryName[$InputFileName], "proover.wl"}]];

If[ Length[$ScriptCommandLine] < 2,
    Print["usage: wolframscript -f szs.wls <proof.p>"]; Exit[2]
];

file = $ScriptCommandLine[[2]];
res = ProoVer`ProoVerCheck[file];
Print["% SZS status ", res["SZS"], " for ", FileNameTake[file]];
