(* kernel_opts.wlt -- TOpt + TKernelOpts + TKernelApplyOpt surface +
   the C-side KernelAxes scaffold under it (Phase 16).

   Covers:
     - construction + structural form of TOpt
     - default axes for elementwise + reduce-tail kernels
     - axes_apply_opt mutations: UNROLL/UPCAST split, SWAP swap
     - validation (out-of-range axis, non-divisible arg, unknown op)
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

(* === SWAP exchanges two axes (in-place; no new axis) === *)

VerificationTest[
    TInit[];
    xT = TTensorCreate @ N @ Range[24];
    TRealize @ TUOpReduce[xT, 0, "SUM"];
    kid = TKernelCount[] - 1;
    TKernelApplyOpt[kid, TOpt["UPCAST", 0, 1]];      (* split LOOP(1) into LOOP(1) + UPCAST(1); no-op shape but adds axis *)
    pre  = First @ TKernelOpts[kid];
    res  = TKernelApplyOpt[kid, TOpt["SWAP", 0, 1]];
    post = First[res];
    {pre["AxisTypes"], post["AxisTypes"]},
    {{"LOOP", "UPCAST", "REDUCE"}, {"UPCAST", "LOOP", "REDUCE"}},
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
       divisors of output_numel (factor in {2,4,8,16}). *)
    TInit[];
    a = TTensorCreate @ N @ Range[16];         (* output_numel = 16 *)
    TRealize @ TUOpMul[a, a];
    TKernelProposed[TKernelCount[] - 1],
    {TOpt["UPCAST", 0, 16], TOpt["UPCAST", 0, 8],
     TOpt["UPCAST", 0, 4],  TOpt["UPCAST", 0, 2]},
    TestID -> "kernel-opts/propose-elementwise-upcast"
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
     AllTrue[Thread[Abs[pre - post] < 1.0*^-5], TrueQ]},
    {True, True},
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

(* === fire-time autotune trigger: env opt-in via THVM_AUTOTUNE.
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

(* === per-program-shape sharing: opt on kid_1 visible on kid_2
       when both kids share the same KProgOp[] via the cache.  This
       is what makes the proposer + auto-bench (next pass) actually
       reach training-loop kernels rather than just one-off kids. *)

VerificationTest[
    TInit[];
    xT = TTensorCreate @ N @ Range[12];
    (* Two structurally-identical reduces -> two kids sharing one
       cached KProgOp[]. *)
    TRealize @ TUOpReduce[xT, 0, "SUM"];
    kid1 = TKernelCount[] - 1;
    TRealize @ TUOpReduce[xT, 0, "SUM"];
    kid2 = TKernelCount[] - 1;
    (* Apply opt to kid1; must show up on kid2's TKernelOpts because
       both kids point at the same KpCacheSlot.axes. *)
    TKernelApplyOpt[kid1, TOpt["UNROLL", 1, 4]];
    {kid1 =!= kid2,
     First[TKernelOpts[kid2]]["Applied"]},
    {True, {TOpt["UNROLL", 1, 4]}},
    TestID -> "kernel-opts/per-program-shape-sharing"
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
