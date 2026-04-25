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

(* === TMnistBatch -- random minibatch sampler === *)

VerificationTest[
    TInit[];
    batch = TMnistBatch[8];
    {Sort @ Keys[batch],
     TTensorShape[batch["images"]],
     TTensorShape[batch["labels"]]},
    {{"images", "labels"}, {8, 1, 28, 28}, {8}},
    TestID -> "mnist/batch-train-shapes"
]

VerificationTest[
    TInit[];
    batch = TMnistBatch[5, "test"];
    TTensorShape[batch["images"]],
    {5, 1, 28, 28},
    TestID -> "mnist/batch-test-shape"
]

(* Two batches drawn back-to-back should differ (random sampling).
   Use seed-free comparison: any difference in labels suffices. *)
VerificationTest[
    TInit[];
    SeedRandom[]; b1 = TMnistBatch[16];
    SeedRandom[]; b2 = TMnistBatch[16];
    Normal @ TTensorData @ b1["labels"] =!= Normal @ TTensorData @ b2["labels"],
    True,
    TestID -> "mnist/batch-randomness-different-draws"
]

VerificationTest[
    TInit[];
    TMnistBatch[3, "bogus"],
    $Failed,
    {TMnistBatch::badsplit},
    TestID -> "mnist/batch-bad-split-fails"
]
