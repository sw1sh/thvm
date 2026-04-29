#!/usr/bin/env wolframscript
(* train.wls -- Phase-6 demo of the full training pipeline:
       forward (Conv + ReLU + MaxPool + Flatten + Linear)
        + sparse_categorical_crossentropy
        + per-weight TGrad (autodiff through the chain)
        + multi-tensor TAdam step.

   The plan's beautiful_mnist target architecture is
       Conv 1->32 5  -> ReLU -> Conv 32->32 5 -> ReLU -> BN32
        -> MaxPool 2x2 -> Conv 32->64 3 -> ReLU -> Conv 64->64 3
        -> ReLU -> BN64 -> MaxPool 2x2 -> Flatten -> Linear 576->10
   trained at BS=512 with Adam.  thvm currently lowers TConv2D as a
   per-batch-element kh*kw partial-sum chain (no batched conv yet),
   so a faithful BS=512 step would take hundreds of seconds.  Phase 9
   (im2col + sgemm) lifts that ceiling.

   This script demonstrates the pipeline composes cleanly with a
   trimmed architecture (one Conv + Pool + Linear) at BS=1, runs N
   steps, and logs per-step kernel count + wallclock + heap pressure
   without round-tripping any tensor data through the WL host.

   Run from the repo root:
       wolframscript -f wl/Examples/beautiful-mnist/train.wls
       N_STEPS=20 wolframscript -f wl/Examples/beautiful-mnist/train.wls
*)

PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];

print[args___] := WriteString["stdout",
    StringJoin @@ Map[ToString, {args}], "\n"]

SeedRandom[42];

nSteps = With[{n = Interpreter["Integer"][Environment["N_STEPS"]]},
    If[ FailureQ[n] || n === $Failed, 5, n]
];

(* === Tiny architecture for the smoke test ============================
   Input {1, 8, 8}                  (synthetic 8x8 "image")
   Conv 1->4, 3x3      -> {4, 6, 6}
   ReLU
   MaxPool 2x2          -> {4, 3, 3}
   Flatten              -> {1, 36}
   Linear 36 -> 10      -> {1, 10}

   ~400 params total -- exercises the full pipeline (Conv + ReLU +
   Pool + Flatten + Linear + sparse-CE + TGrad + TAdam) end-to-end
   in a couple of seconds.  Scaling up to the real beautiful_mnist
   architecture waits on Phase 9. *)

W1 = TTensorCreate @ TGlorot[{4, 1, 3, 3}];
b1 = TTensorCreate @ TZeros [{4}];
W2 = TTensorCreate @ TGlorot[{10, 36}];
b2 = TTensorCreate @ TZeros [{10}];

print["params: ",
    Times @@ TTensorShape[W1], " conv + ",
    Times @@ TTensorShape[W2], " fc weights + biases"];

mBufs = TZerosLike /@ {W1, b1, W2, b2};
vBufs = TZerosLike /@ {W1, b1, W2, b2};

(* Forward: returns the {10}-vector logits for one sample. *)
forward[img_TTerm] := Module[{h1, r1, p1, flat, h2},
    h1   = TConv2D[img, W1, b1];      (* {4, 6, 6} *)
    r1   = TReLU[h1];
    p1   = TMaxPool2d[r1, 2];          (* {4, 3, 3} *)
    flat = TUOpReshape[p1, {1, 36}];
    h2   = TMatVec[W2, flat] + b2;    (* {10} *)
    h2
]

(* Synthetic input + label -- no MNIST data load on first run.  Label
   walks 0..9 mod step so the optimiser has SOMETHING to converge
   towards, and the run is deterministic. *)
makeSample[step_Integer] := Module[{seed = 42 + step},
    SeedRandom[seed];
    {RandomReal[1, {1, 8, 8}], Mod[step, 10]}
]

print["Phase-6 training: ", nSteps, " steps, BS=1, lr=1e-3"];
print[""];

trainOne[step_Integer] := Module[{
    imgArr, label, imgT, tgtT, logits, loss,
    gW1, gb1, gW2, gb2, t0, dt, kBefore, kAfter
},
    kBefore = TKernelCount[];
    {imgArr, label} = makeSample[step];
    imgT = TTensorCreate @ NumericArray[imgArr, "Real32"];
    tgtT = TTensorCreate @ TOneHot[label, 10];

    t0     = AbsoluteTime[];
    logits = forward[imgT];
    loss   = TSparseCategoricalCrossEntropy[logits, tgtT];

    {gW1, gb1, gW2, gb2} = TGrad[loss, #] & /@ {W1, b1, W2, b2};
    TAdam[
        {W1, b1, W2, b2},
        TRealize /@ {gW1, gb1, gW2, gb2},
        mBufs, vBufs,
        step, 0.001, 0.9, 0.999, 1.0*^-8];

    dt     = AbsoluteTime[] - t0;
    kAfter = TKernelCount[];
    print["step ", step,
        ": label=", label,
        " wall=", NumberForm[dt * 1000., {6, 1}], "ms",
        " kernels=", kAfter - kBefore,
        " heap=", THeapPos[]];

    (* GC after each step so the per-iter UOP graph + grad
       intermediates don't accumulate.  The weight tids are
       reachable through EXTERN_PINS so they survive the sweep. *)
    TGCCollect[];
];

Do[trainOne[step], {step, 1, nSteps}];

print[""];
print["final TKernelCount=",  TKernelCount[]];
print["final TTotalBufBytes=", TTotalBufBytes[], " bytes"];
print["final THeapPos=",      THeapPos[]];

print["done."];
