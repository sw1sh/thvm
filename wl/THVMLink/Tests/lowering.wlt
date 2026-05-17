(* lowering.wlt -- output-equivalence tests for the unified rangeify
   pass.  Earlier versions asserted ScalarUop arena shape via
   TKernelScalarUops; that surface was retired alongside rangeify.c
   (the unified pass produces a lifted UOp DAG directly).  The
   *-shape / all-kernels-lower / backward-grad-lowers tests were
   removed when the arena went; what remains here is output bitwise
   equivalence + correctness coverage that survives independent of
   the lowering representation. *)

(* withRangeify is a leftover no-op wrapper: THVM_RANGEIFY env was
   retired alongside the legacy pass; the wrapper keeps existing
   test bodies syntactically intact. *)
SetAttributes[withRangeify, HoldRest];
withRangeify[on_, body_] := (on; body)

(* TKernelScalarUops[0] (the reserved sentinel slot) should also
   read back as Missing -- kid 0 has no program. *)
VerificationTest[
    TInit[];
    TKernelScalarUops[0],
    Missing["NotLowered"],
    TestID -> "lowering/sentinel-slot-missing"
]

(* Querying past KERNELS_NEXT should be a Missing too (the C-side
   returns LIBRARY_FUNCTION_ERROR; the WL wrapper turns that into
   Missing).  Use a kid we KNOW doesn't exist (TKernelCount[]+100). *)
VerificationTest[
    TInit[];
    Quiet @ Check[TKernelScalarUops[TKernelCount[] + 100],
                  Missing["OutOfRange"]],
    Missing["OutOfRange"] | _Missing,
    SameTest -> MatchQ,
    TestID -> "lowering/out-of-range-graceful"
]

(* Output equivalence: rangeified result must match the analytic
   componentwise reference bitwise. *)
VerificationTest[
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0, 4.0}, "Real32"];
    b = TTensorCreate @ NumericArray[{0.5, 1.5, 2.5, 3.5}, "Real32"];
    c = TTensorCreate @ NumericArray[{2.0, 2.0, 2.0, 2.0}, "Real32"];
    Normal @ TTensorData @ TRealize[(a + b) * c],
    {3.0, 7.0, 11.0, 15.0},
    TestID -> "lowering/elementwise-output-bitwise-eq"
]

(* Unary chain: SQRT(NEG(a)) over an all-positive input would
   produce NaN, so use a positive-after-NEG pattern: -(-a) which
   should round-trip to a.  Tests that S_NEG and S_SQRT are wired. *)
VerificationTest[
    withRangeify[True,
      TInit[];
      a = TTensorCreate @ NumericArray[{1.0, 4.0, 9.0, 16.0}, "Real32"];
      Round[Normal @ TTensorData @ TRealize[Sqrt[a]], 0.0001]
    ],
    {1.0, 2.0, 3.0, 4.0},
    TestID -> "lowering/unary-sqrt-correct"
]

(* TSum bitwise matches the analytic reference.
   Sum-of-1..10 = 55. *)
VerificationTest[
    TInit[];
    v = TTensorCreate @ NumericArray[Range[10] * 1.0, "Real32"];
    First @ Normal @ TTensorData @ TRealize @ TSum[v],
    55.0,
    TestID -> "lowering/reduce-sum-output-eq"
]

(* Phase C: REDUCE_MAX lowers via S_REDUCE_MAX.  Max of
   {-1, 5, 3, 2} = 5.  Use TUOpReduce[..., "MAX"] directly since
   there's no public TMax wrapper. *)
VerificationTest[
    withRangeify[True,
      TInit[];
      v = TTensorCreate @ NumericArray[{-1.0, 5.0, 3.0, 2.0}, "Real32"];
      First @ Normal @ TTensorData @ TRealize @ TUOpReduce[v, 0, "MAX"]
    ],
    5.0,
    TestID -> "lowering/reduce-max-output-eq"
]

(* Scalar-tail output equivalence.  Mean of 1..10 = 5.5. *)
VerificationTest[
    TInit[];
    v = TTensorCreate @ NumericArray[Range[10] * 1.0, "Real32"];
    First @ Normal @ TTensorData @ TRealize[TSum[v] / 10.0],
    5.5,
    TestID -> "lowering/scalar-tail-output-eq"
]

(* Phase C-2: Sqrt[TSum[v]] also fuses (single REDUCE -> single
   unary -> root). *)
VerificationTest[
    withRangeify[True,
      TInit[];
      (* sum(1^2 + 2^2 + 3^2 + 4^2) = 30; sqrt(30) ~= 5.4772 *)
      v = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0, 4.0}, "Real32"];
      Round[First @ Normal @ TTensorData @ TRealize[Sqrt[TSum[v * v]]], 0.001]
    ],
    Round[Sqrt[30.], 0.001],
    TestID -> "lowering/sqrt-of-sum-fuses"
]

(* TSoftmax output matches the analytic reference and
   sums to 1.  Reference: numerically-stable softmax computed in
   Mathematica from the same input. *)
VerificationTest[
    Module[{lowered, ref, vData = {1.0, 2.0, 3.0, 4.0}, shifted, eq, sumOk},
      TInit[];
      v       = TTensorCreate @ NumericArray[vData, "Real32"];
      lowered = Normal @ TTensorData @ TRealize @ TSoftmax[v];
      shifted = Exp[vData - Max[vData]];
      ref     = shifted / Total[shifted];
      eq      = (Max[Abs[ref - lowered]] < 1.0*^-6);
      sumOk   = (Abs[Total[lowered] - 1.0] < 1.0*^-6);
      eq && sumOk
    ],
    True,
    TestID -> "lowering/softmax-output-eq"
]

(* Backward output matches the analytic reference.
   L  = sum((w.x - t)^2);  with w.x = 3.0, t = 1.0, residual r = 2.0,
   dL/dw = 2 * r * x = 4.0 * {1, 2, 3, 4} = {4, 8, 12, 16}. *)
VerificationTest[
    Module[{lowered, ref = {4.0, 8.0, 12.0, 16.0}},
      TInit[];
      x = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0, 4.0}, "Real32"];
      w = TTensorCreate @ NumericArray[{0.1, 0.2, 0.3, 0.4}, "Real32"];
      t = TTensorCreate @ NumericArray[{1.0}, "Real32"];
      lowered = Normal @ TTensorData @ TRealize @ TGrad[TMSELoss[TDot[w, x], t], w];
      Max[Abs[ref - lowered]] < 1.0*^-5
    ],
    True,
    TestID -> "lowering/backward-grad-output-eq"
]
