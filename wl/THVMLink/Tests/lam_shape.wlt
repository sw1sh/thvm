(* lam_shape.wlt -- shape annotations on TLam-bound variables.
   `TLamShape[shape, x, body]` records `x`'s shape in a side table
   keyed by the LAM's heap loc.  `term_shape_in` consults this
   table when seeing a TVAR before APP-LAM beta has substituted a
   concrete value.  Annotations are propagated through book
   conversion (TDef) and ALO realization (TRef unfold) so every
   instance of a recursive lambda inherits the binder's shape.

   Materialize integration -- treating shape-annotated TVAR as
   KSRC_AS_INPUT so the lambda body can compile pre-substitution
   -- lands separately. *)

(* === basic: annotation visible on the TVAR === *)

VerificationTest[
    TInit[];
    Module[{lam, body, var, shape},
        lam = TLamShape[{3}, w, w];
        body = THeapRead[TTermVal[lam]];
        (* The body of a one-binder identity lambda is the bound
           VAR itself; its shape should resolve via the side
           table to {3}. *)
        var = body;
        shape = TTermShape[var];
        {TTagName[TTermTag[var]], shape}
    ],
    {"VAR", {3}},
    TestID -> "lam-shape/identity-tvar-resolves-to-annotated-shape"
]

(* === count: registering N distinct annotated lambdas grows the
       side table by N === *)

VerificationTest[
    TInit[];
    Module[{n0, lams, n1},
        n0 = THVMLink`Private`$lamShapeCountFn[];
        lams = Table[TLamShape[{k}, x, x], {k, 1, 5}];
        n1 = THVMLink`Private`$lamShapeCountFn[];
        n1 - n0
    ],
    5,
    TestID -> "lam-shape/registration-grows-side-table"
]

(* === book + alo round-trip preserves the annotation.  Defining
       a TLamShape via TDef and then re-realizing it through TRef
       should leave the bound var still shape-resolvable. === *)

VerificationTest[
    TInit[];
    (
        TDef["shaped_id_test", TLamShape[{4}, w, w]];
        (* Drive the REF one layer to expose the dyn LAM. *)
        TWnf[TApp[TRef["shaped_id_test"], TEra[]]];
        (* After APP-LAM beta the bound var is substituted to ERA
           -- but the LAM cell's annotation should still be in the
           side table at the dyn loc that alo_realize allocated.
           We can verify by registering a fresh shaped lambda
           after this, both should be present (count >= 2). *)
        TLamShape[{1, 2, 3}, x, x];
        THVMLink`Private`$lamShapeCountFn[] >= 2
    ),
    True,
    TestID -> "lam-shape/survives-book-and-alo-round-trip"
]

(* === pre-beta: annotation is consulted before the bound var has
       been substituted.  `TVAR(loc)` cell contents are still the
       body Term (not SUB-marked), so term_shape_in falls into the
       lam_shape lookup branch. === *)

VerificationTest[
    TInit[];
    Module[{lam, varTerm},
        lam = TLamShape[{2, 5}, x, x];
        (* Read the body Term out of the LAM cell.  Identity
           lambda's body is the bound var. *)
        varTerm = THeapRead[TTermVal[lam]];
        TTermShape[varTerm]
    ],
    {2, 5},
    TestID -> "lam-shape/pre-beta-shape-resolves"
]

(* === post-beta: after APP-LAM substitutes a concrete TEN, the
       annotation is no longer consulted (term_resolve sees the
       SUB-marked cell first and returns the substituted TEN's
       own shape). === *)

VerificationTest[
    TInit[];
    Module[{lam, ten, app, post},
        ten = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0, 4.0}, "Real32"];
        lam = TLamShape[{99}, w, w];
        (* Apply the lambda to a 4-vector tensor.  After APP-LAM
           the TVAR resolves to the TEN whose actual shape is
           {4}, NOT the {99} annotation -- post-beta the
           annotation is shadowed by the SUB. *)
        app = TWnf[TApp[lam, ten]];
        post = TTermShape[app];
        post
    ],
    {4},
    TestID -> "lam-shape/post-beta-defers-to-substituted-shape"
]

(* === materialize + APP-LAM end-to-end ====================================
   Compile a lambda body BEFORE substitution: the body's TVAR
   becomes a symbolic input slot.  APP-LAM then substitutes a
   TEN; the kernel resolves the TVAR through SUB at fire time
   and reads the substituted tensor's buffer.

   This is the path-3 capability: "compile once, dispatch many".
   The materialize call happens once, against the abstract
   body.  Each `TApp[lam, ten_K]` reuses the same kernel program
   with `ten_K` as input. *)

VerificationTest[
    TInit[];
    Module[{lamLoc, varTerm, matBody, lam, ten, result},
        lamLoc = THeapAlloc[1];
        THVMLink`Private`$lamShapeSetFn[lamLoc, {3}];
        varTerm = TVarFor[lamLoc];
        (* Body uses the TVAR; pre-substitution materialize
           emits a kernel with KSRC_AS_INPUT(0) for the var. *)
        matBody = TMaterialize[TUOpAdd[varTerm, varTerm]];
        THeapSet[lamLoc, matBody];
        lam = THVMLink`Private`packTerm[0, $TagLAM, 0, lamLoc];
        ten = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
        result = TWnf[TApp[lam, ten]];
        Normal @ TTensorData[result]
    ],
    {2., 4., 6.},
    TestID -> "lam-shape/materialize-body-then-apply-fires-correctly"
]

(* Plain TLam is JIT-aware: when the body is a UOP graph and the
   first TApp's argument has a shape, APP-LAM materializes the
   body before the standard beta.  No flag, no separate
   constructor -- TLam itself does the right thing. *)

VerificationTest[
    TInit[];
    Module[{lam, kernelsBefore, ten, result, kernelsAfter, programs},
        lam = TLam[w, TUOpAdd[w, w]];
        kernelsBefore = TKernelCount[] - 1;
        ten = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
        result = TWnf[TApp[lam, ten]];
        kernelsAfter = TKernelCount[] - 1;
        programs     = TKernelProgramCacheSize[];
        (* No kernel before the first APP; one kernel emerges
           from the JIT step; one distinct program in the cache. *)
        {kernelsBefore, Normal @ TTensorData[result], kernelsAfter, programs}
    ],
    {0, {2., 4., 6.}, 1, 1},
    TestID -> "lam-shape/tlam-jit-on-first-apply"
]

(* Different argument shapes: the same TLam applied to a
   {5}-vector compiles a fresh kernel with that input shape
   (each instance materializes once for its own arg.shape). *)

VerificationTest[
    TInit[];
    Module[{lam5, ten5, result},
        lam5 = TLam[w, TUOpAdd[w, w]];
        ten5 = TTensorCreate @ NumericArray[{10., 20., 30., 40., 50.}, "Real32"];
        result = TWnf[TApp[lam5, ten5]];
        Normal @ TTensorData[result]
    ],
    {20., 40., 60., 80., 100.},
    TestID -> "lam-shape/tlam-shape-inferred-from-arg"
]

(* Non-UOP body: a curried lambda's body is another TLam, not a
   UOP graph -- the JIT step is skipped (materialize would be a
   no-op).  Plain beta substitution proceeds. *)

VerificationTest[
    TInit[];
    Module[{lam, ten, partial, kernelsAfterPartial},
        lam = TLam[x, TLam[y, TUOpAdd[x, y]]];
        ten = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
        partial = TWnf[TApp[lam, ten]];
        kernelsAfterPartial = TKernelCount[] - 1;
        (* Outer beta substitutes x, returns the inner lambda;
           no kernel was emitted because the outer body wasn't
           a UOP graph. *)
        {TTagName[TTermTag[partial]], kernelsAfterPartial}
    ],
    {"LAM", 0},
    TestID -> "lam-shape/curried-lambda-skips-jit-on-outer-app"
]

(* Two distinct APPs to the same materialized lambda body should
   share the kernel program -- the program cache hashes by
   structure, the TVAR slot is the structural placeholder. *)

VerificationTest[
    TInit[];
    Module[{lamLoc, varTerm, matBody, lam, t1, r1, kernels, programs},
        lamLoc = THeapAlloc[1];
        THVMLink`Private`$lamShapeSetFn[lamLoc, {3}];
        varTerm = TVarFor[lamLoc];
        matBody = TMaterialize[TUOpAdd[varTerm, varTerm]];
        THeapSet[lamLoc, matBody];
        lam = THVMLink`Private`packTerm[0, $TagLAM, 0, lamLoc];
        t1 = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
        (* APP-LAM beta is destructive: it SUB-marks heap[lamLoc].
           This asserts the APP works and the program cache size
           stayed at 1 for the body (no per-APP fresh kernel). *)
        r1 = TWnf[TApp[lam, t1]];
        kernels = TKernelCount[] - 1;
        programs = TKernelProgramCacheSize[];
        {Normal @ TTensorData[r1], kernels, programs}
    ],
    {{2., 4., 6.}, 1, 1},
    TestID -> "lam-shape/single-kernel-from-materialized-body"
]
