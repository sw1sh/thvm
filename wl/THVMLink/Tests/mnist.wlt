(* mnist.wlt -- TMnistLoad smoke tests.  Loads a small slice (10
   samples) so the test runs in <2s even on a cold ResourceData
   cache. *)

VerificationTest[
    TInit[];
    data = TMnistLoad[10];
    {Sort @ Keys[data],
     TTensorShape[data["trainImages"]],
     TTensorShape[data["trainLabels"]],
     TTensorShape[data["testImages"]],
     TTensorShape[data["testLabels"]]},
    {{"testImages", "testLabels", "trainImages", "trainLabels"},
     {10, 1, 28, 28}, {10}, {10, 1, 28, 28}, {10}},
    TestID -> "mnist/load-10-samples-shapes"
]

VerificationTest[
    TInit[];
    data = TMnistLoad[3];
    (* Pixel values are normalised to [0, 1] by ImageData "Real32". *)
    With[{vals = Flatten @ Normal @ TTensorData @ data["trainImages"]},
        {Min[vals] >= 0, Max[vals] <= 1}
    ],
    {True, True},
    TestID -> "mnist/pixel-range-0-to-1"
]

VerificationTest[
    TInit[];
    data = TMnistLoad[3];
    (* All labels are valid digit classes (0..9). *)
    With[{labs = Normal @ TTensorData @ data["trainLabels"]},
        And @@ ((0 <= # <= 9) & /@ labs)
    ],
    True,
    TestID -> "mnist/labels-valid-digit-class"
]
