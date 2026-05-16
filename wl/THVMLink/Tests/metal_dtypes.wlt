(* metal_dtypes.wlt -- Metal backend dispatch on per-dtype shader
   variants (Phase I).  Only f32 and i32 are wired today; the
   metal_kernel_supported predicate gates the rest back to CPU.

   Tests run under TInContext[ctx, ...] where ctx is a fresh Metal
   context (TContextNew["metal"]).  Skipped on platforms without
   Metal (TContextNew returns 0).  *)

VerificationTest[
    TInit[]; TReset[];
    x = TTensorCreate @ NumericArray[{0.0}, "Real32"];
    TRealize[x + TUOpConst[-1.0, "f32"]];
    src = TKernelSource[TKernelCount[] - 1, "Metal"];
    StringContainsQ[src, "as_type<float>(0xbf800000u)"]
        && ! StringContainsQ[src, "-1f"],
    True,
    TestID -> "metal/source-f32-const-uses-raw-bits"
]

VerificationTest[
    (* f32 elementwise add via Metal. *)
    TInit[]; TReset[];
    Module[{ctx = TContextNew["metal"], result},
        If[ ctx === 0, Return[True]];   (* skip on no-Metal platforms *)
        result = TInContext[ctx,
            a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0, 4.0}, "Real32"];
            b = TTensorCreate @ NumericArray[{0.5, 0.5, 0.5, 0.5}, "Real32"];
            Normal @ TTensorData @ TRealize[a + b]
        ];
        TContextDestroy[ctx];
        result === {1.5, 2.5, 3.5, 4.5} || ctx === 0
    ],
    True,
    TestID -> "metal/f32-add"
]

VerificationTest[
    (* i32 elementwise add via Metal -- new in Phase I. *)
    TInit[]; TReset[];
    Module[{ctx = TContextNew["metal"], result},
        If[ ctx === 0, Return[True]];
        result = TInContext[ctx,
            a = TTensorCreate @ NumericArray[{1, 2, 3, 4}, "Integer32"];
            b = TTensorCreate @ NumericArray[{10, 20, 30, 40}, "Integer32"];
            Normal @ TTensorData @ TRealize[a + b]
        ];
        TContextDestroy[ctx];
        result === {11, 22, 33, 44} || ctx === 0
    ],
    True,
    TestID -> "metal/i32-add"
]

VerificationTest[
    (* f32 matmul should route through the direct Metal GEMM shader. *)
    TInit[]; TReset[];
    Module[{ctx = TContextNew["metal"], result},
        If[ ctx === 0, Return[True]];
        result = TInContext[ctx,
            a = TTensorCreate @ NumericArray[{{1., 2., 3.}, {4., 5., 6.}}, "Real32"];
            b = TTensorCreate @ NumericArray[{{7., 8.}, {9., 10.}, {11., 12.}}, "Real32"];
            out = TRealize @ TMatMul[a, b];
            kid = TKernelCount[] - 1;
            {Round[Normal @ TTensorData[out], 0.001],
             TKernelDispatchKind[kid]}
        ];
        TContextDestroy[ctx];
        result === {{{58., 64.}, {139., 154.}}, "metal-tile"} || ctx === 0
    ],
    True,
    TestID -> "metal/f32-matmul-direct-gemm"
]

VerificationTest[
    (* Tiled GEMM bounds: non-multiple M/N/K exercises threadgroup
       edge tiles and the second K block. *)
    TInit[]; TReset[];
    Module[{ctx = TContextNew["metal"], result, aData, bData, ref},
        If[ ctx === 0, Return[True]];
        aData = ArrayReshape[N[Mod[Range[17 * 23], 7] - 3], {17, 23}];
        bData = ArrayReshape[N[Mod[Range[23 * 19], 5] - 2], {23, 19}];
        ref = aData . bData;
        result = TInContext[ctx,
            a = TTensorCreate @ NumericArray[aData, "Real32"];
            b = TTensorCreate @ NumericArray[bData, "Real32"];
            out = TRealize @ TMatMul[a, b];
            kid = TKernelCount[] - 1;
            actual = Normal @ TTensorData[out];
            {Max[Abs[Flatten[actual - ref]]] < 0.001,
             Dimensions[actual],
             TKernelDispatchKind[kid]}
        ];
        TContextDestroy[ctx];
        result === {True, {17, 19}, "metal-tile"}
    ],
    True,
    TestID -> "metal/f32-matmul-tiled-gemm-tails"
]

VerificationTest[
    (* Rank-1 TMatVec should promote through the generic TILE_MMA
       recognizer as GEMM with N=1.  This keeps the default path on
       lowered primitives. *)
    TInit[]; TReset[];
    Module[{ctx = TContextNew["metal"], result, oldBackend,
            oldSpecialized, restore},
        oldBackend = Environment["THVM_BACKEND"];
        oldSpecialized = Environment["THVM_METAL_SPECIALIZED"];
        restore[] := (
            If[StringQ[oldBackend],
                SetEnvironment["THVM_BACKEND" -> oldBackend],
                SetEnvironment["THVM_BACKEND" -> ""]];
            If[StringQ[oldSpecialized],
                SetEnvironment["THVM_METAL_SPECIALIZED" -> oldSpecialized],
                SetEnvironment["THVM_METAL_SPECIALIZED" -> ""]]
        );
        SetEnvironment["THVM_BACKEND" -> "metal"];
        SetEnvironment["THVM_METAL_SPECIALIZED" -> "0"];
        If[ ctx === 0, restore[]; Return[True]];
        result = TInContext[ctx,
            w = TTensorCreate @ NumericArray[
                {{1., 2., 3.}, {4., 5., 6.}}, "Real32"];
            x = TTensorCreate @ NumericArray[{7., 8., 9.}, "Real32"];
            out = TRealize @ TMatVec[w, x];
            kid = TKernelCount[] - 1;
            {Round[Normal @ TTensorData[out], 0.001],
             TKernelDispatchKind[kid],
             TKernelProposed[kid]}
        ];
        TContextDestroy[ctx];
        restore[];
        result === {{50., 122.}, "metal-tile",
                    {TOpt["TC", 0, 32], TOpt["TC", 0, 16],
                     TOpt["TC", 0, 8]}}
    ],
    True,
    TestID -> "metal/f32-matvec-rank1-promotes-tile-mma"
]

VerificationTest[
    (* The old direct GEMV diagnostic path is retired; even with
       THVM_METAL_SPECIALIZED enabled, rank-1 TMatVec should keep
       using the generic TILE_MMA/metal-gemm route. *)
    TInit[]; TReset[];
    Module[{ctx = TContextNew["metal"], result, oldSpecialized, restore},
        oldSpecialized = Environment["THVM_METAL_SPECIALIZED"];
        restore[] := If[StringQ[oldSpecialized],
            SetEnvironment["THVM_METAL_SPECIALIZED" -> oldSpecialized],
            SetEnvironment["THVM_METAL_SPECIALIZED" -> ""]];
        SetEnvironment["THVM_METAL_SPECIALIZED" -> "1"];
        If[ ctx === 0, restore[]; Return[True]];
        result = TInContext[ctx,
            w = TTensorCreate @ NumericArray[
                {{1., 2., 3.}, {4., 5., 6.}}, "Real32"];
            x = TTensorCreate @ NumericArray[{7., 8., 9.}, "Real32"];
            out = TRealize @ TMatVec[w, x];
            kid = TKernelCount[] - 1;
            {Round[Normal @ TTensorData[out], 0.001],
             TKernelDispatchKind[kid]}
        ];
        TContextDestroy[ctx];
        restore[];
        result === {{50., 122.}, "metal-tile"} || ctx === 0
    ],
    True,
    TestID -> "metal/f32-matvec-specialized-still-generic"
]

VerificationTest[
    (* TC opts are metadata for the TILE_MMA-backed Metal GEMM path:
       proposal exposes tile sizes, apply records the selected size,
       and the next same-shape matmul dispatches through metal-gemm. *)
    TInit[]; TReset[];
    Module[{ctx = TContextNew["metal"], result, oldBackend, restore,
            aData, bData, ref},
        oldBackend = Environment["THVM_BACKEND"];
        restore[] := If[StringQ[oldBackend],
            SetEnvironment["THVM_BACKEND" -> oldBackend],
            SetEnvironment["THVM_BACKEND" -> ""]];
        SetEnvironment["THVM_BACKEND" -> "metal"];
        If[ ctx === 0, restore[]; Return[True]];
        aData = ArrayReshape[N[Mod[Range[17 * 23], 7] - 3], {17, 23}];
        bData = ArrayReshape[N[Mod[Range[23 * 19], 5] - 2], {23, 19}];
        ref = aData . bData;
        result = TInContext[ctx,
            a = TTensorCreate @ NumericArray[aData, "Real32"];
            b = TTensorCreate @ NumericArray[bData, "Real32"];
            TRealize @ TMatMul[a, b];
            kid = TKernelCount[] - 1;
            props = TKernelProposed[kid];
            TKernelApplyOpt[kid, TOpt["TC", 0, 8]];
            out2 = TRealize @ TMatMul[a, b];
            kid2 = TKernelCount[] - 1;
            actual = Normal @ TTensorData[out2];
            {props,
             TKernelDispatchKind[kid2],
             Max[Abs[Flatten[actual - ref]]] < 0.001}
        ];
        TContextDestroy[ctx];
        restore[];
        result === {{TOpt["TC", 0, 32], TOpt["TC", 0, 16],
                     TOpt["TC", 0, 8]}, "metal-tile", True}
    ],
    True,
    TestID -> "metal/f32-matmul-tc-tile-opt"
]

VerificationTest[
    (* Fire-time autotune benchmarks must not poison the real TJit
       capture pass.  After input mutation, replay should refire the
       full producer chain instead of reusing benchmark intermediates. *)
    TInit[]; TReset[];
    Module[{ctx = TContextNew["metal"], result, oldAuto, oldRuns, restore},
        oldAuto = Environment["THVM_AUTOTUNE"];
        oldRuns = Environment["THVM_AUTOTUNE_RUNS"];
        restore[] := (
            If[StringQ[oldAuto],
                SetEnvironment["THVM_AUTOTUNE" -> oldAuto],
                SetEnvironment["THVM_AUTOTUNE" -> ""]];
            If[StringQ[oldRuns],
                SetEnvironment["THVM_AUTOTUNE_RUNS" -> oldRuns],
                SetEnvironment["THVM_AUTOTUNE_RUNS" -> ""]]
        );
        SetEnvironment["THVM_AUTOTUNE" -> "1"];
        SetEnvironment["THVM_AUTOTUNE_RUNS" -> "1"];
        If[ ctx === 0, restore[]; Return[True]];
        result = TInContext[ctx,
            x = TZeros[{1, 8, 8}];
            w = TTensorCreate @ NumericArray[
                ConstantArray[1., {2, 1, 3, 3}], "Real32"];
            b = TZeros[{2}];
            fc = TTensorCreate @ NumericArray[
                ConstantArray[1., {4, 72}], "Real32"];
            TSet[x, TTensorCreate @ NumericArray[
                ConstantArray[1., {1, 8, 8}], "Real32"]];
            build[] := TMatVec[fc,
                TUOpReshape[TReLU[TConv2D[x, w, b]], {1, 72}]];
            f = TJit[Function[{}, TRealize @ build[]]];
            out = f[];
            v1 = Normal @ TTensorData @ out;
            TSet[x, TTensorCreate @ NumericArray[
                ConstantArray[2., {1, 8, 8}], "Real32"]];
            f[];
            v2 = Normal @ TTensorData @ out;
            {TJitOpCount[f], v1, v2}
        ];
        TContextDestroy[ctx];
        restore[];
        result === {3, {648., 648., 648., 648.},
                    {1296., 1296., 1296., 1296.}}
    ],
    True,
    TestID -> "metal/tjit-autotune-captures-producer-chain"
]

VerificationTest[
    (* Non-diagnostic Conv2D should be able to use the generic
       TileUop Metal reduction path for the im2col-fused scalar graph. *)
    TInit[]; TReset[];
    Module[{ctx = TContextNew["metal"], result, oldSpecialized, oldTile,
            restore, xHost, wHost, expected},
        oldSpecialized = Environment["THVM_METAL_SPECIALIZED"];
        oldTile = Environment["THVM_TILE"];
        restore[] := (
            If[StringQ[oldSpecialized],
                SetEnvironment["THVM_METAL_SPECIALIZED" -> oldSpecialized],
                SetEnvironment["THVM_METAL_SPECIALIZED" -> ""]];
            If[StringQ[oldTile],
                SetEnvironment["THVM_TILE" -> oldTile],
                SetEnvironment["THVM_TILE" -> ""]]
        );
        SetEnvironment["THVM_METAL_SPECIALIZED" -> "0"];
        SetEnvironment["THVM_TILE" -> "1"];
        If[ ctx === 0, restore[]; Return[True]];
        xHost = ConstantArray[1., {2, 6, 6}];
        wHost = N @ ArrayReshape[Range[54], {3, 2, 3, 3}];
        expected = Table[
            Total @ Flatten @ wHost[[co, All, All, All]],
            {co, 3}, {oh, 1, 4}, {ow, 1, 4}];
        result = TInContext[ctx,
            x = TTensorCreate @ NumericArray[xHost, "Real32"];
            w = TTensorCreate @ NumericArray[wHost, "Real32"];
            b = TZeros[{3}];
            out = TRealize @ TConv2D[TReLU[x], w, b];
            {Round[Normal @ TTensorData[out], 0.001],
             MemberQ[Table[TKernelDispatchKind[k],
                 {k, 1, TKernelCount[] - 1}], "metal-conv"]}
        ];
        TContextDestroy[ctx];
        restore[];
        result === {expected, False}
    ],
    True,
    TestID -> "metal/f32-conv2d-generic-tile-reduce"
]

VerificationTest[
    (* Conv2D over a produced activation should route through the
       opt-in direct Metal conv diagnostic shader for the im2col-fused
       scalar graph. *)
    TInit[]; TReset[];
    Module[{ctx = TContextNew["metal"], result, oldSpecialized, restore,
            xHost, wHost, expected},
        oldSpecialized = Environment["THVM_METAL_SPECIALIZED"];
        restore[] := If[StringQ[oldSpecialized],
            SetEnvironment["THVM_METAL_SPECIALIZED" -> oldSpecialized],
            SetEnvironment["THVM_METAL_SPECIALIZED" -> ""]];
        SetEnvironment["THVM_METAL_SPECIALIZED" -> "1"];
        If[ ctx === 0, restore[]; Return[True]];
        xHost = ConstantArray[1., {2, 6, 6}];
        wHost = N @ ArrayReshape[Range[54], {3, 2, 3, 3}];
        expected = Table[
            Total @ Flatten @ wHost[[co, All, All, All]],
            {co, 3}, {oh, 1, 4}, {ow, 1, 4}];
        result = TInContext[ctx,
            x = TTensorCreate @ NumericArray[xHost, "Real32"];
            w = TTensorCreate @ NumericArray[wHost, "Real32"];
            b = TZeros[{3}];
            out = TRealize @ TConv2D[TReLU[x], w, b];
            kinds = Table[TKernelDispatchKind[k], {k, 1, TKernelCount[] - 1}];
            {Round[Normal @ TTensorData[out], 0.001],
             MemberQ[kinds, "metal-conv"]}
        ];
        TContextDestroy[ctx];
        restore[];
        result === {expected, True}
    ],
    True,
    TestID -> "metal/f32-conv2d-direct-produced-activation"
]

VerificationTest[
    (* i32 elementwise mul. *)
    TInit[]; TReset[];
    Module[{ctx = TContextNew["metal"], result},
        If[ ctx === 0, Return[True]];
        result = TInContext[ctx,
            a = TTensorCreate @ NumericArray[{2, 3, 4, 5}, "Integer32"];
            b = TTensorCreate @ NumericArray[{2, 2, 2, 2}, "Integer32"];
            Normal @ TTensorData @ TRealize[a * b]
        ];
        TContextDestroy[ctx];
        result === {4, 6, 8, 10} || ctx === 0
    ],
    True,
    TestID -> "metal/i32-mul"
]

VerificationTest[
    (* i32 reduce-sum. *)
    TInit[]; TReset[];
    Module[{ctx = TContextNew["metal"], result},
        If[ ctx === 0, Return[True]];
        result = TInContext[ctx,
            a = TTensorCreate @ NumericArray[{1, 2, 3, 4, 5}, "Integer32"];
            Normal @ TTensorData @ TRealize @ TUOpReduce[a, 0, "SUM"]
        ];
        TContextDestroy[ctx];
        result === {15} || ctx === 0
    ],
    True,
    TestID -> "metal/i32-reduce-sum"
]

VerificationTest[
    (* Non-supported dtype (i8) on a Metal context: dispatch returns
       -1 (Metal has no i8 shader variants yet) so the kernel never
       fires.  This test confirms the failure is graceful (no
       crash) rather than producing garbage.  Users who want i8 on
       a Metal context should CAST to i32 / f32 first. *)
    TInit[]; TReset[];
    Module[{ctx = TContextNew["metal"], result},
        If[ ctx === 0, Return[True]];
        result = TInContext[ctx,
            a = TTensorCreate @ NumericArray[{1, 2, 3, 4}, "Integer8"];
            Normal @ TTensorData[a]
        ];
        TContextDestroy[ctx];
        result === {1, 2, 3, 4}
    ],
    True,
    TestID -> "metal/i8-input-roundtrip"
]
