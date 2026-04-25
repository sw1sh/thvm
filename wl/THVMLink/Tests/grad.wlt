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

(* === SHRINK: grad PADs cotangent back to source extent === *)

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0, 4.0, 5.0}, "Real32"];
    (* SHRINK keeps indices [1, 4) -- a length-3 slice from a length-5
       tensor.  d/d(a) of that selection w.r.t. a is the {0,1,1,1,0}
       indicator: gy=1 inside the kept window, 0 outside. *)
    g = TRealize @ TGrad[TUOpShrink[a, {{1, 4}}], a];
    Normal @ TTensorData[g],
    {0.0, 1.0, 1.0, 1.0, 0.0},
    TestID -> "grad/shrink-pads-cotangent-with-zeros"
]

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0, 4.0, 5.0}, "Real32"];
    (* d(sum(SHRINK(a*a, [1,4])))/d(a) = 2a inside the kept window,
       0 outside.  Exercises the chain rule through MUL inside SHRINK. *)
    expr = TUOpReduce[TUOpShrink[TUOpMul[a, a], {{1, 4}}], 0, "SUM"];
    g = TRealize @ TGrad[expr, a];
    Normal @ TTensorData[g],
    {0.0, 4.0, 6.0, 8.0, 0.0},
    TestID -> "grad/shrink-inside-mul-chain"
]

(* === SHRINK + PERMUTE + PAD + FLIP composition smoke ===
   Sub-item (e) of the SHRINK/PAD/PERMUTE/FLIP grad arc:
   verifies the four rules compose under the chain rule so the
   forthcoming TUOpConv2D lowering (which fans through these
   movement ops) backprops to a finite, source-shaped gradient. *)

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[
        {{1.0, 2.0, 3.0, 4.0}, {5.0, 6.0, 7.0, 8.0}}, "Real32"];
    (* Chain: a {2, 4} -> SHRINK to {2, 2} on axis 1
                       -> PERMUTE to {2, 2} (transpose)
                       -> PAD by 1 on axis 0 -> {4, 2}
                       -> FLIP axis 0
       Sum to scalar; backprop to a.  Each link exercises one of
       the new grad rules; we just need the outcome to be a valid
       a-shaped {2, 4} grad with finite numerics. *)
    expr = TUOpReduce[
        TUOpReduce[
            TUOpFlip[
                TUOpPad[
                    TUOpPermute[
                        TUOpShrink[a, {{0, 2}, {1, 3}}],
                        {1, 0}],
                    {{1, 1}, {0, 0}}],
                {0}],
            0, "SUM"],
        0, "SUM"];
    g = TRealize @ TGrad[expr, a];
    {TTensorShape[g],
     AllTrue[Flatten @ Normal @ TTensorData[g], NumericQ],
     AllTrue[Flatten @ Normal @ TTensorData[g], (Abs[#] < 1.0*^6)&]},
    {{2, 4}, True, True},
    TestID -> "grad/shrink-permute-pad-flip-composition"
]

(* === FLIP: grad mirrors the cotangent on the same axes === *)

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0, 4.0}, "Real32"];
    (* d(sum(FLIP(a*a, {0})))/d(a) = 2a -- chain through MUL inside
       FLIP.  FLIP is its own inverse, so the cotangent flipped back
       to a's layout is the same constant; element-wise this gives
       2*a in a's natural order. *)
    expr = TUOpReduce[TUOpFlip[TUOpMul[a, a], {0}], 0, "SUM"];
    g = TRealize @ TGrad[expr, a];
    Normal @ TTensorData[g],
    {2.0, 4.0, 6.0, 8.0},
    TestID -> "grad/flip-inside-mul-chain"
]

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0, 4.0}, "Real32"];
    (* d(FLIP(a, {0}))/d(a) = 1 broadcast to a's shape (FLIP just
       moves elements; sum collapses the axis so order doesn't show
       up in the gradient). *)
    expr = TUOpReduce[TUOpFlip[a, {0}], 0, "SUM"];
    g = TRealize @ TGrad[expr, a];
    Normal @ TTensorData[g],
    {1.0, 1.0, 1.0, 1.0},
    TestID -> "grad/flip-identity-cotangent"
]

(* === PERMUTE: grad applies the inverse permutation === *)

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[
        {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}}, "Real32"];
    (* PERMUTE swaps axes (rank-2 transpose).  d/d(a) of the sum of
       transpose(a*a) is 2a -- chain through MUL inside PERMUTE
       exercises that the inverse permute reorders the cotangent
       back to a's axis order. *)
    expr = TUOpReduce[
        TUOpReduce[TUOpPermute[TUOpMul[a, a], {1, 0}], 0, "SUM"],
        0, "SUM"];
    g = TRealize @ TGrad[expr, a];
    Normal @ TTensorData[g],
    {{2.0, 4.0, 6.0}, {8.0, 10.0, 12.0}},
    TestID -> "grad/permute-inside-mul-chain"
]

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[
        {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}}, "Real32"];
    (* d(PERMUTE(a, {1,0}))/d(a) is the indicator: every position
       contributes 1 to its transposed counterpart.  Reduced sum
       collapses both axes; gradient is constant 1 in a's shape. *)
    expr = TUOpReduce[
        TUOpReduce[TUOpPermute[a, {1, 0}], 0, "SUM"],
        0, "SUM"];
    g = TRealize @ TGrad[expr, a];
    Normal @ TTensorData[g],
    {{1.0, 1.0, 1.0}, {1.0, 1.0, 1.0}},
    TestID -> "grad/permute-identity-cotangent"
]

(* === PAD: grad SHRINKs cotangent back to inner extent === *)

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0}, "Real32"];
    (* PAD widens length-2 to length-4 with 1 zero on each side.
       d/d(a) maps a length-4 cotangent back to a length-2 grad =
       the inner slice; with seed=1 broadcast, that's {1, 1}. *)
    g = TRealize @ TGrad[TUOpPad[a, {{1, 1}}], a];
    Normal @ TTensorData[g],
    {1.0, 1.0},
    TestID -> "grad/pad-shrinks-cotangent-to-inner"
]

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    (* d(sum(PAD(a*a, {{1,2}})))/d(a) = 2a -- the sum collapses to
       a scalar cotangent broadcast back to the padded extent;
       SHRINK back to a's length recovers 2a element-wise. *)
    expr = TUOpReduce[TUOpPad[TUOpMul[a, a], {{1, 2}}], 0, "SUM"];
    g = TRealize @ TGrad[expr, a];
    Normal @ TTensorData[g],
    {2.0, 4.0, 6.0},
    TestID -> "grad/pad-inside-mul-chain"
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

(* === LOG2: d(log2 x)/dx = 1 / (x * ln 2) === *)

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 4.0}, "Real32"];
    (* d(log2 a)/da = 1 / (a * ln 2) *)
    g = TRealize @ TGrad[TUOpLog2[a], a];
    Normal @ TTensorData[g],
    1.0 / (Log[2] * {1.0, 2.0, 4.0}),
    SameTest -> (Max[Abs[#1 - #2]] < 1.0*^-5 &),
    TestID -> "grad/log2-derivative"
]

(* === EXP2: d(2^x)/dx = 2^x * ln(2) === *)

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    (* d(2^a)/da = ln(2) * 2^a = ln(2) * {2, 4, 8} *)
    g = TRealize @ TGrad[TUOpExp2[a], a];
    Normal @ TTensorData[g],
    Log[2] * {2.0, 4.0, 8.0},
    SameTest -> (Max[Abs[#1 - #2]] < 1.0*^-5 &),
    TestID -> "grad/exp2-derivative"
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

(* === Rank-up EXPAND in expand_to_target ===
   Regression for the rank-changing EXPAND fix.  Pre-fix, the
   leaf rule's expand_to_target collapsed rank-2 targets to
   rank-1 because the materializer inferred EXPAND's ndim from
   the source's rank.  Now ndim is stored explicitly. *)

VerificationTest[
    TInit[];
    t = TTensorCreate @ NumericArray[
        {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}}, "Real32"];
    g = TRealize @ TGrad[t, t];   (* identity leaf, rank-2 *)
    Normal @ TTensorData[g],
    {{1.0, 1.0, 1.0}, {1.0, 1.0, 1.0}},
    TestID -> "grad/rank-2-leaf-identity"
]

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[
        {{1.0, 2.0}, {3.0, 4.0}}, "Real32"];
    b = TTensorCreate @ NumericArray[
        {{10.0, 20.0}, {30.0, 40.0}}, "Real32"];
    (* d(a + b)/da = 1 in a's full {2,2} shape. *)
    g = TRealize @ TGrad[TUOpAdd[a, b], a];
    Normal @ TTensorData[g],
    {{1.0, 1.0}, {1.0, 1.0}},
    TestID -> "grad/rank-2-add-w-r-t-a"
]

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[
        {{2.0, 3.0}, {4.0, 5.0}}, "Real32"];
    (* d(a*a)/da = 2a in a's {2,2} shape. *)
    g = TRealize @ TGrad[TUOpMul[a, a], a];
    Normal @ TTensorData[g],
    {{4.0, 6.0}, {8.0, 10.0}},
    TestID -> "grad/rank-2-x-times-x-equals-2x"
]

(* === MatVec-style backward (REDUCE_SUM axis=1 of MUL[w, x_expanded]).
   Mirrors the inner backbone of TLinear's gradient.  Asserts that
   d/dw of REDUCE_SUM(MUL(w, expand(x, w.shape))) over the inner
   axis equals expand(x, w.shape) -- i.e. the gradient is x
   broadcast back to w's shape.  Acts as a CPU-vs-Metal parity
   regression: the per-pattern probes used during the
   "Investigate Metal-vs-CPU gradient parity in MLP" task all
   matched between backends, but having this in the test sweep
   means future Metal kernel work can't silently regress it. *)
VerificationTest[
    TInit[];
    w  = TTensorCreate @ NumericArray[{{1.0, 2.0}, {3.0, 4.0}}, "Real32"];
    xb = TUOpExpand[
            TTensorCreate @ NumericArray[{{0.5, 1.5}}, "Real32"],
            {2, 2}];
    g  = TRealize @ TGrad[TUOpReduce[TUOpMul[w, xb], 1, "SUM"], w];
    Normal @ TTensorData[g],
    {{0.5, 1.5}, {0.5, 1.5}},
    TestID -> "grad/matvec-style-backward"
]

(* === Softmax + cross-entropy: d/dz = probs - target ===
   Regression for the TSoftmax cross-coupling fix.  Pre-fix,
   TSoftmax used implicit numel-cycle broadcast in
   MUL[e, RECIP(SUM(e))] and the MUL chain rule missed the
   implicit broadcast, dropping the +probs_i cross-coupling
   term on every non-target index.  The explicit TUOpExpand in
   TSoftmax + the EXPAND grad rule's REDUCE-along-broadcast-axes
   together restore the textbook gradient. *)

VerificationTest[
    TInit[];
    z      = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    target = TTensorCreate @ NumericArray[{0.0, 1.0, 0.0}, "Real32"];
    loss   = TCrossEntropyLoss[TSoftmax[z], target];
    g      = TRealize @ TGrad[loss, z];
    Normal @ TTensorData[g],
    (* probs ~ {0.0900306, 0.244728, 0.665241}; target = {0,1,0};
       expected grad = probs - target. *)
    {0.0900306, 0.244728 - 1.0, 0.665241},
    SameTest -> (Max[Abs[#1 - #2]] < 1.0*^-4 &),
    TestID -> "grad/softmax-cross-entropy-equals-probs-minus-target"
]

(* === REDUCE_MAX gradient: one-hot at argmax ===
   d(REDUCE_MAX(a, axis))/da is 1 at the argmax position along
   the reduced axis, 0 elsewhere.  Built via the CMPEQ argmax
   mask: (a == EXPAND(REDUCE_MAX(a), a.shape)). *)

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 5.0, 3.0, 2.0}, "Real32"];
    g = TRealize @ TGrad[TUOpReduce[a, 0, "MAX"], a];
    Normal @ TTensorData[g],
    {0.0, 1.0, 0.0, 0.0},
    TestID -> "grad/reduce-max-one-hot-at-argmax"
]

VerificationTest[
    TInit[];
    (* 2x2-pool-style probe: reshape {4} to {2,2}, REDUCE_MAX
       along the inner axis, sum the per-row maxes.  d/da should
       mark the argmax of each row.  Inputs: row 0 = {1, 3} -> max
       at index 1; row 1 = {2, 4} -> max at index 1.  So flat
       grad = {0, 1, 0, 1}.  Pre-fix this depended on axis-aware
       EXPAND landing on both backends -- now it does. *)
    a = TTensorCreate @ NumericArray[{1.0, 3.0, 2.0, 4.0}, "Real32"];
    pooled = TUOpReduce[TUOpReshape[a, {2, 2}], 1, "MAX"];
    summed = TUOpReduce[pooled, 0, "SUM"];
    g = TRealize @ TGrad[summed, a];
    Normal @ TTensorData[g],
    {0.0, 1.0, 0.0, 1.0},
    TestID -> "grad/pool-style-reshape-reduce-max-then-sum"
]

(* === CONV2D grad_bias: REDUCE_SUM gy over spatial axes ===
   Bias is broadcast across every output spatial position, so
   d(loss)/d(bias[c_out]) = sum over (y, x) of gy[c_out, y, x].
   With CONST(1) seed and a 4x4 input, 3x3 weights, 2-channel
   output: gy lifts to {2, 2, 2} (C_out=2, H_out=W_out=2),
   sum over spatial = 2*2 = 4 per channel.  Expected
   bias-grad = {4, 4}. *)

VerificationTest[
    TInit[];
    input   = TTensorCreate @ NumericArray[
        ConstantArray[1.0, {1, 4, 4}], "Real32"];
    weights = TTensorCreate @ NumericArray[
        ConstantArray[0.0, {2, 1, 3, 3}], "Real32"];
    bias    = TTensorCreate @ NumericArray[{0.0, 0.0}, "Real32"];
    g = TRealize @ TGrad[TUOpConv2D[input, weights, bias], bias];
    Normal @ TTensorData[g],
    {4.0, 4.0},
    TestID -> "grad/conv2d-bias-equals-spatial-sum-of-gy"
]

(* === CONV2D grad_weights (C_in=1 case) ===
   Cross-correlation identity: gw[c_out, 0, ky, kx]
     = sum_{y, x} input[0, y+ky, x+kx] * gy[c_out, y, x].
   With input = ones{1, 4, 4}, weights = zeros{2, 1, 3, 3},
   bias = zeros{2}; CONST(1) seed -> gy lifted = ones{2, 2, 2};
   gw[c, 0, ky, kx] = sum of a 2x2 ones slice = 4 for all
   (c, ky, kx).  Expected: ones{2, 1, 3, 3} * 4. *)

VerificationTest[
    TInit[];
    input   = TTensorCreate @ NumericArray[
        ConstantArray[1.0, {1, 4, 4}], "Real32"];
    weights = TTensorCreate @ NumericArray[
        ConstantArray[0.0, {2, 1, 3, 3}], "Real32"];
    bias    = TTensorCreate @ NumericArray[{0.0, 0.0}, "Real32"];
    g = TRealize @ TGrad[TUOpConv2D[input, weights, bias], weights];
    Normal @ TTensorData[g],
    ConstantArray[4.0, {2, 1, 3, 3}],
    SameTest -> (Max[Abs[Flatten[#1] - Flatten[#2]]] < 1.0*^-5 &),
    TestID -> "grad/conv2d-weights-cin1-equals-spatial-correlation"
]

(* === CONV2D grad_input via transposed conv ===
   d(loss)/d(input)[c_in, y, x]
     = sum_{c_out, ky, kx, with valid bounds}
         gy[c_out, y - ky, x - kx] * weights[c_out, c_in, ky, kx]
   Concrete probe: input zeros{1, 4, 4}, weights = ones{1, 1, 3, 3},
   bias = zeros{1}; CONST(1) seed -> gy lifted = ones{1, 2, 2}.
   d/dinput at corners is small (only 1 weight position contributes),
   d/dinput at center is 4 (all 4 weight-position windows hit it).
   Expected output is the "valid-conv coverage map" of a 2x2 ones
   gy with 3x3 ones weights:
     {{1,2,2,1},
      {2,4,4,2},
      {2,4,4,2},
      {1,2,2,1}}.  *)

VerificationTest[
    TInit[];
    input   = TTensorCreate @ NumericArray[
        ConstantArray[0.0, {1, 4, 4}], "Real32"];
    weights = TTensorCreate @ NumericArray[
        ConstantArray[1.0, {1, 1, 3, 3}], "Real32"];
    bias    = TTensorCreate @ NumericArray[{0.0}, "Real32"];
    g = TRealize @ TGrad[TUOpConv2D[input, weights, bias], input];
    Normal @ TTensorData[g],
    {{{1.0, 2.0, 2.0, 1.0},
      {2.0, 4.0, 4.0, 2.0},
      {2.0, 4.0, 4.0, 2.0},
      {1.0, 2.0, 2.0, 1.0}}},
    SameTest -> (Max[Abs[Flatten[#1] - Flatten[#2]]] < 1.0*^-5 &),
    TestID -> "grad/conv2d-input-equals-coverage-map"
]

(* === CONV2D grad_weights for C_in > 1 (diagonal-mask trick) ===
   C_in = 2 case.  input{2, 4, 4} with channel 0 = ones, channel
   1 = twos.  weights = zeros{1, 2, 3, 3}; bias = zeros{1};
   CONST(1) seed -> gy lifted = ones{1, 2, 2}.
   gw[0, 0, ky, kx] = sum over (y,x) of input[0, y+ky, x+kx] * 1
                    = sum of 2x2 ones slice = 4.
   gw[0, 1, ky, kx] = sum of 2x2 twos slice = 8.
   So expected: {{4-grid filled with 4}, {3-grid filled with 8}}. *)

VerificationTest[
    TInit[];
    inputData = Join[
        ConstantArray[1.0, {1, 4, 4}],   (* channel 0 = 1s *)
        ConstantArray[2.0, {1, 4, 4}]];  (* channel 1 = 2s *)
    input   = TTensorCreate @ NumericArray[inputData, "Real32"];
    weights = TTensorCreate @ NumericArray[
        ConstantArray[0.0, {1, 2, 3, 3}], "Real32"];
    bias    = TTensorCreate @ NumericArray[{0.0}, "Real32"];
    g = TRealize @ TGrad[TUOpConv2D[input, weights, bias], weights];
    Normal @ TTensorData[g],
    {{ConstantArray[4.0, {3, 3}],
      ConstantArray[8.0, {3, 3}]}},
    SameTest -> (Max[Abs[Flatten[#1] - Flatten[#2]]] < 1.0*^-5 &),
    TestID -> "grad/conv2d-weights-cin2-diagonal-mask-trick"
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
