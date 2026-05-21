#!/usr/bin/env wolframscript
(* tools/verify_install.wls - prove the published paclet installs and
   loads from its URL in a clean kernel, the way an end user would:

       wolframscript -f tools/verify_install.wls <paclet-url-or-file>

   With no argument it reads the URL of the public CloudObject named
   "THVMLink.paclet" (what tools/publish_paclet.wls writes).

   This deliberately does NOT add the repo's wl/ directory to $Path:
   it exercises the shipped library under LibraryResources/$SystemID,
   so it is a real test of the cross-compiled binary for THIS platform. *)

print[args___] := WriteString["stdout", StringJoin[ToString /@ {args}], "\n"];

src = If[ Length[$ScriptCommandLine] >= 2,
    Last[$ScriptCommandLine],
    First[CloudObject["THVMLink.paclet"]]
];
print["[verify] installing from: ", src];

(* Remove any already-installed copy so we test the fetched artifact,
   not a stale local install. *)
Quiet @ PacletUninstall["THVMLink"];

pac = PacletInstall[src, ForceVersionInstall -> True];
If[ Head[pac] =!= PacletObject,
    print["FAIL: PacletInstall returned ", ToString[pac]];
    Exit[1]
];
print["[verify] installed: ", pac["Name"], " ", pac["Version"], " at ", pac["Location"]];

Needs["THVMLink`"];
print["[verify] Needs[\"THVMLink`\"] ok"];

(* Smoke test: drive the native library through one reduction so we
   know the platform dylib actually loaded and runs. *)
TInit[];
id   = TApp[TLam[x, x], TLam[y, y]];   (* (\x.x)(\y.y) -> \y.y *)
wnf  = TWnf[id];
print["[verify] TWnf head tag = ", TTagName[TTermTag[wnf]]];
If[ ! IntegerQ[TTermTag[wnf]],
    print["FAIL: reduction did not return a term"];
    Exit[1]
];

print["[verify] PASS: PacletInstall + Needs + native call all work."];
