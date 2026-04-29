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

W1 = TGlorot[{4, 1, 3, 3}];
b1 = TZeros [{4}];
W2 = TGlorot[{10, 36}];
b2 = TZeros [{10}];

print["params: ",
    Times @@ TTensorShape[W1], " conv + ",
    Times @@ TTensorShape[W2], " fc weights + biases"];

mBufs = TZerosLike /@ {W1, b1, W2, b2};
vBufs = TZerosLike /@ {W1, b1, W2, b2};

(* Pre-allocate per-iter input + target slots ONCE.  Each step writes
   new bytes into these buffers via TSet rather than constructing
   fresh TenDescs (which would change the buf_id and invalidate
   the captured kernel sequence). *)
imgT = TZeros[{1, 8, 8}];
tgtT = TZeros[{10}];

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

(* The optimizer step is wrapped as a TJit closure that captures the
   forward + backward + Adam-update kernel sequence on the FIRST
   call.  Subsequent calls replay the captured sequence directly --
   skipping realize_classify, materialize, and the kernel-program
   hash-cons lookup -- so the per-iter wallclock drops from
   "minutes" (TGrad-bound) to whatever the actual kernel work
   takes. *)
stepFn = Function[{stepIdx},
    Module[{logits, loss},
        logits = forward[imgT];
        loss   = TSparseCategoricalCrossEntropy[logits, tgtT];
        TAdam[
            {W1, b1, W2, b2},
            TRealize /@ Table[TGrad[loss, p], {p, {W1, b1, W2, b2}}],
            mBufs, vBufs,
            stepIdx, 0.001, 0.9, 0.999, 1.0*^-8]
    ]
];

jittedStep = TJit[stepFn];

trainOne[step_Integer] := Module[{imgArr, label, t0, dt, kBefore, kAfter},
    kBefore = TKernelCount[];
    {imgArr, label} = makeSample[step];
    (* Refresh per-iter input + target bytes in place.  The captured
       kernels read from imgT/tgtT's stable buf_id, so this TSet
       feeds the next replay with fresh data. *)
    TSet[imgT, TTensorCreate @ NumericArray[imgArr, "Real32"]];
    TSet[tgtT, TOneHot[label, 10]];

    t0 = AbsoluteTime[];
    jittedStep[step];
    dt = AbsoluteTime[] - t0;

    kAfter = TKernelCount[];
    print["step ", step,
        ": label=", label,
        " wall=", NumberForm[dt * 1000., {6, 1}], "ms",
        " kernels(new)=", kAfter - kBefore,
        " jit-replays=", TJitOpCount[jittedStep],
        " heap=", THeapPos[]];

    (* GC after each step so per-iter UOP graph + grad intermediates
       don't accumulate.  Weight tids are reachable through
       EXTERN_PINS so they survive the sweep. *)
    TGCCollect[];
];

Do[trainOne[step], {step, 1, nSteps}];

print[""];
print["final TKernelCount=",  TKernelCount[]];
print["final TTotalBufBytes=", TTotalBufBytes[], " bytes"];
print["final THeapPos=",      THeapPos[]];

print["done."];
