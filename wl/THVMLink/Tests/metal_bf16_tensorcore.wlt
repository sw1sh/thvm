(* metal_bf16_tensorcore.wlt -- a bf16 matmul with buffer operands must run on
   the Metal simdgroup_matrix tensor-core path: correct to bf16 precision and
   matching the f32 path's result.

   Before this fix bf16 matmuls were gated out of the tensor-core templates at
   every layer (uop_dag_classify_matmul_shape, hand_opt_classify_matmul,
   uop_recognise_tc's downstream, propose, and the render_uop simdgroup load /
   store), so a bf16 GEMM fell to the generic scalar reduce (~25x slower than
   f32 TC).  bf16 operands now load into simdgroup_matrix<bfloat> fragments and
   accumulate in f32; a bf16 output is staged f32->bf16 through threadgroup
   memory at the store.  rel error stays at bf16 rounding (~0.02), not garbage. *)

VerificationTest[
    TInit[]; TReset[];
    Module[{ctx = TContextNew["metal"], worst},
        If[ ctx === 0, Return[True]];   (* skip on non-Metal platforms *)
        worst = TInContext[ctx,
            SeedRandom[7];
            Max @ Map[
                Function[mkn,
                    Module[{m = mkn[[1]], k = mkn[[2]], n = mkn[[3]], av, wv, ref, sd, yb},
                        av = RandomReal[{-0.5, 0.5}, {m, k}];
                        wv = RandomReal[{-0.5, 0.5}, {k, n}];
                        ref = av . wv;
                        sd = StandardDeviation @ Flatten @ ref;
                        yb = Normal @ TRealize @ TMatMul[
                            TRealize @ TUOpCast[TTensorCreate @ NumericArray[av, "Real32"], "bf16"],
                            TRealize @ TUOpCast[TTensorCreate @ NumericArray[wv, "Real32"], "bf16"]];
                        Max @ Abs @ Flatten[yb - ref] / sd]],
                {{64, 512, 1024}, {256, 3072, 3072}, {264, 1040, 2056}}]];
        TContextDestroy[ctx];
        worst < 0.05 || ctx === 0
    ],
    True,
    TestID -> "metal/bf16-tensorcore-matmul-matches-f32"
]

(* Batched strided/transposed attention matmuls on the tiled tensor-core path.
   THVM_TC_BATCHED=1 forces the register-blocked tiled emitter for the two FLUX
   attention GEMMs: the SCORES bmm (qh {h,sq,dh} @ kh^T, output {h,sq,sk}: batch
   axis LEADING in C) and the @V bmm (mhaBmmM, A read transposed, output
   {sq,h,dh}: batch axis in the MIDDLE -> M-major C, M rows heads*head_dim apart).
   Both Q/K/V are strided {h,seq,dim} views of contiguous {seq,h,dim} tensors.

   Guards two batched-tiled emitter fixes: (1) the C-store steps M rows by C's
   real M-axis stride (n_extent was wrong for the M-major @V output), and (2) the
   vectorized A/B load reinterprets the device buffer at the buffer's own dtype
   (a bf16-A / f32-B matmul previously float4-loaded the bf16 operand).  The tiled
   result must match the CPU reference to bf16 rounding -- garbage otherwise. *)
VerificationTest[
    TInit[]; TReset[];
    SetEnvironment["THVM_TC_BATCHED" -> "1"];
    Module[{ctx = TContextNew["metal"], h = 24, sq = 64, sk = 64, dh = 128,
            qd, kd, vd, refScores, refV, mkR, bmm, bmmM, worst},
        If[ ctx === 0, Return[True]];   (* skip on non-Metal platforms *)
        SeedRandom[424242];
        qd = RandomReal[{-1, 1}, {sq, h, dh}];
        kd = RandomReal[{-1, 1}, {sk, h, dh}];
        vd = RandomReal[{-1, 1}, {sk, h, dh}];
        (* CPU reference (f32): per-head scores then per-head context *)
        With[{qhd = Transpose[qd, {2, 1, 3}], khd = Transpose[kd, {2, 1, 3}],
              vhd = Transpose[vd, {2, 1, 3}]},
            refScores = Table[qhd[[b]] . Transpose[khd[[b]]], {b, h}];        (* {h,sq,sk} *)
            refV = Transpose[Table[(refScores[[b]]) . vhd[[b]], {b, h}], {2, 1, 3}]];  (* {sq,h,dh} *)
        worst = TInContext[ctx,
            mkR = TRealize@TUOpCast[TTensorCreate@NumericArray[#, "Real32"], "bf16"] &;
            bmm[a_, bm_, bb_, m_, kk_, nn_] := TUOpReduce[TUOpMul[
                TUOpExpand[TUOpReshape[a,  {bb, m, kk, 1}], {bb, m, kk, nn}],
                TUOpExpand[TUOpReshape[bm, {bb, 1, kk, nn}], {bb, m, kk, nn}]], 2, "SUM"];
            bmmM[a_, bm_, bb_, m_, kk_, nn_] := TUOpReduce[TUOpMul[
                TUOpExpand[TUOpReshape[Transpose[a, {2, 1, 3}], {m, bb, kk, 1}], {m, bb, kk, nn}],
                TUOpExpand[TUOpReshape[bm, {1, bb, kk, nn}], {m, bb, kk, nn}]], 2, "SUM"];
            Module[{q = mkR[qd], k = mkR[kd], v = mkR[vd], qh, kh, vh, sc, scN, ov, sd1, sd2},
                qh = Transpose[q, {2, 1, 3}]; kh = Transpose[k, {2, 1, 3}]; vh = Transpose[v, {2, 1, 3}];
                sc = bmm[qh, Transpose[kh, {1, 3, 2}], h, sq, dh, sk];     (* tiled scores {h,sq,sk} *)
                scN = Normal @ TRealize[sc];
                ov = Normal @ TRealize @ bmmM[mkR[refScores], vh, h, sq, sk, dh];  (* tiled @V {sq,h,dh} *)
                sd1 = StandardDeviation @ Flatten @ N @ refScores;
                sd2 = StandardDeviation @ Flatten @ N @ refV;
                Max[Max @ Abs @ Flatten[scN - refScores] / sd1,
                    Max @ Abs @ Flatten[ov - refV] / sd2]]];
        TContextDestroy[ctx];
        SetEnvironment["THVM_TC_BATCHED" -> "0"];
        worst < 0.05 || ctx === 0
    ],
    True,
    TestID -> "metal/bf16-batched-attention-tiled-matches-cpu"
]
