(* kernel_opts.wlt -- TOpt + TKernelOpts + TKernelApplyOpt surface +
   the C-side KernelAxes scaffold under it (Phase 16).

   Covers:
     - construction + structural form of TOpt
     - default axes for elementwise + reduce-tail kernels
     - axes_apply_opt mutations: UNROLL/UPCAST split, GLOBAL mark, SWAP swap
     - validation (out-of-range axis, non-divisible arg, reserved/unknown op)
     - codegen: post-UNROLL TKernelSource contains the loop pragma
     - JIT cache key: distinct dylib hash post-opt
     - correctness: realize result identical pre/post UNROLL
     - MakeBoxes summary boxes render for both TOpt and TKernelOpts
     - applied_opts log reflects every TKernelApplyOpt invocation *)

(* === TOpt construction === *)

VerificationTest[
    TInit[];
    op = TOpt["UNROLL", 1, 4];
    {Head[op], op[[1]], op[[2]], op[[3]]},
    {TOpt, "UNROLL", 1, 4},
    TestID -> "kernel-opts/topt-construct"
]

(* === default axes for an elementwise kernel: all LOOP, no REDUCE === *)

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1., 2., 3., 4.}, "Real32"];
    TRealize @ TUOpMul[a, a];
    kid = TKernelCount[] - 1;
    First @ TKernelOpts[kid],
    KeyValuePattern[{
        "Kid"       -> kid,
        "AxisTypes" -> {"LOOP"},
        "FullShape" -> {4},
        "Applied"   -> {}
    }],
    SameTest -> MatchQ,
    TestID -> "kernel-opts/default-axes-elementwise"
]

(* === default axes for a reduce-tail kernel: LOOP + trailing REDUCE === *)

VerificationTest[
    TInit[];
    xT = TTensorCreate @ N @ Range[12];
    TRealize @ TUOpReduce[xT, 0, "SUM"];
    kid = TKernelCount[] - 1;
    {First[TKernelOpts[kid]]["AxisTypes"], First[TKernelOpts[kid]]["FullShape"]},
    {{"LOOP", "REDUCE"}, {1, 12}},
    TestID -> "kernel-opts/default-axes-reduce"
]

(* === UNROLL on a reduce axis: splits 12 -> REDUCE(3) + UNROLL(4) === *)

VerificationTest[
    TInit[];
    xT = TTensorCreate @ N @ Range[12];
    TRealize @ TUOpReduce[xT, 0, "SUM"];
    kid = TKernelCount[] - 1;
    res = TKernelApplyOpt[kid, TOpt["UNROLL", 1, 4]];
    a = First[res];
    {a["AxisTypes"], a["FullShape"], a["Applied"]},
    {{"LOOP", "REDUCE", "UNROLL"}, {1, 3, 4}, {TOpt["UNROLL", 1, 4]}},
    TestID -> "kernel-opts/apply-unroll-splits-reduce"
]

(* === UPCAST on an output axis: splits 8 -> LOOP(2) + UPCAST(4) === *)

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[Table[N[i], {i, 8}], "Real32"];
    TRealize @ TUOpMul[a, a];
    kid = TKernelCount[] - 1;
    res = TKernelApplyOpt[kid, TOpt["UPCAST", 0, 4]];
    {First[res]["AxisTypes"], First[res]["FullShape"]},
    {{"LOOP", "UPCAST"}, {2, 4}},
    TestID -> "kernel-opts/apply-upcast-splits-output"
]

(* === LOCAL + GLOBAL: split output axis, then mark remaining LOOP as GLOBAL === *)

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[Table[N[i], {i, 8}], "Real32"];
    TRealize @ TUOpMul[a, a];
    kid = TKernelCount[] - 1;
    TKernelApplyOpt[kid, TOpt["LOCAL", 0, 4]];
    res = TKernelApplyOpt[kid, TOpt["GLOBAL", 0, 2]];
    {First[res]["AxisTypes"], First[res]["FullShape"],
     First[res]["Applied"]},
    {{"GLOBAL", "LOCAL"}, {2, 4},
     {TOpt["LOCAL", 0, 4], TOpt["GLOBAL", 0, 2]}},
    TestID -> "kernel-opts/apply-local-global-output"
]

(* === SWAP exchanges two axes (in-place; no new axis) === *)

VerificationTest[
    TInit[];
    xT = TTensorCreate @ NumericArray[Table[N[i], {i, 16}], "Real32"];
    TRealize @ TUOpMul[xT, xT];
    kid = TKernelCount[] - 1;
    TKernelApplyOpt[kid, TOpt["UPCAST", 0, 4]];      (* split LOOP(16) -> LOOP(4) + UPCAST(4) *)
    pre  = First @ TKernelOpts[kid];
    res  = TKernelApplyOpt[kid, TOpt["SWAP", 0, 1]];
    post = First[res];
    {pre["AxisTypes"], post["AxisTypes"]},
    {{"LOOP", "UPCAST"}, {"UPCAST", "LOOP"}},
    TestID -> "kernel-opts/apply-swap-exchanges-axes"
]

(* === Validation: out-of-range axis returns Failure === *)

VerificationTest[
    TInit[];
    xT = TTensorCreate @ N @ Range[8];
    TRealize @ TUOpReduce[xT, 0, "SUM"];
    kid = TKernelCount[] - 1;
    res = TKernelApplyOpt[kid, TOpt["UNROLL", 99, 4]];
    Head[res],
    Failure,
    TestID -> "kernel-opts/validation-axis-out-of-range"
]

(* === Validation: non-divisible split factor returns Failure === *)

VerificationTest[
    TInit[];
    xT = TTensorCreate @ N @ Range[10];        (* axis_size = 10, not divisible by 4 *)
    TRealize @ TUOpReduce[xT, 0, "SUM"];
    kid = TKernelCount[] - 1;
    res = TKernelApplyOpt[kid, TOpt["UNROLL", 1, 4]];
    Head[res],
    Failure,
    TestID -> "kernel-opts/validation-arg-doesnt-divide"
]

(* === Validation: unknown opt name returns Failure === *)

VerificationTest[
    TInit[];
    xT = TTensorCreate @ N @ Range[8];
    TRealize @ TUOpReduce[xT, 0, "SUM"];
    kid = TKernelCount[] - 1;
    res = TKernelApplyOpt[kid, TOpt["BOGUS", 0, 1]];
    Head[res],
    Failure,
    TestID -> "kernel-opts/validation-unknown-op"
]

(* === Validation: reserved no-op opts are rejected until renderers consume them === *)

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[Table[N[i], {i, 8}], "Real32"];
    TRealize @ TUOpMul[a, a];
    kid = TKernelCount[] - 1;
    {Head @ TKernelApplyOpt[kid, TOpt["PADTO", 0, 16]],
     Head @ TKernelApplyOpt[kid, TOpt["NOLOCALS", 0, 1]],
     Head @ TKernelApplyOpt[kid, TOpt["TC", 0, 1]],
     First[TKernelOpts[kid]]["Applied"]},
    {Failure, Failure, Failure, {}},
    TestID -> "kernel-opts/validation-reserved-opts-rejected"
]

(* === Codegen: post-UNROLL TKernelSource contains clang loop pragma === *)

VerificationTest[
    TInit[];
    xT = TTensorCreate @ N @ Range[16];
    TRealize @ TUOpReduce[xT, 0, "SUM"];
    kid = TKernelCount[] - 1;
    pre  = TKernelSource[kid, "C"];
    TKernelApplyOpt[kid, TOpt["UNROLL", 1, 4]];
    post = TKernelSource[kid, "C"];
    {StringContainsQ[pre,  "#pragma clang loop unroll_count"],
     StringContainsQ[post, "#pragma clang loop unroll_count(4)"]},
    {False, True},
    TestID -> "kernel-opts/codegen-unroll-emits-pragma"
]

(* === JIT cache key: distinct dylib hash post-opt === *)

VerificationTest[
    TInit[];
    xT = TTensorCreate @ N @ Range[16];
    TRealize @ TUOpReduce[xT, 0, "SUM"];
    kid = TKernelCount[] - 1;
    pre  = TKernelJitDylibPath[kid];
    TKernelApplyOpt[kid, TOpt["UNROLL", 1, 4]];
    TRealize @ TUOpReduce[xT, 0, "SUM"];          (* re-fire to rebuild *)
    post = TKernelJitDylibPath[kid];
    pre =!= post,
    True,
    TestID -> "kernel-opts/jit-key-folds-in-opts"
]

(* === Correctness: realize result identical pre/post UNROLL === *)

VerificationTest[
    TInit[];
    xT  = TTensorCreate @ N @ Range[16];
    pre = First @ Normal @ TTensorData @ TRealize @ TUOpReduce[xT, 0, "SUM"];
    kid = TKernelCount[] - 1;
    TKernelApplyOpt[kid, TOpt["UNROLL", 1, 4]];
    post = First @ Normal @ TTensorData @ TRealize @ TUOpReduce[xT, 0, "SUM"];
    Abs[pre - post],
    _ ? (# < 1.0*^-4 &),
    SameTest -> MatchQ,
    TestID -> "kernel-opts/correctness-preserved-after-unroll"
]

(* === Summary box: TOpt renders an InterpretationBox === *)

VerificationTest[
    !FreeQ[ToBoxes[TOpt["UNROLL", 1, 4]], InterpretationBox],
    True,
    TestID -> "kernel-opts/topt-makeboxes-renders-summary"
]

(* === Summary box: TKernelOpts renders an InterpretationBox === *)

VerificationTest[
    TInit[];
    xT = TTensorCreate @ N @ Range[8];
    TRealize @ TUOpReduce[xT, 0, "SUM"];
    !FreeQ[ToBoxes[TKernelOpts[TKernelCount[] - 1]], InterpretationBox],
    True,
    TestID -> "kernel-opts/tkernelopts-makeboxes-renders-summary"
]

(* === proposer: shape-heuristic candidate TOpts === *)

VerificationTest[
    TInit[];
    xT = TTensorCreate @ N @ Range[32];        (* divisible by 16/8/4/2 *)
    TRealize @ TUOpReduce[xT, 0, "SUM"];
    TKernelProposed[TKernelCount[] - 1],
    {TOpt["UNROLL", 1, 16], TOpt["UNROLL", 1, 8],
     TOpt["UNROLL", 1, 4],  TOpt["UNROLL", 1, 2]},
    TestID -> "kernel-opts/propose-reduce-unroll-divisors"
]

VerificationTest[
    TInit[];
    xT = TTensorCreate @ N @ Range[12];        (* divisible by 4, 2 only *)
    TRealize @ TUOpReduce[xT, 0, "SUM"];
    TKernelProposed[TKernelCount[] - 1],
    {TOpt["UNROLL", 1, 4], TOpt["UNROLL", 1, 2]},
    TestID -> "kernel-opts/propose-reduce-unroll-partial-divisors"
]

VerificationTest[
    (* Elementwise kernel proposes UPCAST on the output axis at all
       divisors of the selected output axis (factor in {2,4,8,16}). *)
    TInit[];
    a = TTensorCreate @ N @ Range[16];         (* output_numel = 16 *)
    TRealize @ TUOpMul[a, a];
    TKernelProposed[TKernelCount[] - 1],
    {TOpt["UPCAST", 0, 16], TOpt["UPCAST", 0, 8],
     TOpt["UPCAST", 0, 4],  TOpt["UPCAST", 0, 2]},
    TestID -> "kernel-opts/propose-elementwise-upcast"
]

VerificationTest[
    (* Ranked elementwise outputs must not propose factors that
       divide total numel but fail the selected axis split. *)
    TInit[];
    a = TTensorCreate @ NumericArray[ConstantArray[1., {20, 2, 2}], "Real32"];
    TRealize @ TUOpMul[a, a];
    TKernelProposed[TKernelCount[] - 1],
    {TOpt["UPCAST", 0, 4], TOpt["UPCAST", 0, 2]},
    TestID -> "kernel-opts/propose-ranked-upcast-axis-divisors"
]

VerificationTest[
    TInit[];
    xT = TTensorCreate @ N @ Range[7];         (* prime axis: no divisors > 1 *)
    TRealize @ TUOpReduce[xT, 0, "SUM"];
    TKernelProposed[TKernelCount[] - 1],
    {},
    TestID -> "kernel-opts/propose-prime-axis-empty"
]

(* === autotune: bench candidates, apply the winner === *)

VerificationTest[
    TInit[];
    xT = TTensorCreate @ N @ Range[16];
    TRealize @ TUOpReduce[xT, 0, "SUM"];
    kid = TKernelCount[] - 1;
    res = TKernelAutotune[kid];
    Head[res],
    TKernelOpts,
    TestID -> "kernel-opts/autotune-returns-tkernelopts"
]

VerificationTest[
    (* Correctness: post-autotune realize value matches a no-opt
       baseline (within f32 reduction-order tolerance). *)
    TInit[];
    xT  = TTensorCreate @ NumericArray[Table[N[i / 100.0], {i, 256}], "Real32"];
    pre = First @ Normal @ TTensorData @ TRealize @ TUOpReduce[xT, 0, "SUM"];
    kid = TKernelCount[] - 1;
    TKernelAutotune[kid];
    post = First @ Normal @ TTensorData @ TRealize @ TUOpReduce[xT, 0, "SUM"];
    Abs[pre - post] < 1.0*^-3,
    True,
    TestID -> "kernel-opts/autotune-preserves-correctness"
]

VerificationTest[
    (* Disk cache: first autotune writes a winner; the second runtime
       session with the same program shape should replay it without
       running benchmark dispatches. *)
    Module[{oldCache, oldDisable, oldDir, oldBackend, oldTile, oldRuns,
            dir, restore, files, before, after},
        oldCache   = Environment["AUTOTUNE_CACHE"];
        oldDisable = Environment["AUTOTUNE_DISABLE"];
        oldDir     = Environment["AUTOTUNE_CACHE_DIR"];
        oldBackend = Environment["DEV"];
        oldTile    = Environment["THVM_TILE"];
        oldRuns    = Environment["BEAM_RUNS"];
        dir = CreateDirectory @ FileNameJoin[{
            $TemporaryDirectory,
            "thvm-autotune-cache-" <> ToString[$ProcessID] <> "-" <>
                IntegerString[RandomInteger[10^9]]
        }];
        restore[] := (
            If[StringQ[oldCache],
                SetEnvironment["AUTOTUNE_CACHE" -> oldCache],
                SetEnvironment["AUTOTUNE_CACHE" -> ""]];
            If[StringQ[oldDisable],
                SetEnvironment["AUTOTUNE_DISABLE" -> oldDisable],
                SetEnvironment["AUTOTUNE_DISABLE" -> ""]];
            If[StringQ[oldDir],
                SetEnvironment["AUTOTUNE_CACHE_DIR" -> oldDir],
                SetEnvironment["AUTOTUNE_CACHE_DIR" -> ""]];
            If[StringQ[oldBackend],
                SetEnvironment["DEV" -> oldBackend],
                SetEnvironment["DEV" -> ""]];
            If[StringQ[oldTile],
                SetEnvironment["THVM_TILE" -> oldTile],
                SetEnvironment["THVM_TILE" -> ""]];
            If[StringQ[oldRuns],
                SetEnvironment["BEAM_RUNS" -> oldRuns],
                SetEnvironment["BEAM_RUNS" -> ""]]
        );
        Internal`WithLocalSettings[
            SetEnvironment["AUTOTUNE_CACHE" -> "1"];
            SetEnvironment["AUTOTUNE_DISABLE" -> ""];
            SetEnvironment["AUTOTUNE_CACHE_DIR" -> dir];
            SetEnvironment["DEV" -> ""];
            SetEnvironment["THVM_TILE" -> ""];
            SetEnvironment["BEAM_RUNS" -> ""],

            TInit[];
            xT = TTensorCreate @ NumericArray[Range[32], "Real32"];
            TRealize @ TUOpReduce[xT, 0, "SUM"];
            TKernelAutotune[TKernelCount[] - 1];
            files = FileNames["*.json", dir, Infinity];

            TInit[];
            xT = TTensorCreate @ NumericArray[Range[32], "Real32"];
            TRealize @ TUOpReduce[xT, 0, "SUM"];
            kid = TKernelCount[] - 1;
            before = TKernelDispatchCount[kid];
            TKernelAutotune[kid];
            after = TKernelDispatchCount[kid],

            restore[];
            Quiet @ DeleteDirectory[dir, DeleteContents -> True]
        ];
        {Length[files] > 0, before === after}
    ],
    {True, True},
    TestID -> "kernel-opts/autotune-disk-cache-replays-winner"
]

VerificationTest[
    (* Elementwise UPCAST: emits clang loop pragma on the output
       loop, preserves the computed result.  *)
    TInit[];
    xT  = TTensorCreate @ NumericArray[Table[N[i + 0.5], {i, 32}], "Real32"];
    yT  = TTensorCreate @ NumericArray[Table[N[i * 0.1], {i, 32}], "Real32"];
    pre = Normal @ TTensorData @ TRealize @ TUOpAdd[TUOpMul[xT, yT], TUOpMul[TUOpConst[2.0], xT]];
    kid = TKernelCount[] - 1;
    TKernelApplyOpt[kid, TOpt["UPCAST", 0, 8]];
    src  = TKernelSource[kid, "C"];
    post = Normal @ TTensorData @ TRealize @ TUOpAdd[TUOpMul[xT, yT], TUOpMul[TUOpConst[2.0], xT]];
    {StringContainsQ[src, "#pragma clang loop unroll_count(8)"],
     StringContainsQ[src, "0x40000000"],
     AllTrue[Thread[Abs[pre - post] < 1.0*^-5], TrueQ]},
    {True, True, True},
    TestID -> "kernel-opts/codegen-upcast-pragma-correct"
]

VerificationTest[
    (* Reduce-tail kernel: proposer should NOT mix UPCAST with the
       reduce-axis UNROLL candidates.  Today's heuristic skips
       UPCAST entirely for reduce-tail kernels (axis_size > 0). *)
    TInit[];
    xT = TTensorCreate @ N @ Range[16];
    TRealize @ TUOpReduce[xT, 0, "SUM"];
    kid = TKernelCount[] - 1;
    AllTrue[TKernelProposed[kid], MatchQ[TOpt["UNROLL", _, _]]],
    True,
    TestID -> "kernel-opts/propose-reduce-no-upcast"
]

VerificationTest[
    (* Multi-axis tile-lowered elementwise kernels should still get
       LOCAL candidates.  The renderer flattens the axes when no
       LOCAL/GLOBAL split is present, so proposer coverage should not
       be restricted to rank-1 outputs. *)
    Module[{oldBackend, oldTile, restore, kid, props},
        oldBackend = Environment["DEV"];
        oldTile    = Environment["THVM_TILE"];
        restore[] := (
            If[StringQ[oldBackend],
                SetEnvironment["DEV" -> oldBackend],
                SetEnvironment["DEV" -> ""]];
            If[StringQ[oldTile],
                SetEnvironment["THVM_TILE" -> oldTile],
                SetEnvironment["THVM_TILE" -> ""]]
        );
        Internal`WithLocalSettings[
            SetEnvironment["DEV" -> "metal"];
            SetEnvironment["THVM_TILE" -> "1"],
            TInit[];
            a = TTensorCreate @ NumericArray[ConstantArray[1., {4, 4}], "Real32"];
            b = TTensorCreate @ NumericArray[ConstantArray[2., {4, 4}], "Real32"];
            TRealize[a + b];
            kid = TKernelCount[] - 1;
            props = TKernelProposed[kid],
            restore[]
        ];
        MemberQ[props, TOpt["LOCAL", 0, 4]]
    ],
    True,
    TestID -> "kernel-opts/metal-multi-axis-tile-local-proposal"
]

VerificationTest[
    (* Movement-heavy kernels can have leading unit axes and non-
       divisible middle axes.  The proposer should skip those and
       offer LOCAL candidates on a later splittable loop axis. *)
    Module[{oldBackend, oldTile, restore, kid, props},
        oldBackend = Environment["DEV"];
        oldTile    = Environment["THVM_TILE"];
        restore[] := (
            If[StringQ[oldBackend],
                SetEnvironment["DEV" -> oldBackend],
                SetEnvironment["DEV" -> ""]];
            If[StringQ[oldTile],
                SetEnvironment["THVM_TILE" -> oldTile],
                SetEnvironment["THVM_TILE" -> ""]]
        );
        Internal`WithLocalSettings[
            SetEnvironment["DEV" -> "metal"];
            SetEnvironment["THVM_TILE" -> "1"],
            TInit[];
            a = TTensorCreate @ NumericArray[
                ConstantArray[1., {1, 25, 32}], "Real32"];
            b = TTensorCreate @ NumericArray[
                ConstantArray[2., {1, 25, 32}], "Real32"];
            TRealize[a + b];
            kid = TKernelCount[] - 1;
            props = TKernelProposed[kid],
            restore[]
        ];
        Take[props, UpTo[3]]
    ],
    {TOpt["LOCAL", 2, 32], TOpt["LOCAL", 2, 16],
     TOpt["LOCAL", 2, 8]},
    TestID -> "kernel-opts/metal-local-proposal-skips-unsplittable-axes"
]

VerificationTest[
    (* A LOCAL/GLOBAL split on one output axis must not make remaining
       serial LOOP axes fall back to the per-program Metal JIT.  This
       shape mirrors the beautiful_mnist elementwise kernels that carry
       one bound threadgroup axis plus extra serial tensor axes. *)
    Module[{oldBackend, oldTile, restore, a, b, out, kid, opts, kind, data},
        oldBackend = Environment["DEV"];
        oldTile    = Environment["THVM_TILE"];
        restore[] := (
            If[StringQ[oldBackend],
                SetEnvironment["DEV" -> oldBackend],
                SetEnvironment["DEV" -> ""]];
            If[StringQ[oldTile],
                SetEnvironment["THVM_TILE" -> oldTile],
                SetEnvironment["THVM_TILE" -> ""]]
        );
        Internal`WithLocalSettings[
            SetEnvironment["DEV" -> "metal"];
            SetEnvironment["THVM_TILE" -> "1"],
            TInit[];
            a = TTensorCreate @ NumericArray[ConstantArray[1., {2, 4, 3}], "Real32"];
            b = TTensorCreate @ NumericArray[ConstantArray[2., {2, 4, 3}], "Real32"];
            out = TRealize[a + b];
            kid = TKernelCount[] - 1;
            TKernelApplyOpt[kid, TOpt["LOCAL", 1, 4]];
            opts = First @ TKernelApplyOpt[kid, TOpt["GLOBAL", 0, 2]];
            TKernel[kid][];
            kind = TKernelDispatchKind[kid];
            data = Normal @ TTensorData[out],
            restore[]
        ];
        {opts["AxisTypes"], opts["FullShape"], kind, data}
    ],
    {{"GLOBAL", "LOOP", "LOCAL", "LOOP"}, {2, 1, 4, 3},
     "metal-tile", ConstantArray[3., {2, 4, 3}]},
    TestID -> "kernel-opts/metal-local-global-with-serial-loop-dispatch"
]

VerificationTest[
    (* Wide Metal elementwise ADD trees must split before they exceed
       the direct MSL buffer-argument budget.  Otherwise they drop to
       the per-op Metal interpreter even though each split subtree is
       tile-renderable. *)
    Module[{oldBackend, oldTile, oldCap, restore, xs, out, rows, kinds},
        oldBackend = Environment["DEV"];
        oldTile    = Environment["THVM_TILE"];
        oldCap     = Environment["THVM_METAL_FUSION_MAX_INPUTS"];
        restore[] := (
            If[StringQ[oldBackend],
                SetEnvironment["DEV" -> oldBackend],
                SetEnvironment["DEV" -> ""]];
            If[StringQ[oldTile],
                SetEnvironment["THVM_TILE" -> oldTile],
                SetEnvironment["THVM_TILE" -> ""]];
            If[StringQ[oldCap],
                SetEnvironment["THVM_METAL_FUSION_MAX_INPUTS" -> oldCap],
                SetEnvironment["THVM_METAL_FUSION_MAX_INPUTS" -> ""]]
        );
        Internal`WithLocalSettings[
            SetEnvironment["DEV" -> "metal"];
            SetEnvironment["THVM_TILE" -> "1"];
            SetEnvironment["THVM_METAL_FUSION_MAX_INPUTS" -> "30"],
            TInit[];
            xs = Table[
                TTensorCreate @ NumericArray[ConstantArray[N[i], {4}], "Real32"],
                {i, 34}];
            out = TRealize @ Fold[TUOpAdd, First[xs], Rest[xs]];
            rows = TKernelTable[];
            kinds = Table[TKernelDispatchKind[k], {k, 1, TKernelCount[] - 1}],
            restore[]
        ];
        {Length[rows] >= 2,
         Max[rows[[All, 1]]] <= 30,
         FreeQ[kinds, "metal-op"],
         Round[Normal @ TTensorData[out], 0.001]}
    ],
    {True, True, True, ConstantArray[595., 4]},
    TestID -> "kernel-opts/metal-wide-add-tree-splits-before-arg-limit"
]

VerificationTest[
    (* The fan-in cap also has to split movement-wrapped ADD trees.
       beautiful-mnist Adam updates can put RESHAPE wrappers between
       the oversized ADD subtree and the realized boundary; splitting
       only ADD/MUL children leaves a >30-input tile graph that falls
       through to metal-op. *)
    Module[{oldBackend, oldTile, oldCap, restore, xs, ys, out, rows, kinds},
        oldBackend = Environment["DEV"];
        oldTile    = Environment["THVM_TILE"];
        oldCap     = Environment["THVM_METAL_FUSION_MAX_INPUTS"];
        restore[] := (
            If[StringQ[oldBackend],
                SetEnvironment["DEV" -> oldBackend],
                SetEnvironment["DEV" -> ""]];
            If[StringQ[oldTile],
                SetEnvironment["THVM_TILE" -> oldTile],
                SetEnvironment["THVM_TILE" -> ""]];
            If[StringQ[oldCap],
                SetEnvironment["THVM_METAL_FUSION_MAX_INPUTS" -> oldCap],
                SetEnvironment["THVM_METAL_FUSION_MAX_INPUTS" -> ""]]
        );
        Internal`WithLocalSettings[
            SetEnvironment["DEV" -> "metal"];
            SetEnvironment["THVM_TILE" -> "1"];
            SetEnvironment["THVM_METAL_FUSION_MAX_INPUTS" -> "30"],
            TInit[];
            xs = Table[
                TTensorCreate @ NumericArray[
                    ConstantArray[N[i], {2, 2}], "Real32"],
                {i, 34}];
            ys = TUOpReshape[#, {4}] & /@ xs;
            out = TRealize @ Fold[TUOpAdd, First[ys], Rest[ys]];
            rows = TKernelTable[];
            kinds = Table[TKernelDispatchKind[k], {k, 1, TKernelCount[] - 1}],
            restore[]
        ];
        {Length[rows] >= 2,
         Max[rows[[All, 1]]] <= 30,
         FreeQ[kinds, "metal-op"],
         Round[Normal @ TTensorData[out], 0.001]}
    ],
    {True, True, True, ConstantArray[595., 4]},
    TestID -> "kernel-opts/metal-movement-wrapped-wide-add-splits"
]

VerificationTest[
    (* A large broadcast view with multiple consumers should not be
       materialized as a global Metal buffer when each consumer can
       inline the EXPAND address expression. *)
    Module[{oldBackend, oldTile, oldInline, restore, base, shared,
            out, data, hasExpandKernel},
        oldBackend = Environment["DEV"];
        oldTile    = Environment["THVM_TILE"];
        oldInline  = Environment["THVM_INLINE_MULTI_CONSUMER_EXPAND"];
        restore[] := (
            If[StringQ[oldBackend],
                SetEnvironment["DEV" -> oldBackend],
                SetEnvironment["DEV" -> ""]];
            If[StringQ[oldTile],
                SetEnvironment["THVM_TILE" -> oldTile],
                SetEnvironment["THVM_TILE" -> ""]];
            If[StringQ[oldInline],
                SetEnvironment["THVM_INLINE_MULTI_CONSUMER_EXPAND" -> oldInline],
                SetEnvironment["THVM_INLINE_MULTI_CONSUMER_EXPAND" -> ""]]
        );
        Internal`WithLocalSettings[
            SetEnvironment["DEV" -> "metal"];
            SetEnvironment["THVM_TILE" -> "1"];
            SetEnvironment["THVM_INLINE_MULTI_CONSUMER_EXPAND" -> ""],
            TInit[];
            base = TTensorCreate @ NumericArray[N @ Range[4], "Real32"];
            shared = TUOpExpand[TUOpReshape[base, {1, 4}], {16, 4}];
            out = TRealize[
                TUOpReduce[shared, 0, "SUM"] +
                TUOpReduce[shared * TUOpConst[2.0], 0, "SUM"]];
            data = Round[Normal @ TTensorData[out], 0.001];
            hasExpandKernel = AnyTrue[Range[1, TKernelCount[] - 1],
                Function[kid,
                    TKernelInfo[kid]["output_numel"] === 64 &&
                    Counts[TKernelInfo[kid]["program"][[All, "opcode"]]] ===
                        <|"RESHAPE" -> 1, "EXPAND" -> 1|>]],
            restore[]
        ];
        {data, hasExpandKernel}
    ],
    {{48., 96., 144., 192.}, False},
    TestID -> "kernel-opts/metal-inline-large-multiconsumer-expand"
]

(* === TKernelVariants[kid]: bench-and-report every candidate
       without committing any opt.  Slot 0 is baseline; rest are
       proposed candidates with measured WallUs. *)

VerificationTest[
    TInit[];
    xT = TTensorCreate @ N @ Range[1024];
    yT = TTensorCreate @ N @ Range[1024];
    TRealize @ TUOpAdd[TUOpMul[xT, yT], TUOpMul[TUOpConst[2.0], xT]];
    kid = TKernelCount[] - 1;
    before = TKernelDispatchCount[kid];
    res = TKernelVariants[kid];
    after = TKernelDispatchCount[kid];
    {(* Slot 0 is baseline (Opt -> None). *)
     res[[1, 1]]["Opt"],
     (* Subsequent slots match TKernelProposed candidates. *)
     #["Opt"] & /@ res[[2 ;;, 1]],
     (* Every variant has a measured WallUs. *)
     AllTrue[ #["WallUs"] & /@ res[[All, 1]], # >= 0 &],
     (* Bench fires must bypass the per-realize fire-generation memo. *)
     after > before,
     (* Inspect-only: axes left at baseline (Applied empty). *)
     First[TKernelOpts[kid]]["Applied"]},
    {None, TKernelProposed[kid], True, True, {}},
    TestID -> "kernel-opts/variants-inspect-only"
]

VerificationTest[
    !FreeQ[ToBoxes[TKernelVariant[<|"Kid" -> 1, "Opt" -> None, "WallUs" -> 0|>]],
        InterpretationBox],
    True,
    TestID -> "kernel-opts/tkernelvariant-makeboxes-renders-summary"
]

(* === TKernelAutotuneAll[] sweeps every live kid, returns
       Association kid -> TKernelOpts.  Useful as a one-shot
       pre-warm before a training loop. *)

VerificationTest[
    TInit[];
    aT = TTensorCreate @ N @ Range[16];
    bT = TTensorCreate @ N @ Range[32];
    TRealize @ TUOpReduce[bT, 0, "SUM"];
    TRealize @ TUOpMul[aT, aT];
    res = TKernelAutotuneAll[];
    {AssociationQ[res],
     Length[res],
     AllTrue[Values[res], MatchQ[_TKernelOpts]]},
    {True, TKernelCount[] - 1, True},
    TestID -> "kernel-opts/autotune-all-sweeps-every-kid"
]

(* === fire-time autotune trigger: env opt-in via AUTOTUNE.
       Default off so existing users don't pay surprise bench costs;
       on, every new program shape gets autotuned at first fire. *)

VerificationTest[
    (* Without env: no auto-fire.  Single dispatch (the user's
       TRealize) and Applied stays empty. *)
    TInit[];
    xT = TTensorCreate @ N @ Range[256];
    yT = TTensorCreate @ N @ Range[256, 1, -1];
    TRealize @ TUOpAdd[TUOpMul[xT, yT], TUOpMul[TUOpConst[2.0], xT]];
    kid = TKernelCount[] - 1;
    {TKernelDispatchCount[kid],
     First[TKernelOpts[kid]]["Applied"]},
    {1, {}},
    TestID -> "kernel-opts/auto-fire-disabled-by-default"
]

VerificationTest[
    (* When fire-time autotune runs during the first TJit capture,
       its internal benchmark fires must NOT be recorded into the
       replay sequence.  Only the user's actual reduce dispatch
       belongs in the capture. *)
    Module[{oldAuto, oldRuns, oldBackend, restore, f, opCount},
        oldAuto    = Environment["AUTOTUNE"];
        oldRuns    = Environment["BEAM_RUNS"];
        oldBackend = Environment["DEV"];
        restore[] := (
            If[StringQ[oldAuto],
                SetEnvironment["AUTOTUNE" -> oldAuto],
                SetEnvironment["AUTOTUNE" -> ""]];
            If[StringQ[oldRuns],
                SetEnvironment["BEAM_RUNS" -> oldRuns],
                SetEnvironment["BEAM_RUNS" -> ""]];
            If[StringQ[oldBackend],
                SetEnvironment["DEV" -> oldBackend],
                SetEnvironment["DEV" -> ""]]
        );
        Internal`WithLocalSettings[
            SetEnvironment["AUTOTUNE" -> "1"];
            SetEnvironment["BEAM_RUNS" -> "1"];
            SetEnvironment["DEV" -> ""],
            TInit[];
            xT = TTensorCreate @ NumericArray[Range[32], "Real32"];
            f = TJit[Function[{}, TRealize @ TUOpReduce[xT, 0, "SUM"]]];
            f[];
            opCount = TJitOpCount[f],
            restore[]
        ];
        opCount
    ],
    1,
    TestID -> "kernel-opts/tjit-autotune-bench-fires-not-captured"
]

VerificationTest[
    (* Full fused forwards can exceed the old 16-input inline capture
       cap.  A single high-input fused elementwise kernel should still
       record as one replay op. *)
    TInit[];
    xs = Table[
        TTensorCreate @ NumericArray[N @ Range[i, i + 3], "Real32"],
        {i, 20}];
    expr = Fold[TUOpAdd, First[xs], Rest[xs]];
    f = TJit[Function[{}, TRealize @ expr]];
    out = f[];
    TSet[xs[[1]], TTensorCreate @ NumericArray[N @ Range[100, 103], "Real32"]];
    f[];
    {TJitOpCount[f],
     Round[Normal @ TTensorData @ out, 0.001]},
    {1, Round[
        Normal @ (N @ Range[100, 103] + Total[Table[N @ Range[i, i + 3], {i, 2, 20}]]),
        0.001]},
    TestID -> "kernel-opts/tjit-captures-high-input-kernel"
]

VerificationTest[
    (* Capture introspection should expose enough per-dispatch shape
       metadata to compare THVM replay runs against tinygrad graph
       batches. *)
    TInit[];
    xT = TTensorCreate @ NumericArray[N @ Range[8], "Real32"];
    f = TJit[Function[{}, TRealize @ TUOpAdd[xT, xT]]];
    f[];
    ops = TJitCaptureOps[f];
    runs = TJitCaptureRuns[f];
    summary = TJitCaptureSummary[f];
    {TJitOpCount[f],
     Length[ops],
     First[ops]["OutputNumel"],
     First[ops]["OpCount"] > 0,
     summary["RunCount"],
     summary["DispatchRuns"],
     Length[summary["TopRuns"]]},
    {1, 1, 8, True, 1, {1}, 1},
    TestID -> "kernel-opts/tjit-capture-introspection"
]

VerificationTest[
    (* Rootless optimizer-style TJit steps may sink ASSIGN copies into
       the immediately preceding Metal tile producer.  Skipped ASSIGN
       rows should not split graph replay, and replay should still
       mutate the destination buffers correctly. *)
    Module[{oldBackend, oldTile, restore, w, x, y, f, summary, counters,
            wData, yData},
        oldBackend = Environment["DEV"];
        oldTile    = Environment["THVM_TILE"];
        restore[] := (
            If[StringQ[oldBackend],
                SetEnvironment["DEV" -> oldBackend],
                SetEnvironment["DEV" -> ""]];
            If[StringQ[oldTile],
                SetEnvironment["THVM_TILE" -> oldTile],
                SetEnvironment["THVM_TILE" -> ""]]
        );
        Internal`WithLocalSettings[
            SetEnvironment["DEV" -> "metal"];
            SetEnvironment["THVM_TILE" -> "1"],
            TInit[];
            w = TTensorCreate @ NumericArray[N @ Range[4], "Real32"];
            x = TTensorCreate @ NumericArray[ConstantArray[1., 4], "Real32"];
            y = TTensorCreate @ NumericArray[ConstantArray[0., 4], "Real32"];
            f = TJit[Function[{}, Module[{a, b},
                a = TAssign[w, w + x];
                b = TAssign[y, a * x];
                TRealize @ b;
                Null
            ]]];
            f[];
            summary = TJitCaptureSummary[f];
            THotCountersReset[];
            f[];
            counters = THotCounters[];
            wData = Round[Normal @ TTensorData[w], 0.001];
            yData = Round[Normal @ TTensorData[y], 0.001],
            restore[]
        ];
        {summary["KindCounts"],
         summary["ReplaySkipped"],
         summary["GraphRunCount"],
         summary["GraphEncodedDispatches"],
         Lookup[counters, "JitReplayAssigns"],
         wData,
         yData}
    ],
    {<|"DISPATCH" -> 2, "ASSIGN" -> 2|>, 2, 1, 2, 0,
     {3., 4., 5., 6.}, {3., 4., 5., 6.}},
    TestID -> "kernel-opts/metal-tjit-sinks-assign-into-producer"
]

VerificationTest[
    (* Rootless replay temporaries with non-overlapping lifetimes can
       reuse an earlier Metal output slot.  The pack marker is exposed
       through TJitCaptureOps so memory profiles can tell slot packing
       apart from ordinary repeated writes to the same persistent
       destination. *)
    Module[{oldBackend, oldTile, restore, x, y, one, w, z, f, ops,
            dispatches, summary, counters, wData, zData},
        oldBackend = Environment["DEV"];
        oldTile    = Environment["THVM_TILE"];
        restore[] := (
            If[StringQ[oldBackend],
                SetEnvironment["DEV" -> oldBackend],
                SetEnvironment["DEV" -> ""]];
            If[StringQ[oldTile],
                SetEnvironment["THVM_TILE" -> oldTile],
                SetEnvironment["THVM_TILE" -> ""]]
        );
        Internal`WithLocalSettings[
            SetEnvironment["DEV" -> "metal"];
            SetEnvironment["THVM_TILE" -> "1"],
            TInit[];
            x   = TTensorCreate @ NumericArray[N @ Range[4], "Real32"];
            y   = TTensorCreate @ NumericArray[N @ Range[10, 13], "Real32"];
            one = TTensorCreate @ NumericArray[ConstantArray[1., 4], "Real32"];
            w   = TTensorCreate @ NumericArray[ConstantArray[0., 4], "Real32"];
            z   = TTensorCreate @ NumericArray[ConstantArray[0., 4], "Real32"];
            f = TJit[Function[{}, Module[{t1, t2, u1, u2},
                t1 = TRealize[x + one];
                u1 = TAssign[w, t1 * one];
                TRealize @ u1;
                t2 = TRealize[y + one];
                u2 = TAssign[z, t2 * one];
                TRealize @ u2;
                Null
            ]]];
            f[];
            ops = TJitCaptureOps[f];
            dispatches = Select[ops, #["Kind"] === "DISPATCH" &];
            summary = TJitCaptureSummary[f];
            THotCountersReset[];
            f[];
            counters = THotCounters[];
            wData = Round[Normal @ TTensorData[w], 0.001];
            zData = Round[Normal @ TTensorData[z], 0.001],
            restore[]
        ];
        {Count[Lookup[dispatches, "ReplayPacked", False], True] >= 1,
         summary["ReplayPackedDispatches"] >= 1,
         summary["ReplaySkipped"],
         Lookup[counters, "JitReplayAssigns"],
         wData,
         zData}
    ],
    {True, True, 2, 0, {2., 3., 4., 5.}, {11., 12., 13., 14.}},
    TestID -> "kernel-opts/metal-tjit-packs-rootless-temp-slots"
]

(* === per-program-shape sharing: opt on kid_1 visible on kid_2
       when both kids share the same KProgOp[] via the cache.  This
       is what makes the proposer + auto-bench (next pass) actually
       reach training-loop kernels rather than just one-off kids. *)

VerificationTest[
    TInit[];
    xT = TTensorCreate @ N @ Range[12];
    (* Two structurally-identical reduces produce two kernels with
       the same program key.  The kernel-program cache (which once
       shared one KpCacheSlot.axes across all kids with the same key)
       was deleted in commit 4a7431b7 (Phase 4b/2); each kernel now
       owns its `_local_schedule`.  Program-key equality still drives
       autotune-unique dedup, but applied_opts no longer cross-propagate
       between kids -- kid2 stays at baseline after kid1.applied_opts
       grows.  TKernelAutotuneUnique below is the surviving dedup
       observation. *)
    TRealize @ TUOpReduce[xT, 0, "SUM"];
    kid1 = TKernelCount[] - 1;
    TRealize @ TUOpReduce[xT, 0, "SUM"];
    kid2 = TKernelCount[] - 1;
    key1 = TKernelProgramKey[kid1];
    key2 = TKernelProgramKey[kid2];
    TKernelApplyOpt[kid1, TOpt["UNROLL", 1, 4]];
    {kid1 =!= kid2,
     key1 === key2,
     key1 =!= 0,
     First[TKernelOpts[kid1]]["Applied"],
     First[TKernelOpts[kid2]]["Applied"]},
    {True, True, True, {TOpt["UNROLL", 1, 4]}, {}},
    TestID -> "kernel-opts/per-program-shape-sharing"
]

VerificationTest[
    TInit[];
    xT = TTensorCreate @ N @ Range[16];
    TRealize @ TUOpMul[xT, xT];
    kid1 = TKernelCount[] - 1;
    TRealize @ TUOpMul[xT, xT];
    kid2 = TKernelCount[] - 1;
    res = TKernelAutotuneUnique[];
    {AssociationQ[res],
     Length[Intersection[Keys[res], {kid1, kid2}]],
     TKernelProgramKey[kid1] === TKernelProgramKey[kid2]},
    {True, 1, True},
    TestID -> "kernel-opts/autotune-unique-dedupes-program-shapes"
]

VerificationTest[
    TInit[];
    xT = TTensorCreate @ N @ Range[16];
    TRealize @ TUOpReduce[xT, 0, "SUM"];
    kid = TKernelCount[] - 1;
    profile = TProfileAll[];
    res = TKernelAutotuneTop[profile, 1, "ReduceFlops"];
    {AssociationQ[res],
     Keys[res],
     Length[First[TKernelOpts[kid]]["Applied"]] >= 0},
    {True, {kid}, True},
    TestID -> "kernel-opts/autotune-top-bounds-profile-reps"
]

(* === applied_opts log: chronological list of TOpt actions === *)

VerificationTest[
    TInit[];
    xT = TTensorCreate @ N @ Range[24];
    TRealize @ TUOpReduce[xT, 0, "SUM"];
    kid = TKernelCount[] - 1;
    TKernelApplyOpt[kid, TOpt["UNROLL", 1, 4]];   (* 24 / 4 = 6 *)
    TKernelApplyOpt[kid, TOpt["UNROLL", 1, 3]];   (* outer 6 / 3 = 2 *)
    First[TKernelOpts[kid]]["Applied"],
    {TOpt["UNROLL", 1, 4], TOpt["UNROLL", 1, 3]},
    TestID -> "kernel-opts/applied-log-chronological"
]
