#!/usr/bin/env wolframscript
(* grad-profile.wls -- snapshot heap / kernel state across nested
   TGrad rounds.  Uses TProfile + TProfileReport from THVMLink.
   Run as:  wolframscript -f wl/Examples/grad-profile.wls
*)

PacletDirectoryLoad["wl/THVMLink"]
Get["WolframInstitute`THVMLink`"]

(* a^4 nested TGrad, vector input, depths 1-5. *)
Module[{a, y, seed, profiles},
    profiles = Table[
        TInit[];
        a = TTensorCreate[NumericArray[{2.0, 3.0}, "Real32"]];
        y = TUOpMul[TUOpMul[TUOpMul[a, a], a], a];
        seed = TUOpExpand[TUOpConst[1.0, "f32"], {2}];
        TRealize[ Nest[ TGrad[#, a, seed] &, y, k]];
        TProfile["a^4 round=" <> ToString[k]],
        {k, 1, 5}];
    WriteString["stdout", "\n=== a^4 vector higher-order ===\n"];
    WriteString["stdout", TProfileReport[profiles] <> "\n"]
]

(* a^6 scalar nested TGrad, depths 1-6. *)
Module[{a, y, profiles},
    profiles = Table[
        TInit[];
        a = TTensorCreate[NumericArray[{1.5}, "Real32"]];
        y = TUOpMul[TUOpMul[TUOpMul[TUOpMul[TUOpMul[a, a], a], a], a], a];
        TRealize[ Nest[ TGrad[#, a] &, y, k]];
        TProfile["a^6 round=" <> ToString[k]],
        {k, 1, 6}];
    WriteString["stdout", "\n=== a^6 scalar higher-order ===\n"];
    WriteString["stdout", TProfileReport[profiles] <> "\n"]
]
