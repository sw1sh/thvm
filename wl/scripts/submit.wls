#!/usr/bin/env wolframscript
(* Submits THVMLink to the Wolfram Paclet Repository with PacletPage's SubmitPacletResource: the
   definition notebook's own check, a fresh PacletBuild checked to hold every documentation page
   and to load in a fresh kernel, then the submission under the version in PacletInfo.wl, which
   has to be newer than the repository's. The account must own the publisher, WolframInstitute:
   PACLETPAGE_ACCOUNT, by default the Wolfram Institute's. Build the documentation first, with
   build_docs.wls or release.wls, and the libraries with `make wl-cross`.

       wl -f ~/.claude/skills/wolfram-cloud-account/run-as.wls wolframinstitute wl/scripts/submit.wls
       (or --dry-run: everything but the submission) *)

$ProgressReporting = False

Get[FileNameJoin[{DirectoryName[$InputFileName], "pacletpage.wl"}]]

account = Replace[Environment["PACLETPAGE_ACCOUNT"], Except[_String?(StringLength[#] > 0 &)] -> "wolframinstitute@wolframinstitute.org"]
result = SubmitPacletResource[$pacletDirectory,
    "Account" -> account,
    "DryRun" -> MemberQ[$ScriptCommandLine, "--dry-run"],
    Sequence @@ $resourceOptions
]
If[ ! AssociationQ[result], Exit[1]]
Print[result["Name"], " ", result["Version"], ": ", If[TrueQ[result["Submitted"]], "submitted", "not submitted (dry run)"]]
Print["archive: ", result["Archive"], ", ", result["Pages"], " documentation pages"]
If[ KeyExistsQ[result, "Result"], Print["result: ", result["Result"]]]
