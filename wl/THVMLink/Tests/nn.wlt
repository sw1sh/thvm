(* nn.wlt -- high-level NN converter (Wolfram layers -> TUOp graph)
   plus Tensor-method helpers + autograd through compositions. *)

(* === Tensor-method helpers === *)

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    b = TTensorCreate @ NumericArray[{4.0, 5.0, 6.0}, "Real32"];
    Normal @ TTensorData[TRealize[TDot[a, b]]],
    {32.0},
    TestID -> "nn/dot"
]

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    Normal @ TTensorData[TRealize[TSquare[a]]],
    {1.0, 4.0, 9.0},
    TestID -> "nn/square"
]

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    Normal @ TTensorData[TRealize[TL2Loss[a]]],
    {14.0},
    TestID -> "nn/l2loss"
]

VerificationTest[
    TInit[];
    pred   = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    target = TTensorCreate @ NumericArray[{1.5, 2.5, 3.5}, "Real32"];
    Normal @ TTensorData[TRealize[TMSELoss[pred, target]]],
    {0.75},
    TestID -> "nn/mse"
]

(* === gradients through pure ADD/MUL/REDUCE chains === *)

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    b = TTensorCreate @ NumericArray[{4.0, 5.0, 6.0}, "Real32"];
    Normal @ TTensorData[TRealize[TGrad[TDot[a, b], a]]],
    {4.0, 5.0, 6.0},
    TestID -> "nn/grad-dot-wrt-a"
]

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{2.0, 3.0, 4.0}, "Real32"];
    Normal @ TTensorData[TRealize[TGrad[TL2Loss[a], a]]],
    {4.0, 6.0, 8.0},
    TestID -> "nn/grad-l2-equals-2a"
]

VerificationTest[
    TInit[];
    pred   = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    target = TTensorCreate @ NumericArray[{1.5, 2.5, 3.5}, "Real32"];
    Normal @ TTensorData[TRealize[TGrad[TMSELoss[pred, target], pred]]],
    {-1.0, -1.0, -1.0},
    TestID -> "nn/grad-mse-wrt-pred"
]

(* Polynomial f(x) = x^3 + 2x^2 + x; f'(x) = 3x^2 + 4x + 1. *)
VerificationTest[
    TInit[];
    x   = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    two = TTensorCreate @ NumericArray[{2.0, 2.0, 2.0}, "Real32"];
    cubic = TUOpMul[TSquare[x], x];
    quad  = TUOpMul[TSquare[x], two];
    poly  = TUOpAdd[TUOpAdd[cubic, quad], x];
    Normal @ TTensorData[TRealize[TGrad[TSum[poly], x]]],
    {8.0, 21.0, 40.0},
    TestID -> "nn/grad-cubic-poly"
]

(* === Wolfram LinearLayer forward via TFromNet === *)

VerificationTest[
    TInit[];
    layer = NetReplacePart[
        LinearLayer[3, "Input" -> 2],
        {"Weights" -> NumericArray[{{1.0, 2.0}, {3.0, 4.0}, {5.0, 6.0}},     "Real32"],
         "Biases"  -> NumericArray[{10.0, 20.0, 30.0},                        "Real32"]}
    ];
    x = TTensor[{1, 2}, {7.0, 8.0}, "f32"];
    Normal @ TTensorData[TRealize[TFromNet[layer, x]]],
    {33.0, 73.0, 113.0},
    TestID -> "nn/wolfram-linear-forward"
]

(* === Wolfram ElementwiseLayer (square) via TFromNet === *)

VerificationTest[
    TInit[];
    layer = ElementwiseLayer[# # &];
    x     = TTensorCreate @ NumericArray[{2.0, 3.0, 4.0}, "Real32"];
    Normal @ TTensorData[TRealize[TFromNet[layer, x]]],
    {4.0, 9.0, 16.0},
    TestID -> "nn/wolfram-eltwise-square"
]

(* === NetChain through TFromNet ===
   Build a tiny "polynomial layer": sum_i (W_i * x_i)^2  via
   LinearLayer[1] -> ElementwiseLayer[#^2 &]. *)

VerificationTest[
    TInit[];
    chain = NetReplacePart[
        NetChain[{LinearLayer[1, "Input" -> 3], ElementwiseLayer[# # &]}],
        {{1, "Weights"} -> NumericArray[{{1.0, 1.0, 1.0}}, "Real32"],
         {1, "Biases"}  -> NumericArray[{0.0},             "Real32"]}
    ];
    x = TTensor[{1, 3}, {2.0, 3.0, 4.0}, "f32"];
    (* (1*2 + 1*3 + 1*4)^2 = 81 *)
    Normal @ TTensorData[TRealize[TFromNet[chain, x]]],
    {81.0},
    TestID -> "nn/wolfram-netchain-linear-square"
]

(* === gradient through a NetChain (square only -- LinearLayer's
   EXPAND has no grad rule yet so the matmul backward path is a TODO).
   Test: d(sum(square(W . x + b)))/d(target) via the dot-product form
   (no EXPAND in the path).  A "fake" 1-output Linear via TDot. *)

VerificationTest[
    TInit[];
    a   = TTensorCreate @ NumericArray[{2.0, 3.0, 5.0}, "Real32"];
    w   = TTensorCreate @ NumericArray[{1.0, 1.0, 1.0}, "Real32"];
    out = TSquare[TDot[w, a]];                  (* (sum(w*a))^2 *)
    (* (sum(a))^2 = (10)^2 = 100; d/da = 2*sum(a)*w = 20*{1,1,1} *)
    {Normal @ TTensorData[TRealize[out]],
     Normal @ TTensorData[TRealize[TGrad[out, a]]]},
    {{100.0}, {20.0, 20.0, 20.0}},
    TestID -> "nn/grad-through-square-of-dot"
]
