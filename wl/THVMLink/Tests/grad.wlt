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

(* The pool-style probe (RESHAPE + REDUCE_MAX along an inner axis)
   is deferred -- the chain works structurally but exposes a deeper
   bug in cpu_op_expand: when expanding a rank-N source to a larger
   rank-N target along a non-leading axis (e.g. {2} -> {2,2} where
   each src element repeats along a NEW axis), the kernel falls to
   numel-cycling and produces {3,4,3,4} instead of the correct
   {3,3,4,4}.  Tracked as a follow-up "Axis-aware EXPAND in CPU
   kernel" task in TASKS.md. *)

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
