(* reclaim.wlt - TReclaim: the buffers of the central processing unit backend that no held term, JIT
                 capture or definition reaches are freed, and nothing else is. *)

BeginPackage["THVMLinkTests`Reclaim`",
    {"WolframInstitute`THVMLink`", "MUnit`"}];

Begin["`Private`"];

(* === the buffer of a dropped result is freed, a held one is kept === *)

VerificationTest[
    TInit[];
    TReset[];
    Module[{keep, before, freed},
        keep = TRealize[TTensorCreate[NumericArray[ConstantArray[1., 1000], "Real32"]] + 1.];
        Module[{drop = TRealize[TTensorCreate[NumericArray[ConstantArray[1., 100000], "Real32"]] * 2.]}, Null];
        before = TTotalBufBytes[];
        freed = TReclaim[];
        {freed >= 400000, before - TTotalBufBytes[] == freed, Normal[TTensorData[keep]][[;; 2]]}
    ],
    {True, True, {2., 2.}},
    TestID -> "reclaim/frees-dropped-keeps-held"
]

(* === a TJit capture keeps the buffers it replays into === *)

VerificationTest[
    TInit[];
    TReset[];
    Module[{x = TTensorCreate[ConstantArray[1., 50000]], double, freed},
        double = TJit[TRealize[# * 2.] &];
        Module[{y = double[x]}, Null];
        freed = TReclaim[];
        {freed, Take[Normal[double[x]], 2]}
    ],
    {0, {2., 2.}},
    TestID -> "reclaim/a-capture-keeps-its-buffers"
]

End[];
EndPackage[];
