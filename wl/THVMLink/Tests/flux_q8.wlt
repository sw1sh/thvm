(* flux_q8.wlt - FLUX's 8-bit weights (fxQuantizeWeight, the linear on them in NN/Examples/FLUX): a weight
   is its row's scale times an integer from -127 to 127, the nearest one.  Small random matrices on the
   default device; no model is loaded. *)

Get["WolframInstitute`THVMLink`Examples`"]

$q8 := $q8 = Module[{w, weight, q},
    TInit[]; TReset[];
    SeedRandom[11];
    weight = TTensorCreate[RandomVariate[NormalDistribution[0, 0.02], {48, 320}], "bf16"];
    w = Normal[TRealize[TUOpCast[weight, "f32"]]];
    q = fxQuantizeWeight[weight];
    <|"Weight" -> weight, "W" -> w, "Quantized" -> q, "Q" -> Normal[TRealize[TUOpCast[q["q"], "i32"]]],
      "S" -> Normal[TRealize[TUOpCast[q["s"], "f32"]]]|>]

(* The integers are int8 from -127 to 127, one scale a row, and each row's largest weight is at 127. *)
VerificationTest[
    {TTensorDType[$q8["Quantized"]["q"]], Dimensions[$q8["Q"]], Dimensions[$q8["S"]], MinMax[$q8["Q"]],
     Union[Max /@ Abs[$q8["Q"]]]},
    {"i8", {48, 320}, {48}, {-127, 127}, {127}},
    TestID -> "flux-q8/int8-with-a-scale-per-row"
]

(* Each integer is the nearest one to the weight over its row's scale: no further than half a step (the
   0.001 is for the device's single precision), and no smaller in size on average, as a truncated one
   would be by half a step. *)
VerificationTest[
    With[{steps = Clip[$q8["W"] / $q8["S"], {-127., 127.}]},
        {Max[Abs[$q8["Q"] - steps]] < 0.501, Abs[Mean[Flatten[Abs[$q8["Q"]] - Abs[steps]]]] < 0.01}],
    {True, True},
    TestID -> "flux-q8/rounded-to-the-nearest"
]

(* So the 8-bit weights are no smaller on the whole: their projection on the weights is 1 to a part in a
   thousand (truncation gives 0.98). *)
VerificationTest[
    With[{back = $q8["Q"] * $q8["S"]},
        Abs[Total[Flatten[back * $q8["W"]]] / Total[Flatten[$q8["W"]^2]] - 1] < 0.001],
    True,
    TestID -> "flux-q8/not-shrunk"
]

(* The linear on the 8-bit weight is the linear on the weight to the quantization error: under 2 percent
   of the output, at its scale. *)
VerificationTest[
    Module[{x, y, y8},
        SeedRandom[12];
        x = TTensorCreate[RandomVariate[NormalDistribution[], {6, 320}]];
        y = Normal[TRealize[fxLinear[x, $q8["Weight"]]]];
        y8 = Normal[TRealize[fxLinear[x, $q8["Quantized"]]]];
        {Dimensions[y8], Norm[Flatten[y8 - y]] / Norm[Flatten[y]] < 0.02, Abs[Total[Flatten[y8 * y]] / Total[Flatten[y^2]] - 1] < 0.005}],
    {{6, 48}, True, True},
    TestID -> "flux-q8/the-linear-on-8-bit-weights"
]
