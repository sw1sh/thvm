(* grad.wlt -- end-to-end autograd via UOP_GRAD chain-rule rewrite. *)

(* === leaf cases === *)

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    g = TRealize @ TGrad[a, a];   (* d(a)/d(a) = 1 *)
    Normal @ TTensorData[g],
    {1.0, 1.0, 1.0},
    TestID -> "grad/identity-1d"
]

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    b = TTensorCreate @ NumericArray[{4.0, 5.0, 6.0}, "Real32"];
    g = TRealize @ TGrad[b, a];   (* d(b)/d(a) = 0 *)
    Normal @ TTensorData[g],
    {0.0, 0.0, 0.0},
    TestID -> "grad/independent-leaf-zero"
]

(* === ADD === *)

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    b = TTensorCreate @ NumericArray[{10.0, 20.0, 30.0}, "Real32"];
    (* d(a + b)/d(a) = 1 *)
    g = TRealize @ TGrad[TUOpAdd[a, b], a];
    Normal @ TTensorData[g],
    {1.0, 1.0, 1.0},
    TestID -> "grad/add-w-r-t-a"
]

(* === MUL: product rule === *)

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    b = TTensorCreate @ NumericArray[{4.0, 5.0, 6.0}, "Real32"];
    (* d(a * b)/d(a) = b *)
    g = TRealize @ TGrad[TUOpMul[a, b], a];
    Normal @ TTensorData[g],
    {4.0, 5.0, 6.0},
    TestID -> "grad/mul-product-rule"
]

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    b = TTensorCreate @ NumericArray[{4.0, 5.0, 6.0}, "Real32"];
    (* d(a * b)/d(b) = a *)
    g = TRealize @ TGrad[TUOpMul[a, b], b];
    Normal @ TTensorData[g],
    {1.0, 2.0, 3.0},
    TestID -> "grad/mul-w-r-t-b"
]

(* === NEG === *)

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    (* d(-a)/d(a) = -1 *)
    g = TRealize @ TGrad[TUOpNeg[a], a];
    Normal @ TTensorData[g],
    {-1.0, -1.0, -1.0},
    TestID -> "grad/neg"
]

(* === REDUCE_SUM === *)

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0, 4.0}, "Real32"];
    (* d(sum(a))/d(a) = ones_like(a)  (cotangent broadcasts) *)
    g = TRealize @ TGrad[TUOpReduce[a, 0, "SUM"], a];
    Normal @ TTensorData[g],
    {1.0, 1.0, 1.0, 1.0},
    TestID -> "grad/reduce-sum-broadcasts-back"
]

(* === composite: x * x === *)

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{2.0, 3.0, 5.0}, "Real32"];
    (* d(a * a)/d(a) = 2a   (product rule with both sides hitting target) *)
    g = TRealize @ TGrad[TUOpMul[a, a], a];
    Normal @ TTensorData[g],
    {4.0, 6.0, 10.0},
    TestID -> "grad/x-times-x-equals-2x"
]

(* === RESHAPE: identity-on-data, identity-on-grad === *)

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0, 4.0}, "Real32"];
    (* d(reshape(a, {2,2}))/d(a) = 1, returned in a's shape {4} *)
    g = TRealize @ TGrad[TUOpReshape[a, {2, 2}], a];
    Normal @ TTensorData[g],
    {1.0, 1.0, 1.0, 1.0},
    TestID -> "grad/reshape-identity-cotangent"
]

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0, 4.0}, "Real32"];
    (* d(sum(reshape(a*a, {2,2})))/d(a) = 2a -- chain through MUL
       inside a RESHAPE.  Reduce collapses to a scalar cotangent
       that broadcasts back. *)
    expr = TUOpReduce[TUOpReshape[TUOpMul[a, a], {2, 2}], 0, "SUM"];
    g = TRealize @ TGrad[expr, a];
    Normal @ TTensorData[g],
    {2.0, 4.0, 6.0, 8.0},
    TestID -> "grad/reshape-inside-mul-chain"
]

(* === CMPLT / ReLU: mask is non-differentiable === *)

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{-1.0, 2.0, -3.0, 4.0}, "Real32"];
    (* d(ReLU(a))/d(a) = mask = {0, 1, 0, 1}.  Exercises the CMPLT-zero
       rule via the surrounding MUL product rule. *)
    g = TRealize @ TGrad[TReLU[a], a];
    Normal @ TTensorData[g],
    {0.0, 1.0, 0.0, 1.0},
    TestID -> "grad/relu-mask"
]

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{-2.0, 0.5, 1.0, -0.1}, "Real32"];
    b = TTensorCreate @ NumericArray[{1.0, 1.0, 1.0, 1.0}, "Real32"];
    (* d(CMPLT(a, b))/d(a) = 0 directly (no MUL wrapper). *)
    g = TRealize @ TGrad[TUOpCmplt[a, b], a];
    Normal @ TTensorData[g],
    {0.0, 0.0, 0.0, 0.0},
    TestID -> "grad/cmplt-direct-zero"
]

(* === EXPAND: cotangent collapses along expanded axes === *)

VerificationTest[
    TInit[];
    a    = TTensorCreate @ NumericArray[{2.5}, "Real32"];        (* shape {1} *)
    seed = TTensorCreate @ NumericArray[{1.0, 1.0, 1.0}, "Real32"]; (* shape {3} *)
    (* y = EXPAND(a, {3}) is {2.5, 2.5, 2.5}; cotangent ones{3}.
       d/da = sum(gy) = 3, returned in a's shape {1}. *)
    g = TRealize @ TUOpGrad[TUOpExpand[a, {3}], seed, a];
    Normal @ TTensorData[g],
    {3.0},
    TestID -> "grad/expand-shape1-to-shape3"
]

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{0.0}, "Real32"];
    (* d(EXPAND(CONST(5), shape) wrt a) = 0 -- CONST has no
       gradient.  EXPAND-of-CONST short-circuit. *)
    g = TRealize @ TGrad[TUOpExpand[TUOpConst[5.0, "f32"], {4}], a];
    Normal @ TTensorData[g],
    {0.0},
    TestID -> "grad/expand-of-const-short-circuits"
]

(* === RECIP: d(1/x)/dx = -1/x^2 === *)

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{2.0, 4.0, 5.0}, "Real32"];
    (* d(1/a)/da = -1/a^2 = {-0.25, -0.0625, -0.04} *)
    g = TRealize @ TGrad[TUOpRecip[a], a];
    Normal @ TTensorData[g],
    {-0.25, -0.0625, -0.04},
    SameTest -> (Max[Abs[#1 - #2]] < 1.0*^-5 &),
    TestID -> "grad/recip-derivative"
]

(* === simple linear: 2x + 3 === *)

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    expr = TUOpAdd[TUOpMul[TUOpConst[2.0, "f32"], a], TUOpConst[3.0, "f32"]];
    (* d(2a + 3)/d(a) = 2 *)
    g = TRealize @ TGrad[expr, a];
    Normal @ TTensorData[g],
    {2.0, 2.0, 2.0},
    TestID -> "grad/linear-2x-plus-3"
]
