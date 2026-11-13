#!/usr/bin/env wolframscript
(* Republishes the THVMLink documentation site around the paclet resource already deployed, with
   the current PacletPage: the site's shell, its version label and its sources, not the paclet or
   its pages.  The paclet directory is the one whose built notebooks that release published
   (by default this checkout's), since the site's sources are its authoring notebooks.

       wl -f ~/.claude/skills/wolfram-cloud-account/run-as.wls wolframinstitute wl/scripts/refresh_site.wls [dir] [--dry-run]

   --dry-run writes the site's files nowhere and prints its header, to see the version label. *)

$ProgressReporting = False

Get[FileNameJoin[{DirectoryName[$InputFileName], "pacletpage.wl"}]]

account = Replace[Environment["PACLETPAGE_ACCOUNT"], Except[_String?(StringLength[#] > 0 &)] -> "wolframinstitute@wolframinstitute.org"]
dryRun = MemberQ[$ScriptCommandLine, "--dry-run"]
dir = Replace[Select[Rest[$ScriptCommandLine], DirectoryQ], {{d_, ___} :> ExpandFileName[d], _ -> $pacletDirectory}]
resource = "https://www.wolframcloud.com/obj/wolframinstitute/DeployedResources/Paclet/WolframInstitute/THVMLink"
siteOptions = {"VersionLabel" -> True, "FeedbackOptions" -> {"Domains" -> {"wolframinstitute.org", "wolfram.com"}}}

catalog = PacletPageCatalog[resource, "Documentation" -> dir, "Monitor" -> True]
If[ ! AssociationQ[catalog], Print["no catalog"]; Exit[1]]
Print["catalog: ", Length[catalog["Pages"]], " pages, version ", Lookup[catalog, "Version", None]]

If[ dryRun,
    files = PacletPageFiles[catalog, "VersionLabel" -> True];
    If[ ! AssociationQ[files], Exit[1]];
    Print["header: ", StringCases[files["index.html"], "<span class=\"name\">" ~~ Shortest[___] ~~ "</span></span>"]];
    Exit[0]]

site = DeployPacletPage[catalog, "THVMLink", "Account" -> account, Sequence @@ siteOptions, "Source" -> dir, "Monitor" -> True]
If[ ! AssociationQ[site], Exit[1]]
Print["site: ", site["Site"]]
check = CheckPacletPage[catalog, "THVMLink", "Pages" -> All, "Wait" -> 120]
If[ AssociationQ[check],
    Scan[Print["check failed: ", #["Check"], ": ", #["Detail"]] &, Select[Normal[check["Checks"]], ! TrueQ[#["OK"]] &]];
    Print["check: ", If[TrueQ[check["OK"]], "everything is reachable", "problems found"]];
    Exit[If[TrueQ[check["OK"]], 0, 1]]]
