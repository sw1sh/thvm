(* multiroot_shared_shrink.wlt -- cross-root buffer-lifetime regression.

   TRealize[{a, b}] bundles its roots in a TAG_CTR and materializes each
   child as a SEPARATE sub-pass (thvm_materialize recurses per child).  A
   forward activation reached by BOTH roots -- here a shared lazy matmul
   sliced by two SHRINK views, one per root -- looks single-consumer
   WITHIN the first child's sub-pass, so the legacy freelist memory planner
   (mem_plan_record) used to record + freelist-push its output buffer; a
   later same-size allocation in the SIBLING child's sub-pass then recycled
   the still-live bytes.  The CPU/CUDA arena planner already extends such a
   buffer's lifetime via xpass_is_shared (whole-bundle root walk); Metal
   rides the freelist, which had NO cross-root protection -- so the offset-0
   (second-materialized) root's slice was corrupted at SwiGLU-buffer scale.

   Fix: gate mem_plan_record on !xpass_is_shared(boundary_loc), the freelist
   analogue of the arena planner's cross-root lifetime extension (tinygrad
   schedule/memory.py: held_bufs excluded from planning + last_appearance
   spans the whole linearized schedule).  src/schedule/materialize.c.

   The bug is Metal-only and surfaces only at real (SwiGLU) buffer scale; on
   CPU the assertion holds unconditionally.  Run the suite under DEV=metal
   to exercise the Metal path -- the multi-root result must match the
   single-root reference exactly (max|d| == 0). *)

VerificationTest[
    TInit[];
    TReset[];
    SeedRandom[7];
    dim = 3072; mlp = 9216; stxt = 512; simg = 256; s = stxt + simg;
    md = TRealize@TTensorCreate@NumericArray[N@RandomReal[{-0.3, 0.3}, #], "Real32"] &;
    sA = md[{s, dim}]; sB = md[{dim, dim}];
    shared = Function[{}, TMatMul[sA, sB]];     (* {s, dim} shared lazy node *)
    woT = md[{dim, dim}]; woI = md[{dim, dim}];
    winT = md[{2 mlp, dim}]; woutT = md[{dim, mlp}];
    winI = md[{2 mlp, dim}]; woutI = md[{dim, mlp}];
    silu = TUOpMul[#, TSigmoid[#]] &;
    swiglu = Function[{x, rows, win, wout},
        With[{hh = TMatMul[x, Transpose@TRealize@win]},
            With[{
                x1 = TUOpShrink[hh, {{0, rows}, {0, mlp}}],
                x2 = TUOpShrink[hh, {{0, rows}, {mlp, 2 mlp}}]},
                TMatMul[TUOpMul[silu[x1], x2], Transpose@TRealize@wout]]]];
    consumer = Function[{ctx, off, n, wo, win, wout},
        With[{r = TMatMul[TUOpShrink[ctx, {{off, off + n}, {0, dim}}], Transpose@TRealize@wo]},
            TUOpAdd[r, swiglu[r, n, win, wout]]]];
    fT = consumer[#, 0, stxt, woT, winT, woutT] &;
    fI = consumer[#, stxt, simg, woI, winI, woutI] &;
    refT = Normal@TRealize@fT[shared[]];                  (* single-root reference *)
    rM = TRealize@{fI[shared[]], fT[shared[]]};            (* bundled multi-root *)
    mTxt = Normal@TRealize@rM[[2]];
    Max@Abs@Flatten[mTxt - refT] < 1.*^-3,
    True,
    TestID -> "multiroot/shared-shrink-offset0-not-corrupted"
]
