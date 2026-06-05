(* ::Package:: *)
(* gpt2_weights.wl - extract the GPT-2 117M weights from the Wolfram
   NeuralNetRepository net into the array association TGPT2FromArrays
   consumes.

   The high-level NetModel["GPT2 Transformer Trained on WebText Data"]
   deserialisation is broken in NeuralNetworks 15.0.3 (the EmbeddingLayer
   $Scales array was added after this net was serialised, so the upgrade
   raises an InvalidPort error and NetModel returns $Failed).  The raw
   WLNet file imports fine, so this helper reads it directly with
   Import[..., "WLNet"] and pulls the arrays out of the node tree.  The
   tied token-embedding / LM-head weight lives in the net's SharedArrays.

   Wolfram LinearLayer weights are {out, in}; thvm's TLinear is x . W and
   wants {in, out}, so every projection weight is transposed once here. *)

GPT2WeightFile::usage = "GPT2WeightFile[] returns the path to the cached GPT-2 117M EvaluationNet WLNet file, or $Failed if the resource is not cached.  Download once with NetModel-Evaluate, then this reads it directly (bypassing the broken high-level deserialiser).";
GPT2ExtractArrays::usage = "GPT2ExtractArrays[] imports the cached GPT-2 117M WLNet and returns the weight association TGPT2FromArrays consumes: TokenEmbedding, PositionEmbedding, Blocks (12 per-block param associations), FinalNormScale, FinalNormBias.  All linear weights are transposed to thvm's {in, out} convention and wrapped as TTensorCreate handles.";
GPT2Encoder::usage = "GPT2Encoder[] returns the GPT-2 BPE NetEncoder from the cached net: GPT2Encoder[][text] gives the 1-indexed token ids of a prompt string.";
GPT2Labels::usage = "GPT2Labels[] returns the GPT-2 token-label list (the output NetDecoder's Labels) for host-side decoding: GPT2Labels[][[id]] is the surface string of token `id` (1-indexed, space-prefixed as GPT-2 emits).";

Begin["`GPT2`Private`"];

(* Locate the cached resource file by the GPT-2 117M UUID. *)
$gpt2UUID = "84d61d0e-ae17-4af2-9e13-94be9545de84";

GPT2WeightFile[] := Module[{base, hits},
    base = FileNameJoin[{$UserBaseDirectory, "Objects", "Resources",
        StringTake[$gpt2UUID, 3], $gpt2UUID, "download"}];
    If[ !DirectoryQ[base], Return[$Failed]];
    hits = FileNames["data.WLNet", base, Infinity];
    (* Prefer the weight-bearing EvaluationNet over the Uninitialized one. *)
    SelectFirst[hits,
        StringContainsQ[#, "EvaluationNet"] &&
            !StringContainsQ[#, "Uninitialized"],
        First[hits, $Failed]]
]

rawNet[] := Module[{file, g},
    file = GPT2WeightFile[];
    If[ file === $Failed,
        Message[GPT2ExtractArrays::nofile]; Return[$Failed]];
    (* The WLNet importer needs the NeuralNetworks paclet loaded to
       resolve the stored NumericArrays; without it Import returns lazy
       TensorT placeholders that Normal can't materialise. *)
    Needs["NeuralNetworks`"];
    g = Quiet @ Import[file, "WLNet"];
    g /. NetChain[a_Association, ___] :> a
]
GPT2ExtractArrays::nofile = "GPT-2 117M net not cached.  Run NetModel[\"GPT2 Transformer Trained on WebText Data\"] once (online) to download it, then retry.";

(* Wolfram {out, in} -> thvm {in, out}, wrapped as a TTensorCreate. *)
linWeightT[node_] := TTensorCreate @ NumericArray[
    Transpose @ Normal @ node["Parameters"]["Net"]["Arrays"]["Weights"],
    "Real32"]
linBiasT[node_] := TTensorCreate @ NumericArray[
    Normal @ node["Parameters"]["Net"]["Arrays"]["Biases"], "Real32"]
vecT[arr_] := TTensorCreate @ NumericArray[Normal @ arr, "Real32"]

blockParams[blockChain_] := Module[{g1, g2, att, norm1, norm2},
    g1    = blockChain["Nodes"]["1"];      (* norm + attention + add *)
    g2    = blockChain["Nodes"]["2"];      (* norm + mlp + add *)
    att   = g1["Nodes"]["attention"];
    norm1 = g1["Nodes"]["norm"];
    norm2 = g2["Nodes"]["norm"];
    <|
        "norm1Scale" -> vecT[norm1["Arrays"]["Scaling"]],
        "norm1Bias"  -> vecT[norm1["Arrays"]["Biases"]],
        (* attention sub-NetGraph nodes: 1=Key 2=Query 4=Value 6=Output *)
        "wQ" -> linWeightT[att["Nodes"]["2"]], "bQ" -> linBiasT[att["Nodes"]["2"]],
        "wK" -> linWeightT[att["Nodes"]["1"]], "bK" -> linBiasT[att["Nodes"]["1"]],
        "wV" -> linWeightT[att["Nodes"]["4"]], "bV" -> linBiasT[att["Nodes"]["4"]],
        "wO" -> linWeightT[att["Nodes"]["6"]], "bO" -> linBiasT[att["Nodes"]["6"]],
        "norm2Scale" -> vecT[norm2["Arrays"]["Scaling"]],
        "norm2Bias"  -> vecT[norm2["Arrays"]["Biases"]],
        "w1" -> linWeightT[g2["Nodes"]["linear1"]], "b1" -> linBiasT[g2["Nodes"]["linear1"]],
        "w2" -> linWeightT[g2["Nodes"]["linear2"]], "b2" -> linBiasT[g2["Nodes"]["linear2"]]
    |>
]

GPT2ExtractArrays[] := Module[{raw, dec, blockKeys, finalNorm},
    raw = rawNet[];
    If[ raw === $Failed, Return[$Failed]];
    dec       = raw["Nodes"]["decoder"];
    (* nodes 1..12 are transformer blocks; node 13 is the final norm. *)
    blockKeys = Select[Keys[dec["Nodes"]],
        StringMatchQ[#, DigitCharacter ..] &&
            dec["Nodes"][#]["Type"] === "Chain" &];
    blockKeys = SortBy[blockKeys, ToExpression];
    finalNorm = SelectFirst[Values[dec["Nodes"]],
        #["Type"] === "Normalization" &];
    <|
        "TokenEmbedding"    -> TTensorCreate @ NumericArray[
            Normal @ raw["SharedArrays"]["Weights"], "Real32"],
        "PositionEmbedding" -> TTensorCreate @ NumericArray[
            Normal @ raw["Nodes"]["embedding"]["Nodes"]["embeddingpos"][
                "Arrays"]["Weights"], "Real32"],
        "Blocks"            -> (blockParams[dec["Nodes"][#]] & /@ blockKeys),
        "FinalNormScale"    -> vecT[finalNorm["Arrays"]["Scaling"]],
        "FinalNormBias"     -> vecT[finalNorm["Arrays"]["Biases"]]
    |>
]

(* The high-level net is broken, but its NetEncoder (input) and the
   output NetDecoder's Labels survive the WLNet import intact and can be
   pulled off the imported (still-$Failed-as-a-net) expression with
   NetExtract.  Cache the imported expression so we extract once. *)
$importedNet := $importedNet = Module[{file},
    file = GPT2WeightFile[];
    If[ file === $Failed, $Failed, (Needs["NeuralNetworks`"]; Quiet @ Import[file, "WLNet"])]]

GPT2Encoder[] := Module[{g},
    g = $importedNet;
    If[ g === $Failed, $Failed, Quiet @ NetExtract[g, "Input"]]
]

GPT2Labels[] := Module[{g, od},
    g = $importedNet;
    If[ g === $Failed, Return[$Failed]];
    od = Quiet @ NetExtract[g, "Output"];
    Quiet @ NetExtract[od, "Labels"]
]

End[];
