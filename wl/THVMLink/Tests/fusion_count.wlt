(* fusion_count.wlt -- pin the kernel-count claim of the round-3
   tinygrad-style fusion arc.  The numbers were derived for the
   plan at ~/.claude/plans/magical-wondering-biscuit.md (Phase
   "Expected kernel counts"); any regression past these ceilings
   means the scheduler over-realized and should be diagnosed
   per the plan's rollback policy. *)

(* Subtract 1 for the reserved KERNELS[0] slot. *)
ClearAll[kernelDelta];
kernelDelta[before_, after_] := after - before;

(* Linear + MSE forward: matmul-as-REDUCE-tail + L2-loss-as-REDUCE
   fuses to two kernels (one per REDUCE root). *)
VerificationTest[
    TInit[];
    x = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0, 4.0}, "Real32"];
    w = TTensorCreate @ NumericArray[{0.1, 0.2, 0.3, 0.4}, "Real32"];
    t = TTensorCreate @ NumericArray[{1.0}, "Real32"];
    before = TKernelCount[];
    TRealize @ TMSELoss[TDot[w, x], t];
    kernelDelta[before, TKernelCount[]],
    2,
    TestID -> "fusion-count/linear-mse-forward-eq-2"
]

(* Linear + MSE forward+backward: full pipeline emits 3 kernels.
   2 forward (Dot, MSELoss) + 1 backward.  Phase 16's leak fix
   re-baselined this 3 -> 5 because the target-aware leaf rule
   wrapped non-target / non-differentiable branches in
   EXPAND-shaped scalar zeros that no longer numel-broadcast
   cleanly with the target-shaped sibling, so the outer ADD
   combiner had to materialize each side as its own kernel.
   The fix in `grad_zero_at` (return scalar CONST(0) instead of
   EXPAND'd zero) restores numel-1 broadcast through the ADD,
   collapsing the backward sub-graph back to one kernel and
   incidentally fixing the LeNet wrong-shape gradient bug
   (Conv2D + ReLU + MaxPool2d producing a {2, H_out, W_out}
   tensor instead of the {C_out, C_in, kh, kw} weight gradient). *)
VerificationTest[
    TInit[];
    x = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0, 4.0}, "Real32"];
    w = TTensorCreate @ NumericArray[{0.1, 0.2, 0.3, 0.4}, "Real32"];
    t = TTensorCreate @ NumericArray[{1.0}, "Real32"];
    before = TKernelCount[];
    TRealize @ TGrad[TMSELoss[TDot[w, x], t], w];
    kernelDelta[before, TKernelCount[]],
    3,
    TestID -> "fusion-count/linear-mse-forward-plus-backward-eq-3"
]

(* Softmax forward.  Naive softmax = exp(x) / sum(exp(x)) -- the
   exp output is shared by both the REDUCE and the divide, so
   realize_classify still marks it as a multi-consumer boundary
   (rule b).  After the Phase-3 softmax-style relaxation, the REDUCE
   itself fuses into the divide kernel: the chain
   REDUCE -> RECIP -> EXPAND -> MUL collapses to one kernel because
   the REDUCE's only consumer (RECIP) sits on a scalar-preserving
   chain bottoming out at an EXPAND back to vector shape.  Two
   kernels total (exp, fused divide-with-inlined-reduce).  Tinygrad's
   fully-fused softmax that ALSO recomputes exp inside the divide
   stays out of scope -- that needs a "vector-cost" inlining
   heuristic that the relaxation pass does not yet implement. *)
VerificationTest[
    TInit[];
    x = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0, 4.0}, "Real32"];
    before = TKernelCount[];
    TRealize @ TSoftmax[x];
    kernelDelta[before, TKernelCount[]],
    2,
    TestID -> "fusion-count/softmax-forward-eq-2"
]

(* (a + b) * c -- single elementwise chain fuses to one kernel.
   Already covered by tensors.wlt's compound-fuses-to-one-kernel
   test; repeated here as the simplest fusion-arc pin. *)
VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    b = TTensorCreate @ NumericArray[{4.0, 5.0, 6.0}, "Real32"];
    c = TTensorCreate @ NumericArray[{7.0, 8.0, 9.0}, "Real32"];
    before = TKernelCount[];
    TRealize[(a + b) * c];
    kernelDelta[before, TKernelCount[]],
    1,
    TestID -> "fusion-count/compound-elementwise-eq-1"
]

(* Long elementwise chain (8 ADDs over 9 distinct inputs).  All
   pairwise + binary; nothing reads an intermediate twice; the
   whole chain MUST fuse to one kernel.  Tinygrad: 1.  Failing
   here flags a regression in elementwise-chain absorption
   (realize_classify mis-classifying an interior ADD as a
   multi-consumer boundary, or a movement-op chain breaking the
   contig path). *)
VerificationTest[
    TInit[];
    ts  = Table[
        TTensorCreate @ NumericArray[{1.0 * i, 2.0 * i}, "Real32"],
        {i, 9}];
    sum = Fold[Plus, First @ ts, Rest @ ts];
    before = TKernelCount[];
    TRealize @ sum;
    kernelDelta[before, TKernelCount[]],
    1,
    TestID -> "fusion-count/long-add-chain-eq-1"
]

(* 16-deep elementwise chain alternating ADD / MUL.  Pure scalar
   op tree, no shared intermediates -- still one kernel. *)
VerificationTest[
    TInit[];
    ts  = Table[
        TTensorCreate @ NumericArray[{1.0 * i, 2.0 * i}, "Real32"],
        {i, 17}];
    expr = Fold[
        If[ EvenQ[#2], #1 + ts[[#2]], #1 * ts[[#2]]] &,
        First @ ts, Range[2, 17]];
    before = TKernelCount[];
    TRealize @ expr;
    kernelDelta[before, TKernelCount[]],
    1,
    TestID -> "fusion-count/16-deep-mixed-elementwise-eq-1"
]

(* Two-layer MLP forward (linear-relu-linear) -- fuses to 2
   matmul kernels, one per Dot.  ReLU absorbs into the
   downstream matmul's epilogue (or out as a separate pointwise
   kernel; today's classifier emits a separate pointwise kernel,
   so the bound is 3).  Tinygrad: 2.  This test pins our current
   ceiling and surfaces regressions when classifier rules
   change. *)
VerificationTest[
    TInit[];
    x  = TTensorCreate @ NumericArray[Range[8] * 1.0, "Real32"];
    w1 = TTensorCreate @ NumericArray[Table[i + j * 0.1, {i, 4}, {j, 8}], "Real32"];
    w2 = TTensorCreate @ NumericArray[Table[i - j * 0.1, {i, 2}, {j, 4}], "Real32"];
    h1 = TReLU @ TDot[w1, x];
    out = TDot[w2, h1];
    before = TKernelCount[];
    TRealize @ out;
    kernelDelta[before, TKernelCount[]] <= 3,
    True,
    TestID -> "fusion-count/two-layer-mlp-forward-le-3"
]

(* TGradMany must NOT pay quadratically for sharing N targets.
   With 4 targets sharing a forward DAG of ~8 forward kernels,
   total realize cost should stay well under "one separate full
   forward+backward per target" (which would be 4 * (forward + 1)
   ~ 36).  Empirical ceiling for the shared-leaves form is around
   12-18; we pin <= 24 as a generous regression guard.  A
   regression past this means the shared-forward-DAG memo broke. *)
VerificationTest[
    TInit[];
    x  = TTensorCreate @ NumericArray[Range[8] * 1.0, "Real32"];
    a  = TTensorCreate @ NumericArray[Table[i + j * 0.1, {i, 4}, {j, 8}], "Real32"];
    b  = TTensorCreate @ NumericArray[Table[i - j * 0.1, {i, 2}, {j, 4}], "Real32"];
    c  = TTensorCreate @ NumericArray[Range[2] * 1.0, "Real32"];
    out = TDot[b, TReLU @ TDot[a, x]] + c;
    loss = TSum @ (out * out);
    before = TKernelCount[];
    TRealize /@ TGradMany[loss, {x, a, b, c}];
    kernelDelta[before, TKernelCount[]] <= 24,
    True,
    TestID -> "fusion-count/gradmany-4-targets-le-24"
]

