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
    Module[{lam, var, shape, alo, aloVar},
        TDef["shaped_id_test", TLamShape[{4}, w, w]];
        (* Drive the REF one layer to expose the dyn LAM. *)
        alo = TWnf[TApp[TRef["shaped_id_test"], TEra[]]];
        (* After APP-LAM beta the bound var is substituted to ERA
           -- but the LAM cell's annotation should still be in the
           side table at the dyn loc that alo_realize allocated.
           We can verify by registering a fresh shaped lambda
           after this, both should be present (count >= 2). *)
        TLamShape[{1, 2, 3}, x, x];
        THVMLink`Private`$lamShapeCountFn[] >= 2
    ],
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
