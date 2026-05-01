(* tile_uops.wlt -- tests for the TileUop introspection layer above
   scalar-UOp rangeify lowering. *)

SetAttributes[tileWithRangeify, HoldRest];
tileWithRangeify[on_, body_] := Module[{prev, r},
    prev = Environment["THVM_RANGEIFY"];
    If[ prev === $Failed, prev = None];
    SetEnvironment["THVM_RANGEIFY" -> If[ on, "1", "0"]];
    r = body;
    SetEnvironment["THVM_RANGEIFY" -> prev];
    r
]

tileSimpleAdd[] := (
    TInit[];
    a = TTensorCreate @ NumericArray[{1.0, 2.0, 3.0}, "Real32"];
    b = TTensorCreate @ NumericArray[{4.0, 5.0, 6.0}, "Real32"];
    TRealize[a + b];
)

tileAxisSig[plan_] := ({#["axis_type"], #["extent"]} & /@ plan["axes"])

VerificationTest[
    TInit[];
    TKernelTileUops[0],
    Missing["NotLowered"],
    TestID -> "tile-uops/sentinel-slot-missing"
]

VerificationTest[
    TInit[];
    Quiet @ Check[TKernelTileUops[TKernelCount[] + 100],
                  Missing["OutOfRange"]],
    Missing["OutOfRange"] | _Missing,
    SameTest -> MatchQ,
    TestID -> "tile-uops/out-of-range-graceful"
]

VerificationTest[
    tileWithRangeify[True,
      tileSimpleAdd[];
      uops = TKernelTileUops[1];
      KeyTake[Counts[#["op"] & /@ uops],
              {"TILE_NONE", "TILE_SCALAR_BODY", "TILE_STORE",
               "TILE_AXIS", "TILE_LOOP_NEST"}]
    ],
    <|"TILE_NONE" -> 1, "TILE_SCALAR_BODY" -> 1, "TILE_STORE" -> 1,
      "TILE_AXIS" -> 1, "TILE_LOOP_NEST" -> 1|>,
    TestID -> "tile-uops/elementwise-add-shape"
]

VerificationTest[
    tileWithRangeify[True,
      tileSimpleAdd[];
      plan = TKernelTilePlan[1];
      {plan["root"], plan["store_tile"], plan["body_tile"],
       plan["dtype"], plan["axes"]}
    ],
    {4, 2, 1, "f32",
     {<|"id" -> 3, "axis_type" -> "LOOP", "extent" -> 3|>}},
    TestID -> "tile-uops/elementwise-add-plan"
]

VerificationTest[
    tileWithRangeify[True,
      TInit[];
      a = TTensorCreate @ NumericArray[Table[N[i], {i, 8}], "Real32"];
      TRealize @ TUOpMul[a, a];
      kid = TKernelCount[] - 1;
      before = tileAxisSig @ TKernelTilePlan[kid];
      TKernelApplyOpt[kid, TOpt["UPCAST", 0, 4]];
      after = tileAxisSig @ TKernelTilePlan[kid];
      {before, after}
    ],
    {{{"LOOP", 8}}, {{"LOOP", 2}, {"UPCAST", 4}}},
    TestID -> "tile-uops/apply-upcast-syncs-plan"
]

VerificationTest[
    tileWithRangeify[True,
      TInit[];
      xT = TTensorCreate @ N @ Range[12];
      TRealize @ TUOpReduce[xT, 0, "SUM"];
      kid = TKernelCount[] - 1;
      TKernelApplyOpt[kid, TOpt["UNROLL", 1, 4]];
      tileAxisSig @ TKernelTilePlan[kid]
    ],
    {{"LOOP", 1}, {"REDUCE", 3}, {"UNROLL", 4}},
    TestID -> "tile-uops/apply-unroll-syncs-reduce-plan"
]

VerificationTest[
    tileWithRangeify[True,
      TInit[];
      a = TTensorCreate @ NumericArray[Table[N[i], {i, 8}], "Real32"];
      TRealize @ TUOpMul[a, a];
      kid = TKernelCount[] - 1;
      TKernelApplyOpt[kid, TOpt["UPCAST", 0, 4]];
      TKernelApplyOpt[kid, TOpt["SWAP", 0, 1]];
      tileAxisSig @ TKernelTilePlan[kid]
    ],
    {{"UPCAST", 4}, {"LOOP", 2}},
    TestID -> "tile-uops/apply-swap-syncs-plan"
]

VerificationTest[
    tileWithRangeify[True,
      tileSimpleAdd[];
      plan = TKernelTilePlan[1];
      uops = TKernelTileUops[1];
      root = uops[[plan["root"] + 1]];
      store = uops[[plan["store_tile"] + 1]];
      body = uops[[plan["body_tile"] + 1]];
      {root["op"], root["src"], store["op"], store["src"], body["op"]}
    ],
    {"TILE_LOOP_NEST", {2, 3}, "TILE_STORE", {1}, "TILE_SCALAR_BODY"},
    TestID -> "tile-uops/root-store-body-links"
]

VerificationTest[
    tileWithRangeify[True,
      tileSimpleAdd[];
      plan = TKernelTilePlan[1];
      scalar = TKernelScalarUops[1];
      store = scalar[[plan["scalar_store"] + 1]];
      {store["op"],
       store["src"] === {plan["scalar_index"], plan["scalar_value"]}}
    ],
    {"S_STORE", True},
    TestID -> "tile-uops/scalar-store-boundary"
]

VerificationTest[
    tileWithRangeify[False,
      tileSimpleAdd[];
      TKernelTilePlan[1]
    ],
    Missing["NotLowered"],
    TestID -> "tile-uops/rangeify-off-missing"
]
