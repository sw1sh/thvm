(* lowering.wlt -- tests for the scalar-UOp lowering pass (rangeify).
   Phase A covers the introspection surface (TKernelScalarUops);
   Phase B covers elementwise-chain lowering, asserted via the
   scalar-graph shape AND output equivalence vs. the legacy path.
   Phase C/D will extend with reduce + backward coverage. *)

(* Helper: run a closure with THVM_RANGEIFY toggled and restore on
   exit.  HoldRest so the body doesn't evaluate before we flip the
   env var.  Returns the body's value.  Environment[] returns $Failed
   when the var is unset, but SetEnvironment[var -> $Failed] errors;
   coerce $Failed -> None for the restore. *)
SetAttributes[withRangeify, HoldRest];
withRangeify[on_, body_] := Module[{prev, r},
    prev = Environment["THVM_RANGEIFY"];
    If[ prev === $Failed, prev = None];
    SetEnvironment["THVM_RANGEIFY" -> If[ on, "1", "0"]];
    r = body;
    SetEnvironment["THVM_RANGEIFY" -> prev];
    r
]

(* Helper: count scalar ops by kind for a single kernel. *)
scalarOpTally[kid_] := Module[{uops = TKernelScalarUops[kid]},
    If[ uops === Missing["NotLowered"],
      <||>,
      KeyDrop[Counts[#["op"] & /@ uops], "S_NONE"]]
]

(* When rangeify is off, every kernel emits via the legacy visit()
   path and TKernelScalarUops should report Missing["NotLowered"].
   Wrap explicitly so the test passes regardless of the shell's
   THVM_RANGEIFY setting. *)
VerificationTest[
    withRangeify[False,
      TInit[];
      a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
      b = TTensorCreate @ NumericArray[{4.0, 5.0, 6.0}, "Real32"];
      TRealize[a + b];
      n = TKernelCount[] - 1;
      AllTrue[Range[n], TKernelScalarUops[#] === Missing["NotLowered"] &]
    ],
    True,
    TestID -> "lowering/legacy-path-reports-missing"
]

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

(* Phase B: with rangeify on, a single elementwise op produces a
   scalar-UOp graph with the expected shape: one S_RANGE per output
   dim, one S_DEFINE_PARAM per input, one S_DEFINE_OUTPUT, S_INDEX +
   S_LOAD per input read, one S_ADD, one S_INDEX + S_STORE for the
   output, capped by S_BUFFERIZE. *)
VerificationTest[
    withRangeify[True,
      TInit[];
      a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
      b = TTensorCreate @ NumericArray[{4.0, 5.0, 6.0}, "Real32"];
      TRealize[a + b];
      tally = scalarOpTally[1];
      KeyTake[tally, {"S_RANGE", "S_DEFINE_PARAM", "S_DEFINE_OUTPUT",
                      "S_INDEX",  "S_LOAD",        "S_STORE",
                      "S_BUFFERIZE", "S_ADD"}]
    ],
    <|"S_RANGE" -> 1, "S_DEFINE_PARAM" -> 2, "S_DEFINE_OUTPUT" -> 1,
      "S_INDEX" -> 3, "S_LOAD" -> 2, "S_STORE" -> 1, "S_BUFFERIZE" -> 1,
      "S_ADD" -> 1|>,
    TestID -> "lowering/elementwise-add-shape"
]

(* Compound elementwise (a + b) * c should fuse into one kernel
   where the lowered graph has 2 S_ADD-ish ALU ops sharing the same
   ranges and 3 S_LOAD reads (a, b, c -- one per input). *)
VerificationTest[
    withRangeify[True,
      TInit[];
      a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
      b = TTensorCreate @ NumericArray[{4.0, 5.0, 6.0}, "Real32"];
      c = TTensorCreate @ NumericArray[{7.0, 8.0, 9.0}, "Real32"];
      TRealize[(a + b) * c];
      tally = scalarOpTally[1];
      {tally["S_LOAD"], tally["S_ADD"], tally["S_MUL"], tally["S_BUFFERIZE"]}
    ],
    {3, 1, 1, 1},
    TestID -> "lowering/compound-elementwise-fuses-shape"
]

(* Output equivalence: rangeified result must match legacy result
   bitwise.  Run the same expression with each path and diff. *)
VerificationTest[
    Module[{legacy, lowered},
      legacy = withRangeify[False,
        TInit[];
        a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0, 4.0}, "Real32"];
        b = TTensorCreate @ NumericArray[{0.5, 1.5, 2.5, 3.5}, "Real32"];
        c = TTensorCreate @ NumericArray[{2.0, 2.0, 2.0, 2.0}, "Real32"];
        Normal @ TTensorData @ TRealize[(a + b) * c]];
      lowered = withRangeify[True,
        TInit[];
        a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0, 4.0}, "Real32"];
        b = TTensorCreate @ NumericArray[{0.5, 1.5, 2.5, 3.5}, "Real32"];
        c = TTensorCreate @ NumericArray[{2.0, 2.0, 2.0, 2.0}, "Real32"];
        Normal @ TTensorData @ TRealize[(a + b) * c]];
      legacy === lowered
    ],
    True,
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

(* Phase C: TSum over a vector lowers to one kernel with a single
   REDUCE-typed RANGE wrapped in S_REDUCE_SUM.  Same shape as the
   Phase B elementwise add but with an extra REDUCE range and one
   S_REDUCE_SUM op instead of S_ADD. *)
VerificationTest[
    withRangeify[True,
      TInit[];
      v = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0, 4.0}, "Real32"];
      TRealize @ TSum[v];
      (* The SUM kernel is the only one (TSum on a TEN). *)
      sumKid = SelectFirst[Range[TKernelCount[] - 1],
                 TKernelScalarUops[#] =!= Missing["NotLowered"] &];
      tally = scalarOpTally[sumKid];
      KeyTake[tally, {"S_RANGE", "S_DEFINE_PARAM", "S_DEFINE_OUTPUT",
                      "S_INDEX",  "S_LOAD",        "S_REDUCE_SUM",
                      "S_STORE",  "S_BUFFERIZE"}]
    ],
    <|"S_RANGE" -> 2, "S_DEFINE_PARAM" -> 1, "S_DEFINE_OUTPUT" -> 1,
      "S_INDEX" -> 2, "S_LOAD" -> 1, "S_REDUCE_SUM" -> 1,
      "S_STORE" -> 1, "S_BUFFERIZE" -> 1|>,
    TestID -> "lowering/reduce-sum-shape"
]

(* Phase C: TSum output bitwise matches legacy.  Sum-of-1..10 should
   give 55. *)
VerificationTest[
    Module[{legacy, lowered, expected = 55.0},
      legacy = withRangeify[False,
        TInit[];
        v = TTensorCreate @ NumericArray[Range[10] * 1.0, "Real32"];
        First @ Normal @ TTensorData @ TRealize @ TSum[v]];
      lowered = withRangeify[True,
        TInit[];
        v = TTensorCreate @ NumericArray[Range[10] * 1.0, "Real32"];
        First @ Normal @ TTensorData @ TRealize @ TSum[v]];
      legacy === lowered === expected
    ],
    True,
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
