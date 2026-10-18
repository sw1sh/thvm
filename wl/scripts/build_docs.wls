#!/usr/bin/env wolframscript
(* Builds the THVMLink documentation with PacletPage's BuildPacletDocumentation: every page under
   wl/THVMLink/docs/ becomes an authoring notebook under wl/THVMLink/Documentation/English, and
   docs/ResourceDefinition.md becomes wl/THVMLink/ResourceDefinition.nb, the definition the
   resource is deployed and submitted from. A page converts again only when its source, the
   paclet's code or the converter changed; a deploy builds the finished pages from these. The
   examples run against the library in wl/THVMLink/LibraryResources, so build it first
   (`make wl-cross`, or `make wl` for this platform only).

       wl -f wl/scripts/build_docs.wls               (or --no-cache: every example evaluated afresh)

   release.wls builds the same way before it publishes. *)

$ProgressReporting = False

Get[FileNameJoin[{DirectoryName[$InputFileName], "pacletpage.wl"}]]

result = buildDocumentation["UseCache" -> ! MemberQ[$ScriptCommandLine, "--no-cache"], "Monitor" -> True]
If[ ! AssociationQ[result], Exit[1]]
Print[Length[result["Notebooks"]], " pages", If[StringQ[result["Definition"]], " and the definition notebook", ""],
    ": ", Length[result["Converted"]], " converted, ", Length[result["Cached"]], " from the cache, ",
    Length[result["Evaluated"]], " with their examples evaluated afresh"]
If[ result["Failed"] =!= {}, Print["failed: ", result["Failed"]]; Exit[1]]
