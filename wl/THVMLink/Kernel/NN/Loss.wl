(* ::Package:: *)
(* NN/Loss.wl - cross-entropy losses: probability-form, categorical (one-hot),
   and sparse (integer-label) categorical. *)

BeginPackage["WolframInstitute`THVMLink`", {"GeneralUtilities`"}];

SetUsage[TCrossEntropyLoss, "TCrossEntropyLoss[pred$, target$] = -sum(target$ * log(pred$)), probability-form categorical cross-entropy. Both inputs are same-shape TTerms; target$ is typically a one-hot vector. A small eps is added before the log to keep it finite."];
SetUsage[TSparseCategoricalCrossEntropy, "TSparseCategoricalCrossEntropy[logits$, intLabels$] is the categorical cross-entropy loss from pre-softmax logits$ and integer class labels$ (one int per sample, not one-hot; the tinygrad / Keras convention). intLabels$ has logits$' shape with the last (class) axis dropped; the loss is the mean over every sample (tinygrad's sparse_categorical_crossentropy). For one-hot targets use TCategoricalCrossEntropy."];
SetUsage[TCategoricalCrossEntropy, "TCategoricalCrossEntropy[logits$, targetOneHot$] is the categorical cross-entropy loss from pre-softmax logits$ and a one-hot target$. It uses the stable logsumexp form max(logits$) + log(sum(exp(logits$ - max(logits$)))) - sum(target$ * logits$) along the last axis, then averages over the leading batch axis (rank-1 logits$ skip the average). The max-subtract is grad-transparent and prevents exp overflow. For integer class labels use TSparseCategoricalCrossEntropy."];

Begin["`Private`"];

(* CrossEntropy probabilities form: target is a probability
   distribution (typically one-hot), pred is the model's predicted
   distribution.  Loss = -sum_i target_i * log(pred_i). *)
(* Cross-entropy with eps clamp: when softmax peaks sharply, the
   non-winning class probabilities round to zero and Log[0] -> -inf.
   Adding a small eps before the log keeps the loss finite without
   meaningfully changing the gradient on well-behaved distributions
   (eps below the ULP of probability slots > eps stays inert under
   round-to-nearest-even).  Standard in all production training
   pipelines. *)
TCrossEntropyLoss[pred_TTerm, target_TTerm] := - Total[target * Log[pred + TUOpConst[1.*^-7]]]

(* TCategoricalCrossEntropy[logits, targetOneHot] -- categorical
   cross-entropy from logits with a one-hot target.  loss =
   log(sum(exp(logits))) - sum(target * logits), reduced along the
   LAST axis (the class axis); for rank-2 batched inputs the
   per-sample loss is then averaged along axis 0.  Numerically naive
   (no max-subtract); fine for the activation magnitudes a typical
   NN forward produces. *)
TCategoricalCrossEntropy[logits_TTerm, target_TTerm] := With[{shape = tUopShape[logits]},
    Module[{rank, classAxis, sumShape, mFlat, mBcast, perSample},
        rank = Length[shape];
        classAxis = rank - 1;
        sumShape = ReplacePart[shape, classAxis + 1 -> 1];
        (* Stable logsumexp: log(sum(exp(z))) = m + log(sum(exp(z - m)))
           with m = max along the class axis.  Grad-transparent (the
           softmax sums to 1, so the dm/dz terms cancel) and required:
           without it random-init conv logits overflow exp -> NaN. *)
        mFlat = TUOpReduce[logits, classAxis, "MAX"];
        mBcast = TUOpExpand[TUOpReshape[mFlat, sumShape], shape];
        perSample = mFlat + Log[TUOpReduce[Exp[logits - mBcast], classAxis, "SUM"]] - TUOpReduce[target * logits, classAxis, "SUM"];
        If[ rank <= 1,
            perSample,
            Total[perSample] / shape[[1]] (* batch mean over axis 0 *)
        ]
    ]
]

(* TSparseCategoricalCrossEntropy[logits, intLabels]: log_softmax over the
   last axis at the labels, the one-hot an in-graph compare against an
   arange (src/nn/sparse_categorical_crossentropy.c).  Labels of another
   shape with one label per sample (a {1} for rank-1 logits) take the
   logits' shape without the class axis first. *)
TSparseCategoricalCrossEntropy[logits_TTerm, intLabels_TTerm] := With[{want = Most[tUopShape[logits]]},
    nnCall[TSparseCategoricalCrossEntropy, "sparse_categorical_crossentropy",
        {logits, If[tUopShape[intLabels] === want || Times @@ tUopShape[intLabels] =!= Times @@ want,
            intLabels, TUOpReshape[intLabels, want]]}, {-1}]
]

End[];

EndPackage[];
