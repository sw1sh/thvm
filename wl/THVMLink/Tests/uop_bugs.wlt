(* uop_bugs.wlt -- minimal repros of two thvm bugs surfaced while
   writing wl/THVMLink/Kernel/Einx.wl.  These tests are expected to
   FAIL on master until the underlying rangeify / materialize defects
   are fixed.  The failures are intentional: they're the signal that
   the bugs still exist.  Don't quarantine them in pending_*.wlt and
   don't paper them over with caller-side workarounds.

   Both bugs live below the einx layer.  einx hits them via
   - TEinDot:  WL Times is Orderless -> reorders Mul operands -> bug 1
   - TEinVar:  (x - broadcast(mean))^2 + reduce -> bug 2 *)

(* === Bug 1: TUOpMul is not commutative under Expand + Reduce ====

   Input:   ab = Expand(Reshape(a:{2,2}, {2,2,1}), {2,2,2})
            bb = Expand(Reshape(b:{2,2}, {1,2,2}), {2,2,2})
            r1 = Reduce(TUOpMul[ab, bb], axis = 1, SUM)
            r2 = Reduce(TUOpMul[bb, ab], axis = 1, SUM)

   Expected:  r1 === r2  (Mul is commutative; same axes, same op)
   Observed:  r1 = {{19, 22}, {43, 50}}
              r2 = {{19, 43}, {22, 50}}    -- transposed!

   The result of r1 matches the textbook a.b matmul; r2 is the
   transpose.  Hypothesis: rangeify / materialize derives the
   output strides from the FIRST Mul operand, so swapping operands
   silently rotates the reduce's stride pattern.

   Knock-on: WL's `*` is Orderless and will reorder Mul operands
   based on TTerm raw IDs; any callsite using `*` followed by a
   reduce is at risk. *)

VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{{1.0, 2.0}, {3.0, 4.0}}, "Real32"];
    b = TTensorCreate @ NumericArray[{{5.0, 6.0}, {7.0, 8.0}}, "Real32"];
    ab = TUOpExpand[TUOpReshape[a, {2, 2, 1}], {2, 2, 2}];
    bb = TUOpExpand[TUOpReshape[b, {1, 2, 2}], {2, 2, 2}];
    r1 = Normal @ TTensorData @ TRealize @ TUOpReduce[TUOpMul[ab, bb], 1, "SUM"];
    r2 = Normal @ TTensorData @ TRealize @ TUOpReduce[TUOpMul[bb, ab], 1, "SUM"];
    r1 === r2,
    True,
    TestID -> "thvm/uop/mul-not-commutative-under-expand-reduce"
]

(* Verifies WL Times Orderless hits the same path: TUOpMul[ab,bb]
   gives correct matmul; `ab * bb` rolls the dice on operand order. *)
VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{{1.0, 2.0}, {3.0, 4.0}}, "Real32"];
    b = TTensorCreate @ NumericArray[{{5.0, 6.0}, {7.0, 8.0}}, "Real32"];
    ab = TUOpExpand[TUOpReshape[a, {2, 2, 1}], {2, 2, 2}];
    bb = TUOpExpand[TUOpReshape[b, {1, 2, 2}], {2, 2, 2}];
    explicit = Normal @ TTensorData @ TRealize @ TUOpReduce[TUOpMul[ab, bb], 1, "SUM"];
    starred  = Normal @ TTensorData @ TRealize @ TUOpReduce[ab * bb, 1, "SUM"];
    explicit === starred,
    True,
    TestID -> "thvm/uop/times-orderless-vs-tuopmul-differ"
]

(* === Bug 2: reduce-of-(diff * diff) drifts from realized form ===

   Input:   term       :{1, 4}
            mean       = Reduce(term, axis 1) / 4
            broadcast  = Expand(Reshape(mean, {1, 1}), {1, 4})
            diff       = term - broadcast
            sq         = diff * diff
            sumLazy    = Reduce(sq, axis 1, SUM)
            sumFromReal = Reduce(Realize(sq), axis 1, SUM)

   Concrete with x = [1, 2, 3, 4]:
     mean      = 2.5
     diff      = [-1.5, -0.5, 0.5, 1.5]
     sq prints = [2.25, 0.25, 0.25, 2.25]  (both lazy and realized)
     Expected  sum = 5.0
     Observed  sumFromReal = 30.0   (= sum of x^2; the broadcast / sub
                                       got dropped on the realize path)
               sumLazy     = 31.25  (= sum(x^2) + Var(x); something
                                       else again -- distinct from
                                       both expected and realized)

   So the printed intermediate is correct but the chain Realize / Reduce
   both diverge.  Both behaviours are wrong; they disagree with each
   other in addition to disagreeing with the math.  Likely a rangeify
   issue with re-using a reduce-result (the mean) inside a downstream
   chain that's then reduced again. *)

VerificationTest[
    TInit[];
    x = TTensorCreate @ NumericArray[{{1.0, 2.0, 3.0, 4.0}}, "Real32"];
    mean = TUOpReduce[x, 1, "SUM"] / 4;
    meanB = TUOpExpand[TUOpReshape[mean, {1, 1}], {1, 4}];
    diff = x - meanB;
    sq = diff * diff;
    Normal @ TTensorData @ TRealize @ TUOpReduce[sq, 1, "SUM"],
    {5.0},
    TestID -> "thvm/uop/reduce-of-bcast-sub-square"
]

VerificationTest[
    TInit[];
    x = TTensorCreate @ NumericArray[{{1.0, 2.0, 3.0, 4.0}}, "Real32"];
    mean = TUOpReduce[x, 1, "SUM"] / 4;
    meanB = TUOpExpand[TUOpReshape[mean, {1, 1}], {1, 4}];
    diff = x - meanB;
    sq = diff * diff;
    (* Realized form should match the lazy reduce: both should be {5.0}. *)
    Normal @ TTensorData @ TRealize @ sq,
    {{2.25, 0.25, 0.25, 2.25}},
    TestID -> "thvm/uop/realized-square-matches-print"
]

VerificationTest[
    TInit[];
    x = TTensorCreate @ NumericArray[{{1.0, 2.0, 3.0, 4.0}}, "Real32"];
    mean = TUOpReduce[x, 1, "SUM"] / 4;
    meanB = TUOpExpand[TUOpReshape[mean, {1, 1}], {1, 4}];
    diff = x - meanB;
    sq = diff * diff;
    sqR = TRealize @ sq;
    fresh = Normal @ TTensorData @ TRealize @ TUOpReduce[sqR, 1, "SUM"];
    lazy  = Normal @ TTensorData @ TRealize @ TUOpReduce[sq, 1, "SUM"];
    {fresh, lazy, fresh === lazy},
    {{5.0}, {5.0}, True},
    TestID -> "thvm/uop/realized-then-reduce-matches-lazy-reduce"
]
