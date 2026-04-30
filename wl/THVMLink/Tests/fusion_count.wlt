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
