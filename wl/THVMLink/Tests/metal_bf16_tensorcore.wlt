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
