#!/usr/bin/env wolframscript
(* metal-gemm-autotune.wls -- focused Metal GEMM TC autotune smoke.

   Builds one f32 matmul, verifies the TILE_MMA/metal-gemm path,
   prints the proposed TC tile-size variants, applies TKernelAutotune,
   then reruns the same matmul and checks numerical agreement against
   Wolfram's host matmul.

   Run from the repo root:

       wolframscript -f wl/Examples/metal-gemm-autotune.wls
       M=128 K=128 N=128 wolframscript -f wl/Examples/metal-gemm-autotune.wls

   Environment knobs:
       M, K, N          GEMM shape: {M,K} . {K,N}
                        defaults: M=64, K=64, N=64
       THVM_BACKEND     defaulted to metal if unset
       THVM_TILE        defaulted to 1 if unset
       THVM_AUTOTUNE_RUNS
                        defaulted to 9 if unset
*)

PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];

print[args___] := WriteString["stdout",
    StringJoin @@ Map[ToString, {args}], "\n"]

envValue[name_String, default_String] := Module[{v = Environment[name]},
    If[StringQ[v] && StringLength[StringTrim[v]] > 0, v, default]]

parseInt[name_String, default_Integer] := Module[{
    raw = StringTrim @ envValue[name, ToString[default]]
},
    If[StringMatchQ[raw, DigitCharacter ..], ToExpression[raw], default]]

formatOpt[None] := "baseline"
formatOpt[TOpt[op_String, axis_Integer, arg_Integer]] :=
    op <> "[axis=" <> ToString[axis] <> ", arg=" <> ToString[arg] <> "]"

formatOpts[{}] := "{}"
formatOpts[opts_List] := "{" <> StringRiffle[formatOpt /@ opts, ", "] <> "}"

variantRow[v_TKernelVariant] := First[v]

If[envValue["THVM_BACKEND", ""] === "",
    SetEnvironment["THVM_BACKEND" -> "metal"]];
If[envValue["THVM_TILE", ""] === "",
    SetEnvironment["THVM_TILE" -> "1"]];
If[envValue["THVM_AUTOTUNE_RUNS", ""] === "",
    SetEnvironment["THVM_AUTOTUNE_RUNS" -> "9"]];

m = parseInt["M", 64];
k = parseInt["K", 64];
n = parseInt["N", 64];

If[m <= 0 || k <= 0 || n <= 0,
    print["FAIL: M, K, and N must be positive integers"];
    Exit[1]];

TInit[];
TReset[];

print["Metal GEMM autotune"];
print["  THVM_BACKEND=", envValue["THVM_BACKEND", "default-cpu"],
      "  THVM_TILE=", envValue["THVM_TILE", "off"],
      "  THVM_AUTOTUNE_RUNS=", envValue["THVM_AUTOTUNE_RUNS", "5"]];
print["  shape: {", m, ", ", k, "} . {", k, ", ", n, "}"];

ctx = TContextNew["metal"];
If[ctx === 0,
    print["SKIP: Metal context unavailable"];
    Exit[0]];

aData = ArrayReshape[N[Mod[Range[m * k], 17] - 8], {m, k}];
bData = ArrayReshape[N[Mod[Range[k * n], 13] - 6], {k, n}];
ref = aData . bData;

result = TInContext[ctx,
    a = TTensorCreate @ NumericArray[aData, "Real32"];
    b = TTensorCreate @ NumericArray[bData, "Real32"];

    out = TRealize @ TMatMul[a, b];
    kid = TKernelCount[] - 1;
    plan = TKernelTilePlan[kid];
    actual = Normal @ TTensorData[out];
    maxErr = Max[Abs[Flatten[actual - ref]]];

    props = TKernelProposed[kid];
    vars = variantRow /@ TKernelVariants[kid];

    tuned = TKernelAutotune[kid];
    afterTile = TKernelTilePlan[kid]["mma"]["tile_size"];

    out2 = TRealize @ TMatMul[a, b];
    kid2 = TKernelCount[] - 1;
    plan2 = TKernelTilePlan[kid2];
    actual2 = Normal @ TTensorData[out2];
    maxErr2 = Max[Abs[Flatten[actual2 - ref]]];

    <|
        "kid" -> kid,
        "kind" -> TKernelDispatchKind[kid],
        "plan" -> plan["mma"],
        "max_err" -> maxErr,
        "proposed" -> props,
        "variants" -> vars,
        "applied" -> First[tuned]["Applied"],
        "after_tile" -> afterTile,
        "kid2" -> kid2,
        "kind2" -> TKernelDispatchKind[kid2],
        "tile2" -> plan2["mma"]["tile_size"],
        "max_err2" -> maxErr2
    |>
];

TContextDestroy[ctx];

print["  kid: ", result["kid"],
      "  dispatch: ", result["kind"],
      "  default tile: ", result["plan"]["tile_size"]];
print["  mma: M=", result["plan"]["M"],
      " N=", result["plan"]["N"],
      " K=", result["plan"]["K"],
      " Ainput=", result["plan"]["a_input"],
      " Binput=", result["plan"]["b_input"]];
print["  max error before tune: ", Round[result["max_err"], 0.000001]];
print["  proposals: ", formatOpts[result["proposed"]]];

print["  variants:"];
Do[
    print["    ", formatOpt[row["Opt"]], "  ",
          Round[N @ row["WallUs"], 0.001], " us"],
    {row, result["variants"]}];

print["  applied: ", formatOpts[result["applied"]],
      "  selected tile: ", result["after_tile"]];
print["  rerun kid: ", result["kid2"],
      "  dispatch: ", result["kind2"],
      "  tile: ", result["tile2"]];
print["  max error after tune: ", Round[result["max_err2"], 0.000001]];

If[result["kind"] =!= "metal-gemm" || result["kind2"] =!= "metal-gemm",
    print["FAIL: expected metal-gemm dispatch"];
    Exit[1]];

If[result["max_err"] > 0.001 || result["max_err2"] > 0.001,
    print["FAIL: GEMM output diverged from host reference"];
    Exit[1]];

If[Length[result["proposed"]] === 0,
    print["FAIL: expected TC proposals for Metal GEMM"];
    Exit[1]];

print["Metal GEMM autotune PASSED."];
