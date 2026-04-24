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
