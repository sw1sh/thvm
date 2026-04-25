#!/usr/bin/env wolframscript
(* forward.wls -- LeNet forward inference on a few MNIST samples.

   No training: backprop through Conv2D / Pool / RESHAPE / EXP2 /
   RECIP isn't implemented yet (see docs/grad-roadmap.md when it
   lands).  Predictions come from a NetInitialize'd random-weight
   LeNet so accuracy is ~chance (~10%).

   Run from the repo root:

       wolframscript -f wl/Examples/lenet-mnist/forward.wls
       THVM_BACKEND=metal wolframscript -f wl/Examples/lenet-mnist/forward.wls

   The Metal path looks for build/default.metallib relative to
   cwd; running from elsewhere needs THVM_METAL_METALLIB= override
   at dylib build time.
*)

PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];

debugPrint[args___] := WriteString["stdout", StringJoin @@ Map[ToString, {args}], "\n"]

TInit[];
(* NetModel["LeNet"] returns the canonical LeCun architecture
   with un-initialized weights (the local Mathematica paclet
   version mismatch surfaces them as `Automatic` symbol-shapes).
   NetInitialize fills them with random NumericArrays so
   TFromNet has concrete tensors to wrap. *)
net = NetInitialize @ NetModel["LeNet"];
debugPrint["LeNet built (random init): ", Length[net], " layers"];

batch = TMnistBatch[5];
debugPrint["loaded ", TTensorShape[batch["images"]][[1]], " MNIST samples"];

(* Pull host-side copies once; iterate per-sample. *)
imgs   = Normal @ TTensorData @ batch["images"];   (* {5, 1, 28, 28} *)
labels = Normal @ TTensorData @ batch["labels"];   (* {5} *)

debugPrint["per-sample forward pass:"];
Do[
    Module[{x, probs, pred, conf},
        x     = TTensorCreate @ NumericArray[imgs[[i]], "Real32"];
        probs = Normal @ TTensorData @ TRealize @ TFromNet[net, x];
        pred  = Position[probs, Max[probs]][[1, 1]] - 1;   (* 0-indexed *)
        conf  = Round[Max[probs], 0.001];
        debugPrint["  sample ", i, ": true=", labels[[i]],
                   " pred=", pred, " confidence=", conf]
    ],
    {i, 5}
];

debugPrint["done."];
