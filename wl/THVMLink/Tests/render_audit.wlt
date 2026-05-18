(* render_audit.wlt -- audits emitted MSL kernels for two structural
   render-time bugs surfaced by lazy kernelization of forward+backward
   tensor graphs.  Bug 1 is expected to FAIL on master until
   render_uop.c's reduce-nesting + range-collection blind spots are
   fixed.  Don't quarantine them in pending_*.wlt.

   Both bugs live in src/codegen/render_uop.c:
   - rmu_collect_ranges_rec (line 420) refuses to descend into UOP_REDUCE
     bodies, so range axes that only appear inside nested reduce bodies
     are dropped from the kernel's ranges[] and from each reduce's
     required_pos.  Symptom: variables referenced inside an inner
     reduce body but never declared as for-loop indices.
   - The reduce emit macro (RMU_EMIT_ONE_REDUCE) treats every reduce
     as a free-standing block ordered by required_pos.  If reduce A's
     body reads reduce B's loop variable, A must be emitted INSIDE B's
     loop; the current emitter has no such nesting and falls back to
     flat hoisting.  Symptom: an accumulator declared but its for-loop
     never emitted because the reduce-axis range term wasn't reachable.

   Both bugs are triggered by TConv2D + TBatchNormTrain backward, where
   the chain rule fans the BN variance buffer through nested reduces
   that share loop axes with their consumers. *)

(* === single shared forward+backward build (calling TInit twice in
       the same kernel currently traps; share one build across all
       assertions until that separate bug is fixed). === *)
auditCounts = Module[{bs = 3, W1, b1, gamma, beta, W2, b2, x, tgt,
                      h1, n1, p1, flat, logits, loss, params, n, fallbacks},
    TInit[];
    SeedRandom[1];
    W1    = TTensorCreate @ NumericArray[
        RandomReal[{-0.1, 0.1}, {4, 1, 3, 3}], "Real32"];
    b1    = TTensorCreate @ NumericArray[ConstantArray[0., {4}], "Real32"];
    gamma = TTensorCreate @ NumericArray[ConstantArray[1.0, {4}], "Real32"];
    beta  = TTensorCreate @ NumericArray[ConstantArray[0.0, {4}], "Real32"];
    W2    = TTensorCreate @ NumericArray[
        RandomReal[{-0.1, 0.1}, {16, 5}], "Real32"];
    b2    = TTensorCreate @ NumericArray[ConstantArray[0., {5}], "Real32"];
    x     = TTensorCreate @ NumericArray[
        RandomReal[{0., 1.}, {bs, 1, 6, 6}], "Real32"];
    tgt   = TTensorCreate @ NumericArray[
        RandomInteger[{0, 4}, {bs}], "Integer32"];
    h1     = TConv2D[x, W1, b1];
    n1     = TBatchNormTrain[h1, gamma, beta];
    p1     = TMaxPool2d[n1, 2];
    flat   = TUOpReshape[p1, {bs, 16}];
    logits = TLinear[flat, W2, b2];
    loss   = TSparseCategoricalCrossEntropy[logits, tgt];
    params = {W1, gamma, W2};
    TMaterialize @ TWnf @ loss;
    TMaterialize /@ (TWnf /@ TGradMany[loss, params]);
    n = TKernelCount[] - 1;
    fallbacks = Length @ Select[Range[1, n],
        Length[DeleteDuplicates @ StringCases[
            TKernelSource[#, "Metal"],
            RegularExpression["\\bbuf\\d{4,}"]]] > 0 &];
    (* Use-before-decl: for each kernel, find every aN axis reference
       and every (uint|int) aN = declaration; flag the kernel if any
       referenced axis name was never declared in the same source.
       Symptom of the reduce-emit-nesting bug: reduces are sibling-
       hoisted but their bodies reference axes owned by other reduces
       emitted later in the same kernel (or never, when the inner
       reduce's for-loop is dropped because its reduce-axis range term
       was unreachable). *)
    undecl = Length @ Select[Range[1, n],
        Function[k, Module[{src, decls, refs},
            src = TKernelSource[k, "Metal"];
            decls = DeleteDuplicates @ StringCases[src,
                RegularExpression["(?:uint|int) (a\\d+)"] -> "$1"];
            refs = DeleteDuplicates @ StringCases[src,
                RegularExpression["\\b(a\\d+)\\b"] -> "$1"];
            Length[Complement[refs, decls]] > 0
        ]]];
    <|
        "Kernels"      -> n,
        "Fallbacks"    -> fallbacks,
        "Leaks"        -> Length @ TKernelAuditLeaks[],
        "UndeclaredAx" -> undecl
    |>
];

(* === Bug 1: render emits buf<N> fallback names in backward MSL ===
   Backward TConv2D + BN-train reaches at least one UOP_BUFFER (the BN
   variance) that the discover walker fails to register, so its load in
   the emitted MSL references an undeclared `buf<N>` instead of an
   `inN` kernel argument. *)
VerificationTest[
    auditCounts["Fallbacks"],
    0,
    TestID -> "thvm/render/no-buf-fallback-names-in-backward"
]

(* === Bug 2: kernel-level structural leak audit returns clean ===
   TKernelAuditLeaks walks every live kernel's lifted store_root and
   reports those whose root term still contains a raw UOP_BUFFERIZE
   node or a bare TAG_TEN reference.  Either leak means the rangeify
   pass left an unresolved tensor/buffer reference that the renderer
   then falls back on. *)
VerificationTest[
    auditCounts["Leaks"],
    0,
    TestID -> "thvm/render/no-bufferize-or-ten-leaks-in-backward"
]

(* === Bug 3: kernel count stays bounded (regression guard) ===
   Backward TGradMany over {W1, gamma, W2} for this 2-layer network
   currently produces ~600 kernels; this generous bound flags any
   future regression that explodes per-target expansion further. *)
VerificationTest[
    auditCounts["Kernels"] <= 1500,
    True,
    TestID -> "thvm/render/backward-kernel-count-under-1500"
]

(* === Bug 4: no axis variable referenced before its for-loop declares it ===
   The reduce emit macro (RMU_EMIT_ONE_REDUCE in src/codegen/render_uop.c)
   treats every reduce as a free-standing block ordered by required_pos.
   If reduce A's body reads reduce B's loop variable (B's reduce-axis),
   A must be emitted INSIDE B's for-loop body; current emitter has no
   such nesting -- it sibling-hoists all reduces, producing kernels
   where `_acc<A>`'s body references `aN` whose `for (uint aN = ...)`
   only appears later (or never, when an inner reduce's loop drops
   because its reduce-axis range term was structurally unreachable).
   These kernels fail Metal compile with `undeclared identifier`. *)
VerificationTest[
    auditCounts["UndeclaredAx"],
    0,
    TestID -> "thvm/render/no-undeclared-axis-vars-in-backward"
]
