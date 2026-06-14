(* metal_transposed_matmul.wlt -- regression guard for the Metal tiled
   tensor-core matmul reading a TRANSPOSED-VIEW B operand.

   y = A . Transpose[W] with a materialized W: B = Transpose[W] is a
   transposed-stride view ({in,out} over the {out,in} buffer).  This was wrong
   for non-tiny shapes (rel ~6 vs the host/CPU reference) because the fast tiled
   path (render_uop.c rmu_emit_matmul_tc_tiled) hardcoded a row-major B leading
   dimension and staged the wrong bytes.  Fixed by staging a transposed B from
   the weight's strides (b_trans path: B[k][n] = W[n][k], coalesced over W's
   contiguous K).  This is the contiguous-weight FLUX path -- fxLinear loads
   {out,in} weights and matmuls Transpose[w], holding the weight ONCE on the
   device instead of pre-transposing to a second {in,out} buffer.

   The non-tiled simdgroup paths (direct-load + threadgroup-staged) already
   carried a_trans/b_trans/lda/ldb; the tiled path was the remaining gap. *)

VerificationTest[
    TInit[]; TReset[];
    Module[{ctx = TContextNew["metal"], av, wv, ref, sd, rel},
        If[ ctx === 0, Return[True]];   (* skip on no-Metal platforms *)
        SeedRandom[7];
        av = RandomReal[{-1, 1}, {64, 96}];
        wv = RandomReal[{-1, 1}, {128, 96}];   (* {out, in}; B = Transpose[wv] is {in, out} *)
        ref = av . Transpose[wv];
        sd  = StandardDeviation @ Flatten @ ref;
        rel = TInContext[ctx,
            Module[{a = TTensorCreate @ NumericArray[av, "Real32"],
                    w = TTensorCreate @ NumericArray[wv, "Real32"]},
                Max @ Abs @ Flatten[Normal @ TRealize @ TMatMul[a, Transpose[w]] - ref] / sd]];
        TContextDestroy[ctx];
        rel < 0.01 || ctx === 0
    ],
    True,
    TestID -> "metal/transposed-matmul-matches-host-nontiny"
]

(* Non-square, larger, tile-eligible: exercises register blocking + the
   coalesced transposed-B staging at a FLUX-projection-like shape. *)
VerificationTest[
    TInit[]; TReset[];
    Module[{ctx = TContextNew["metal"], av, wv, ref, sd, rel},
        If[ ctx === 0, Return[True]];
        SeedRandom[11];
        av = RandomReal[{-1, 1}, {128, 256}];
        wv = RandomReal[{-1, 1}, {512, 256}];
        ref = av . Transpose[wv];
        sd  = StandardDeviation @ Flatten @ ref;
        rel = TInContext[ctx,
            Module[{a = TTensorCreate @ NumericArray[av, "Real32"],
                    w = TTensorCreate @ NumericArray[wv, "Real32"]},
                Max @ Abs @ Flatten[Normal @ TRealize @ TMatMul[a, Transpose[w]] - ref] / sd]];
        TContextDestroy[ctx];
        rel < 0.01 || ctx === 0
    ],
    True,
    TestID -> "metal/transposed-matmul-nonsquare-tiled"
]

(* No-regression on the contiguous-B path: the SAME product fed a materialized
   {in,out} B must stay exact (the b_trans=0 codegen is byte-identical). *)
VerificationTest[
    TInit[]; TReset[];
    Module[{ctx = TContextNew["metal"], av, wv, ref, sd, rel},
        If[ ctx === 0, Return[True]];
        SeedRandom[11];
        av = RandomReal[{-1, 1}, {128, 256}];
        wv = RandomReal[{-1, 1}, {512, 256}];
        ref = av . Transpose[wv];
        sd  = StandardDeviation @ Flatten @ ref;
        rel = TInContext[ctx,
            Module[{a  = TTensorCreate @ NumericArray[av, "Real32"],
                    wt = TTensorCreate @ NumericArray[Transpose[wv], "Real32"]},
                Max @ Abs @ Flatten[Normal @ TRealize @ TMatMul[a, wt] - ref] / sd]];
        TContextDestroy[ctx];
        rel < 0.01 || ctx === 0
    ],
    True,
    TestID -> "metal/transposed-matmul-contiguous-noregression"
]
