(* use_realize.wlt - f1d-c: verify the inlined-kernel path
                      preserves gradient correctness on the
                      poly-regression scenario from nn.wlt.
                      Flips MATERIALIZE_USE_REALIZE_INFO on
                      for the duration of the test, restores
                      it after. *)

BeginPackage["THVMLinkTests`UseRealize`",
    {"THVMLink`", "MUnit`"}];

Begin["`Private`"];

(* === toggle-on path forward + grad gives same loss + grads as
       toggle-off (which today is the legacy default).  Mirrors
       nn/poly-regression-gradients but flipped on first. === *)

VerificationTest[
    TInit[];
    Module[{prev, a, b, x, t, pred, loss, lossR, daR, dbR},
        prev = TSetUseRealizeInfo[True];
        a = TTensorCreate @ NumericArray[{1.0}, "Real32"];
        b = TTensorCreate @ NumericArray[{1.0}, "Real32"];
        x = TTensorCreate @ NumericArray[{2.0}, "Real32"];
        t = TTensorCreate @ NumericArray[{10.0}, "Real32"];
        pred = TUOpAdd[TUOpMul[a, TSquare[x]], TUOpMul[b, x]];
        loss = TMSELoss[pred, t];
        lossR = Normal @ TTensorData @ TRealize[loss];
        daR   = Normal @ TTensorData @ TRealize[TGrad[loss, a]];
        dbR   = Normal @ TTensorData @ TRealize[TGrad[loss, b]];
        TSetUseRealizeInfo[prev];
        {lossR, daR, dbR}
    ],
    {{16.0}, {-32.0}, {-16.0}},
    TestID -> "use-realize/poly-regression-gradients"
]

(* === simple chain forward computes correctly with toggle on. === *)

VerificationTest[
    TInit[];
    Module[{prev, a, b, c, out},
        prev = TSetUseRealizeInfo[True];
        a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0, 4.0}, "Real32"];
        b = TTensorCreate @ NumericArray[{5.0, 6.0, 7.0, 8.0}, "Real32"];
        c = TTensorCreate @ NumericArray[{2.0, 2.0, 2.0, 2.0}, "Real32"];
        out = Normal @ TTensorData @ TRealize @ TUOpMul[TUOpAdd[a, b], c];
        TSetUseRealizeInfo[prev];
        out
    ],
    {12.0, 16.0, 20.0, 24.0},
    TestID -> "use-realize/elementwise-chain-correct-result"
]

(* === toggle-off path (default) is independently correct. === *)

VerificationTest[
    TInit[];
    Module[{a, b, c, out},
        a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0, 4.0}, "Real32"];
        b = TTensorCreate @ NumericArray[{5.0, 6.0, 7.0, 8.0}, "Real32"];
        c = TTensorCreate @ NumericArray[{2.0, 2.0, 2.0, 2.0}, "Real32"];
        out = Normal @ TTensorData @ TRealize @ TUOpMul[TUOpAdd[a, b], c];
        out
    ],
    {12.0, 16.0, 20.0, 24.0},
    TestID -> "use-realize/elementwise-chain-toggle-off-baseline"
]

End[];
EndPackage[];
