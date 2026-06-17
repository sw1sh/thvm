#!/usr/bin/env wolframscript
(* build.wls -- build the current-platform THVMLink native library, pack
   the paclet, and install it locally so `Needs["WolframInstitute`THVMLink`"]` loads THIS
   build (no PacletDirectoryLoad, and no stale repository copy shadowing
   it).  Run from anywhere:

       wolframscript -f wl/scripts/build.wls

   Set BUILD_NATIVE=0 to skip the `make wl` step when the dylib is
   already current (just re-pack + re-install).  For the documentation
   build see build_docs.wls; to publish to the cloud see
   publish_paclet.wls. *)

repoRoot  = FileNameJoin[{DirectoryName[$InputFileName], "..", ".."}];
pacletDir = FileNameJoin[{DirectoryName[$InputFileName], "..", "THVMLink"}];

If[ ! DirectoryQ[pacletDir],
    Print["ERROR: paclet directory not found: ", pacletDir];
    Exit[1]
];

(* 1. Build the native library for this platform.  `make` defaults to the
   WL paclet dylib; we name `wl` explicitly so the script is robust to the
   default goal.  Skip with BUILD_NATIVE=0. *)
If[ Environment["BUILD_NATIVE"] =!= "0",
    Print["[build] make wl  (", repoRoot, ")"];
    proc = RunProcess[{"make", "wl"}, ProcessDirectory -> repoRoot];
    Print[StringTrim[proc["StandardOutput"]]];
    If[ proc["ExitCode"] =!= 0,
        Print[proc["StandardError"]];
        Print["ERROR: `make wl` failed (exit ", proc["ExitCode"], ")"];
        Exit[1]
    ]
];

(* 2. Confirm this platform's library is present before packing. *)
libExt = <|"MacOSX-ARM64" -> "dylib", "MacOSX-x86-64" -> "dylib",
           "Linux-x86-64" -> "so", "Linux-ARM64" -> "so",
           "Windows-x86-64" -> "dll"|>;
lib = FileNameJoin[{pacletDir, "LibraryResources", $SystemID,
                    "THVMLink." <> Lookup[libExt, $SystemID, "so"]}];
If[ ! FileExistsQ[lib],
    Print["ERROR: no native library for ", $SystemID, ": ", lib];
    Print["       build it first: cd ", repoRoot, " && make wl"];
    Exit[1]
];
Print["[build] library: ", lib, " (", FileByteCount[lib], " bytes)"];

(* 3. Prune debug artifacts (clang .dSYM) so the archive ships only the
   loadable binaries. *)
cruft = FileNames["*.dSYM", FileNameJoin[{pacletDir, "LibraryResources"}], Infinity];
If[ cruft =!= {},
    Print["[build] pruning ", Length[cruft], " debug artifact(s)"];
    Scan[DeleteDirectory[#, DeleteContents -> True] &, cruft]
];

(* 4. Pack.  CreatePacletArchive zips the directory per PacletInfo.wl and
   names the file <Publisher>__<Name>-<version>.paclet in outDir. *)
outDir = FileNameJoin[{repoRoot, "build"}];
If[ ! DirectoryQ[outDir], CreateDirectory[outDir]];
Print["[build] packing ", pacletDir, " ..."];
pacletFile = CreatePacletArchive[pacletDir, outDir];
If[ ! StringQ[pacletFile] || ! FileExistsQ[pacletFile],
    Print["ERROR: CreatePacletArchive failed: ", ToString[pacletFile]];
    Exit[1]
];
Print["[build] packed -> ", pacletFile, " (", FileByteCount[pacletFile], " bytes)"];

(* 5. Install locally, replacing any previous install so `Needs` picks up
   THIS build.  ForceVersionInstall lets a rebuild at the same version
   overwrite the installed copy. *)
Quiet @ PacletUninstall["WolframInstitute/THVMLink"];
pac = PacletInstall[pacletFile, ForceVersionInstall -> True];
If[ Head[pac] =!= PacletObject,
    Print["ERROR: PacletInstall failed: ", ToString[pac]];
    Exit[1]
];
Print["[build] installed: ", pac["Name"], " ", pac["Version"]];
Print["[build] location:  ", pac["Location"]];
Print["[build] use it:    Needs[\"WolframInstitute`THVMLink`\"]"];
