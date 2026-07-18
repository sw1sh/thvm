#!/usr/bin/env wolframscript
(* dsv2_local_llm.wls -- run DeepSeek-V2-Lite in one Wolfram kernel via THVMLink
   and expose it to LLMConfiguration / the Chat Notebook as an in-process LLM
   service.  No HTTP, no Python, no subprocess: the tokenizer, the model, and
   the decode loop all live in THIS kernel.  This replaces the whole
   server.py + register_thvm.wls HTTP stack.

   Run it (from the repo root):
       wl -f tools/thvm_llm_service/dsv2_local_llm.wls
   then, in the SAME kernel (or a notebook whose kernel ran this file):
       cfg = LLMConfiguration[<|"Model" -> <|"Service" -> "THVMLocal"|>|>];
       LLMSynthesize["Hello", LLMEvaluator -> cfg]
       ChatEvaluate[ChatObject[LLMEvaluator -> cfg], "..."]

   Where the model runs = where this kernel runs.  THVM_WEIGHTS controls it:
       synth (default) -- a small throwaway-weight config, runs on any CPU;
                          exercises the full path (tokenize -> prefill ->
                          greedy decode -> detokenize) but says nonsense.
       real            -- the true model, imported NATIVELY from the ollama
                          GGUF (dsvGgufInit, no conversion / no intermediate
                          file); the blob is auto-located from the ollama
                          manifest, or set THVM_GGUF.  Set "Device" -> "cuda" and
                          run this kernel on a GPU box for real-time chat.  The
                          tokenizer.json still comes from THVM_SNAPSHOT.

   The service is SESSION-SCOPED: the model is resident in this kernel, so it
   answers here; a fresh kernel must run this file again to serve. *)

PacletDirectoryLoad["wl/THVMLink"];
Get["WolframInstitute`THVMLink`Examples`"];
Needs["LLMServices`"];

TInit[]; TReset[];

(* --- locate the tokenizer.json (env override, else the HF cache) ---------- *)
$snapshot = Environment["THVM_SNAPSHOT"] /. $Failed :> DirectoryName @ FirstCase[
    FileNames["tokenizer.json",
        FileNameJoin[{$HomeDirectory, ".cache", "huggingface", "hub",
            "models--deepseek-ai--DeepSeek-V2-Lite-Chat", "snapshots"}], Infinity],
    p_String :> p,
    (Print["set THVM_SNAPSHOT to the DeepSeek-V2-Lite snapshot dir"]; Exit[1])];

$thvmTok = dsvTokenizer[FileNameJoin[{$snapshot, "tokenizer.json"}]];

(* --- the model: config -> decode state, resident in this kernel ----------- *)
(* the ollama GGUF blob for a model tag: read the ollama manifest for the model
   layer's digest, then the content-addressed blob path. *)
ollamaGguf[tag_String] := Module[{tp = StringSplit[tag, ":"], layers, dig},
    layers = Import[FileNameJoin[{$HomeDirectory, ".ollama", "models", "manifests",
        "registry.ollama.ai", "library", tp[[1]], tp[[2]]}], "RawJSON"]["layers"];
    dig = SelectFirst[layers, StringContainsQ[#["mediaType"], ".model"] &]["digest"];
    FileNameJoin[{$HomeDirectory, ".ollama", "models", "blobs", StringReplace[dig, ":" -> "-"]}]
]

$real = Environment["THVM_WEIGHTS"] === "real";

$thvmState = If[ $real
    ,
    (* the real ollama DeepSeek-V2-Lite via NATIVE GGUF import: no conversion, no
       intermediate file, and the routed experts stay Q8_0 in place (~16GB
       resident instead of ~32GB bf16).  THVM_GGUF overrides the auto-located
       ollama blob; DEV -> metal/cuda runs it on the GPU. *)
    dsvGgufInit[dsvConfig["Device" -> (Environment["DEV"] /. $Failed -> "cpu"), "ExpertsQ8" -> True],
        Environment["THVM_GGUF"] /. $Failed :> ollamaGguf["deepseek-v2:16b-lite-chat-q8_0"]]
    ,
    (* a small synthetic config that runs anywhere and proves the full path *)
    dsvSynthInit[dsvConfig[
        "Dim" -> 64, "NLayers" -> 2, "NHeads" -> 4, "KvLora" -> 32,
        "DNope" -> 16, "DRope" -> 8, "DV" -> 16, "NExperts" -> 8, "NShared" -> 2,
        "TopK" -> 3, "MoeInter" -> 48, "DenseInter" -> 96,
        "Vocab" -> If[$real, 102400, 256], "MaxSeq" -> 256]]
];

(* --- the request function: messages -> chat prompt -> ids -> decode -> text  *)
$vocab = $thvmState["Config"]["Vocab"];

thvmContent[c_] := Which[
    StringQ[c], c,
    ListQ[c], StringJoin @ Cases[c, a_Association :> Lookup[a, "Content", Lookup[a, "text", ""]]],
    True, ToString[c]];

thvmLocalRequest[so_, requestName_, params_] := Module[{msgs, prompt, ids, gen, content, maxTok},
    msgs = (<|"Role" -> Lookup[#, "Role", "user"], "Content" -> thvmContent @ Lookup[#, "Content", ""]|> &) /@
        Lookup[params, "Messages", {}];
    prompt = dsvChatPrompt[msgs];
    ids = dsvEncode[$thvmTok, prompt];
    (* synthetic vocab is tiny; clip ids so the smoke test never indexes past it *)
    ids = If[$real, ids, Mod[ids, $vocab]];
    maxTok = Replace[Lookup[params, "MaxTokens", 128], Except[_Integer] -> 128];
    gen = dsvGenerate[$thvmState, ids, "MaxTokens" -> maxTok, "EosId" -> If[$real, 100001, -1]];
    content = dsvDecode[$thvmTok, gen];
    <|"Role" -> "Assistant", "Content" -> content, "FinishReason" -> "stop",
        "Model" -> "thvm-dsv2-local", "Timestamp" -> Now,
        "Usage" -> <|"History" -> Quantity[Length[ids], "Tokens"], "Completion" -> Quantity[Length[gen], "Tokens"]|>,
        "CallID" -> CreateUUID[], "ToolRequests" -> {}, "RawResponseWXF" -> Missing[]|>];

$thvmParams = <|
    "ConnectionFunction" -> (ServiceObject["THVMLocal", <|"ID" -> "in-process"|>] &),
    "RequestFunction" -> thvmLocalRequest,
    "RequestSubmitFunction" -> thvmLocalRequest,
    "ServiceConnectionRequest" -> "Chat",
    "PostprocessingFunction" -> Identity,
    "SupportedParameters" -> DeleteCases["TopProbabilities" | "Reasoning"],
    "ModelList" -> {"thvm-dsv2-local"},
    "Icon" -> None, "Hidden" -> False|>;

RegisterService[LLMServices`Chat, "THVMLocal", $thvmParams, OverwriteTarget -> True];
RegisterService[LLMServices`ChatSubmit, "THVMLocal", $thvmParams, OverwriteTarget -> True];

(* --- demo ----------------------------------------------------------------- *)
$cfg = LLMConfiguration[<|"Model" -> <|"Service" -> "THVMLocal", "Name" -> "thvm-dsv2-local"|>|>];
Print["THVMLocal registered (", If[$real, "real checkpoint", "synthetic smoke config"], ")."];
Print["LLMSynthesize -> ", LLMSynthesize["Say hello.", LLMEvaluator -> $cfg]];
