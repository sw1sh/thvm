#!/usr/bin/env wolframscript
(* wl/scripts/publish_paclet.wls - pack wl/THVMLink into a .paclet and
   copy it to a public CloudObject so it installs with PacletInstall[url].

   Run from anywhere:

       wolframscript -f wl/scripts/publish_paclet.wls

   Assumes the per-platform libraries under
   wl/THVMLink/LibraryResources/<SystemID>/ have already been built
   (see `make wl-cross`).  Warns, but still publishes, if any of the
   five typical SystemIDs is missing - a partial paclet still installs
   on the platforms it does ship.

   The published CloudObject is read by the round-trip check in
   wl/scripts/verify_install.wls:  PacletInstall[url]; Needs["THVMLink`"]. *)

(* Repo-relative paths only; resolve against this script's location so
   the script works regardless of the caller's working directory. *)
repoRoot   = FileNameJoin[{DirectoryName[$InputFileName], "..", ".."}];
pacletDir  = FileNameJoin[{repoRoot, "wl", "THVMLink"}];
outDir     = FileNameJoin[{repoRoot, "build"}];

If[ ! DirectoryQ[pacletDir],
    Print["ERROR: paclet directory not found: ", pacletDir];
    Exit[1]
];

(* Report platform coverage.  These are the five typical Wolfram
   SystemIDs; the WL loader (Kernel/THVMLink.wl) keys the dylib path
   off $SystemID, so each must have its native library to load there. *)
wanted = {"MacOSX-ARM64", "MacOSX-x86-64", "Linux-x86-64", "Linux-ARM64", "Windows-x86-64"};
libExt = <|"MacOSX-ARM64" -> "dylib", "MacOSX-x86-64" -> "dylib",
           "Linux-x86-64" -> "so", "Linux-ARM64" -> "so",
           "Windows-x86-64" -> "dll"|>;
present = Select[wanted, FileExistsQ[FileNameJoin[{pacletDir, "LibraryResources", #, "THVMLink." <> libExt[#]}]] &];
missing = Complement[wanted, present];
Print["[publish] platform libraries present: ", Length[present], "/", Length[wanted]];
Scan[Print["  + ", #] &, present];
Scan[Print["  - MISSING ", #] &, missing];
If[ missing =!= {},
    Print["[publish] note: publishing a partial paclet (run `make wl-cross` to build all five)."]
];

(* Prune debug artifacts that build tools drop next to the libraries
   (.dSYM dirs from clang, .pdb/.lib/.exp from zig's Windows link) so the
   distributed paclet ships only the loadable binaries. *)
cruft = Join[
    FileNames["*.dSYM", FileNameJoin[{pacletDir, "LibraryResources"}], Infinity],
    FileNames[{"*.pdb", "*.lib", "*.exp"}, FileNameJoin[{pacletDir, "LibraryResources"}], Infinity]
];
If[ cruft =!= {},
    Print["[publish] pruning ", Length[cruft], " debug artifact(s)"];
    Scan[If[DirectoryQ[#], DeleteDirectory[#, DeleteContents -> True], DeleteFile[#]] &, cruft]
];

(* Pack.  CreatePacletArchive zips the directory per PacletInfo.wl and
   names the file THVMLink-<version>.paclet in outDir. *)
Print["[publish] packing ", pacletDir, " ..."];
pacletFile = CreatePacletArchive[pacletDir, outDir];
If[ ! StringQ[pacletFile] || ! FileExistsQ[pacletFile],
    Print["ERROR: CreatePacletArchive failed: ", ToString[pacletFile]];
    Exit[1]
];
Print["[publish] packed -> ", pacletFile, " (", FileByteCount[pacletFile], " bytes)"];

(* Ensure a cloud session.  wolframscript usually carries one; if not,
   CloudConnect[] reads $WolframID / prompts. *)
If[ ! TrueQ[$CloudConnected], CloudConnect[]];
If[ ! TrueQ[$CloudConnected],
    Print["ERROR: not connected to the Wolfram Cloud (CloudConnect failed)."];
    Exit[1]
];

(* Copy the packed file to a public CloudObject.  CopyFile uploads the
   raw bytes, so the object's URL serves the .paclet directly and
   PacletInstall[url] can fetch + install it. *)
target = CloudObject["THVMLink.paclet", Permissions -> "Public"];
co = CopyFile[pacletFile, target, OverwriteTarget -> True];
If[ Head[co] =!= CloudObject,
    Print["ERROR: CopyFile to CloudObject failed: ", ToString[co]];
    Exit[1]
];

url = First[co];
Print[""];
Print["[publish] published: ", url];
Print["[publish] install with:"];
Print["    PacletInstall[\"", url, "\"]"];
Print["    Needs[\"THVMLink`\"]"];
