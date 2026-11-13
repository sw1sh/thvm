#!/usr/bin/env wolframscript
(* Releases THVMLink with PacletPage's PublishPacletPage, in one kernel and so one cloud account:
   builds the documentation as build_docs.wls does, deploys the paclet resource, publishes the
   documentation site around it with the authoring notebooks as its sources, and checks the site.
   Build the libraries first (`make wl-cross`), so the archive ships one per platform.

       wl -f ~/.claude/skills/wolfram-cloud-account/run-as.wls wolframinstitute wl/scripts/release.wls
       (or --dry-run: the build and the resource, not deployed)

   The account is PACLETPAGE_ACCOUNT, by default the Wolfram Institute's. submit.wls submits the
   paclet to the Paclet Repository. *)

$ProgressReporting = False

Get[FileNameJoin[{DirectoryName[$InputFileName], "pacletpage.wl"}]]

account = Replace[Environment["PACLETPAGE_ACCOUNT"], Except[_String?(StringLength[#] > 0 &)] -> "wolframinstitute@wolframinstitute.org"]
dryRun = MemberQ[$ScriptCommandLine, "--dry-run"]

(* the documentation first, in two parts (pacletpage.wl), so the publish step only deploys it *)
build = buildDocumentation["Monitor" -> True]
If[ ! AssociationQ[build] || build["Failed"] =!= {}, Print["documentation failed: ", If[AssociationQ[build], build["Failed"], build]]; Exit[1]]
Print["documentation: ", Length[build["Notebooks"]], " pages"]

result = PublishPacletPage[$pacletDirectory, "THVMLink",
    "Account" -> account,
    "DryRun" -> dryRun,
    "Build" -> False,
    "Monitor" -> True,
    "ResourceOptions" -> $resourceOptions,
    "SiteOptions" -> {"VersionLabel" -> True, "FeedbackOptions" -> {"Domains" -> {"wolframinstitute.org", "wolfram.com"}}}
]
If[ ! AssociationQ[result], Exit[1]]
Print["resource: ", result["Resource"]["Resource"], If[TrueQ[result["Resource"]["Deployed"]], "", " (dry run)"]]
If[ AssociationQ[result["Site"]], Print["site: ", result["Site"]["Site"]]]
If[ AssociationQ[result["Check"]],
    Scan[Print["check failed: ", #["Check"], ": ", #["Detail"]] &, Select[Normal[result["Check"]["Checks"]], ! TrueQ[#["OK"]] &]];
    Print["check: ", If[TrueQ[result["Check"]["OK"]], "everything is reachable", "problems found"]];
    Exit[If[TrueQ[result["Check"]["OK"]], 0, 1]]
]
