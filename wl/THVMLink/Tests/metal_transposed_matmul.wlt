(* metal_transposed_matmul.wlt -- OPEN BUG (documented failing test).

   On Metal, a matmul whose B operand is a TRANSPOSED VIEW, y = A . Transpose[W],
   is wrong for non-tiny shapes: at {64,96} . Transpose[{128,96}] the result has
   rel error ~6 vs the host reference (and vs the CPU backend, which is exact).
   The SAME matmul is correct when:
     - the shape is tiny (2x2 . Transpose[2x2] matches by hand), or
     - B is left as an un-realized (cast/transpose-fused) view rather than a
       materialized buffer, or
     - it runs on the CPU backend.
   So the defect is specific to the Metal lowering of a matmul that reads a
   transposed-stride view over a MATERIALIZED buffer above the tile threshold.
   It affects f32 (clearest repro below) and bf16 alike; it is masked in real
   workloads (FLUX / GPT-2) because there the matmul output feeds further ops
   and the fused lowering reaches a different, correct kernel.

   This blocks bf16-native FLUX weights (fxLinear = TMatMul[x, Transpose[W]]
   with a realized bf16 W).  Investigation notes: NOT the tile-JIT codegen
   (THVM_DUMP_TILE_JIT_SRC emits nothing for this matmul -- it routes through a
   precompiled metallib kernel), NOT the metal_dispatch premat gather (its
   needs_premat branch does not fire here), NOT the simdgroup tensor-core
   template.  Root cause is in the precompiled Metal matmul kernel's handling
   of a transposed-stride B, or in the dispatch that feeds it a strided view
   instead of a contiguous buffer.  See memory project_metal_transposed_matmul_bug. *)

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
