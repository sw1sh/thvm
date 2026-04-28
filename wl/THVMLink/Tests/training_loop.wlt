(* training_loop.wlt -- the canonical training-loop pattern.
   Compute graph references CONCRETE weight tensors (not TLam-bound
   variables); TAssign mutates those tensors in place each step.
   The whole step graph is materialized ONCE before the loop runs;
   each iteration just re-fires the same kernels with the in-place
   weight buffers updated by the previous iter's ASSIGN.

   Distinct from sgd.wlt's bound-w pattern, which substitutes a
   fresh w UOP graph per iter via APP-LAM beta -- there each
   iter's compute lives at a different heap loc, so materialize
   emits N independent kernels (N=iter-count).  The bound-w
   pattern hits a cubic scaling cliff past n~12 (see
   docs/loop-profile.md); the ASSIGN pattern is linear in n at the
   per-iter cost and constant in kernel count. *)

(* === scalar weight: f(w) = (w - target)^2, lr=0.1 === *)

VerificationTest[
    TInit[];
    Module[{w, tgt, lr, step, matStep, kernelCountBefore},
        w   = TTensorCreate @ NumericArray[{0.}, "Real32"];
        tgt = TTensorCreate @ NumericArray[{4.}, "Real32"];
        lr  = TUOpConst[0.1, "f32"];
        (* analytic gradient: d/dw (w-tgt)^2 = 2(w-tgt) *)
        step = TAssign[w,
            TUOpAdd[w, TUOpNeg[TUOpMul[lr,
                TUOpMul[TUOpConst[2.0, "f32"],
                    TUOpAdd[w, TUOpNeg[tgt]]]]]]];
        matStep = TMaterialize[TNf[step]];
        kernelCountBefore = TKernelCount[];
        TDef["sgd_scalar_loop",
            TLam[m, TIfZero[m, TUOpConst[0.0, "f32"],
                TPriForce[TRef["sgd_scalar_step"],
                    TApp[TRef["sgd_scalar_loop"], TOp2["-", m, TNum[1]]]]]]];
        TDef["sgd_scalar_step", matStep];
        TWnf @ TApp[TRef["sgd_scalar_loop"], TNum[20]];
        (* w_{i+1} = 0.8 w_i + 0.2 tgt; 20 iters from 0:
             w_n = (1 - 0.8^n) * tgt = (1 - 0.8^20) * 4 ~= 3.954 *)
        {Round[First @ Normal @ TTensorData[w], 0.001],
         (* Exactly one kernel emitted before the loop runs;
            the loop re-fires the same kernel each iter. *)
         kernelCountBefore - 1}
    ],
    {3.954, 1},
    TestID -> "training-loop/scalar-l2-20iters-one-kernel"
]

(* === vector weight + TGrad === *)

VerificationTest[
    TInit[];
    Module[{w, tgt, lr, gradExpr, step, matStep, profile},
        w   = TTensorCreate @ NumericArray[{0., 0., 0.}, "Real32"];
        tgt = TTensorCreate @ NumericArray[{1., 2., 3.}, "Real32"];
        lr  = TUOpConst[0.1, "f32"];
        gradExpr = TGrad[TL2Loss[TUOpAdd[w, TUOpNeg[tgt]]], w];
        step = TAssign[w,
            TUOpAdd[w, TUOpNeg[TUOpMul[lr, gradExpr]]]];
        matStep = TMaterialize[TNf[step]];
        TDef["sgd_vec_loop",
            TLam[m, TIfZero[m, TUOpConst[0.0, "f32"],
                TPriForce[TRef["sgd_vec_step"],
                    TApp[TRef["sgd_vec_loop"], TOp2["-", m, TNum[1]]]]]]];
        TDef["sgd_vec_step", matStep];
        TWnf @ TApp[TRef["sgd_vec_loop"], TNum[10]];
        profile = TProfile[];
        {Round[Normal @ TTensorData[w], 0.0001],
         profile["Kernels"]}
    ],
    {Round[(1 - 0.8^10) * {1., 2., 3.}, 0.0001], 1},
    TestID -> "training-loop/vector-tgrad-l2-10iters-one-kernel"
]

(* === matrix weight: (W - tgt)^2 elementwise, sum reduce === *)

VerificationTest[
    TInit[];
    Module[{W, tgt, lr, gradExpr, step, matStep},
        W   = TTensorCreate @ NumericArray[{{0., 0.}, {0., 0.}}, "Real32"];
        tgt = TTensorCreate @ NumericArray[{{1., 2.}, {3., 4.}}, "Real32"];
        lr  = TUOpConst[0.1, "f32"];
        gradExpr = TGrad[TL2Loss[TUOpAdd[W, TUOpNeg[tgt]]], W];
        step = TAssign[W,
            TUOpAdd[W, TUOpNeg[TUOpMul[lr, gradExpr]]]];
        matStep = TMaterialize[TNf[step]];
        TDef["sgd_mat_loop",
            TLam[m, TIfZero[m, TUOpConst[0.0, "f32"],
                TPriForce[TRef["sgd_mat_step"],
                    TApp[TRef["sgd_mat_loop"], TOp2["-", m, TNum[1]]]]]]];
        TDef["sgd_mat_step", matStep];
        TWnf @ TApp[TRef["sgd_mat_loop"], TNum[10]];
        Round[Normal @ TTensorData[W], 0.0001]
    ],
    Round[(1 - 0.8^10) * {{1., 2.}, {3., 4.}}, 0.0001],
    TestID -> "training-loop/matrix-tgrad-10iters"
]

(* === long loop: n=200 should run in well under a second.  This
   is the smoke test that the per-iter cost stays bounded. *)

VerificationTest[
    TInit[];
    Module[{w, tgt, lr, gradExpr, step, matStep, t0, dt, profile},
        w   = TTensorCreate @ NumericArray[{0., 0., 0.}, "Real32"];
        tgt = TTensorCreate @ NumericArray[{1., 2., 3.}, "Real32"];
        lr  = TUOpConst[0.1, "f32"];
        gradExpr = TGrad[TL2Loss[TUOpAdd[w, TUOpNeg[tgt]]], w];
        step = TAssign[w,
            TUOpAdd[w, TUOpNeg[TUOpMul[lr, gradExpr]]]];
        matStep = TMaterialize[TNf[step]];
        TDef["sgd_long_loop",
            TLam[m, TIfZero[m, TUOpConst[0.0, "f32"],
                TPriForce[TRef["sgd_long_step"],
                    TApp[TRef["sgd_long_loop"], TOp2["-", m, TNum[1]]]]]]];
        TDef["sgd_long_step", matStep];
        t0 = AbsoluteTime[];
        TWnf @ TApp[TRef["sgd_long_loop"], TNum[200]];
        dt = AbsoluteTime[] - t0;
        profile = TProfile[];
        (* w should converge to tgt; one kernel total. *)
        {Round[Normal @ TTensorData[w], 0.0001],
         profile["Kernels"],
         dt < 5.0}
    ],
    {{1., 2., 3.}, 1, True},
    TestID -> "training-loop/200-iters-one-kernel-under-5s"
]

(* === Kernel program hash-cons: structurally identical kernels
       across iterations share one KProgOp[] in the cache, even
       in the slow bound-w pattern where each iter emits its own
       KernelEntry.  Verifies kernel_program_cache_size stays
       bounded as n grows. === *)

VerificationTest[
    TInit[];
    Module[{target, lr, w0, kernels5, prog5, kernels10, prog10},
        target = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
        lr     = TUOpConst[0.1, "f32"];
        TDef["sgd_loop_kp_test",
            TLam[w, TLam[n, TIfZero[n, w,
                TApp[TApp[TRef["sgd_loop_kp_test"],
                    TUOpAdd[w, TUOpNeg[TUOpMul[lr,
                        TGrad[TL2Loss[TUOpAdd[w, TUOpNeg[target]]], w]]]]],
                    TOp2["-", n, TNum[1]]]]]]];
        w0 = TTensorCreate @ NumericArray[{0.0, 0.0, 0.0}, "Real32"];
        TRealize[TApp[TApp[TRef["sgd_loop_kp_test"], w0], TNum[5]]];
        kernels5 = TKernelCount[] - 1;
        prog5    = TKernelProgramCacheSize[];

        TInit[];
        target = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
        lr     = TUOpConst[0.1, "f32"];
        TDef["sgd_loop_kp_test",
            TLam[w, TLam[n, TIfZero[n, w,
                TApp[TApp[TRef["sgd_loop_kp_test"],
                    TUOpAdd[w, TUOpNeg[TUOpMul[lr,
                        TGrad[TL2Loss[TUOpAdd[w, TUOpNeg[target]]], w]]]]],
                    TOp2["-", n, TNum[1]]]]]]];
        w0 = TTensorCreate @ NumericArray[{0.0, 0.0, 0.0}, "Real32"];
        TRealize[TApp[TApp[TRef["sgd_loop_kp_test"], w0], TNum[10]]];
        kernels10 = TKernelCount[] - 1;
        prog10    = TKernelProgramCacheSize[];

        (* Kernel count grows with n (bound-w emits one KernelEntry per
           iter); the program cache stays bounded -- the per-iter step
           kernel is structurally identical so it gets one cache entry,
           plus one for the small scalar-zero CONST kernel.  prog should
           NOT grow with n. *)
        {kernels5, prog5, kernels10, prog10, prog5 === prog10}
    ],
    {6, 2, 11, 2, True},
    TestID -> "training-loop/bound-w-kernel-program-hash-cons"
]

(* === TGrad target lookup uses the ACTUAL tensor, not a TVAR.  The
       step graph references w by tid; ASSIGN patches w's buffer in
       place; subsequent kernel re-fires read the new buffer through
       the same input slot.  No bound-variable substitution. === *)

VerificationTest[
    TInit[];
    Module[{w, gradExpr, observedTid},
        w = TTensorCreate @ NumericArray[{0., 0., 0.}, "Real32"];
        observedTid = TTermVal[w];
        gradExpr = TGrad[TL2Loss[w], w];
        TWnf @ TAssign[w, gradExpr];
        (* w's tid should be unchanged: ASSIGN preserves identity. *)
        TTermVal[w] === observedTid
    ],
    True,
    TestID -> "training-loop/assign-preserves-weight-tid"
]
