(* ============================================================================
   register_thvm.wls -- register the "thvm" DeepSeek-V2-Lite pod as a first-class
   Wolfram LLM service backed by an OpenAI-compatible HTTP endpoint.

   Run ONCE per machine (persists via PersistentSymbol["LLMServices"]):
       wl -f register_thvm.wls
   Override the endpoint / key with env vars THVM_ENDPOINT / THVM_API_KEY.

   After this, from any kernel:
       cfg = LLMConfiguration[<|"Model" -> <|"Service" -> "THVM", "Name" -> "thvm-dsv2"|>|>];
       LLMSynthesize["...", LLMEvaluator -> cfg]
       LLMFunction["...", LLMEvaluator -> cfg]
       ChatEvaluate[ChatObject[LLMEvaluator -> cfg], "..."]
   and "THVM" appears in the Chat Notebook model menu.
   ============================================================================ *)

Needs["LLMServices`"];

$endpoint = Environment["THVM_ENDPOINT"] /. $Failed -> "http://127.0.0.1:8321/v1/chat/completions";
$apiKey   = Environment["THVM_API_KEY"]  /. $Failed -> "sk-thvm-dummy";

(* The registration persists via PersistentSymbol and is re-read by FRESH
   kernels, so the request function must be a SELF-CONTAINED closure: only
   built-ins + the endpoint/key embedded by With.  A named DownValues symbol
   (or a reference to $endpoint) would come back undefined after a restart
   and every call would fail with "response does not follow the expected
   format". *)
thvmRequest = With[{endpoint = $endpoint, apiKey = $apiKey},
  Function[{so, requestName, params},
    Module[{msgs, model, extra, body, resp, json, content},
      msgs = Replace[Lookup[params, "Messages", {}], {
        l_List :> Map[<|"role" -> ToLowerCase[Lookup[#, "Role", "user"]],
          "content" -> Replace[Lookup[#, "Content", ""], s_String :> s, {0}]|> &, l],
        o_ :> {<|"role" -> "user", "content" -> ToString[o]|>}}];
      model = Lookup[params, "Model", "thvm-dsv2"];
      (* forward the OpenAI-standard knobs the framework filled in, when present *)
      extra = DeleteMissing @ <|
        "temperature" -> Lookup[params, "Temperature", Missing[]],
        "max_tokens"  -> Lookup[params, "MaxTokens",   Missing[]],
        "stop"        -> Lookup[params, "StopTokens",   Missing[]]
      |>;
      body = ExportString[Join[<|"model" -> model, "messages" -> msgs, "stream" -> False|>, extra], "JSON"];
      resp = URLRead[HTTPRequest[endpoint, <|
          "Method"  -> "POST",
          "Headers" -> {"Content-Type" -> "application/json", "Authorization" -> "Bearer " <> apiKey},
          "Body"    -> body|>], {"StatusCode", "Body"}];
      json    = ImportString[resp["Body"], "RawJSON"];
      content = json["choices"][[1]]["message"]["content"];
      <|"Role" -> "Assistant", "Content" -> content,
        "FinishReason" -> Lookup[json["choices"][[1]], "finish_reason", "stop"],
        "Model" -> Lookup[json, "model", model], "Timestamp" -> Now,
        "Usage" -> <|"History" -> Quantity[0, "Tokens"], "Completion" -> Quantity[0, "Tokens"]|>,
        "CallID" -> Lookup[json, "id", CreateUUID[]], "ToolRequests" -> {}, "RawResponseWXF" -> Missing[]|>
    ]]];

thvmParams = <|
  "ConnectionFunction"       -> (ServiceObject["THVM", <|"ID" -> "local"|>] &),
  "RequestFunction"          -> thvmRequest,
  "RequestSubmitFunction"    -> thvmRequest,
  "ServiceConnectionRequest" -> "Chat",
  "PostprocessingFunction"   -> Identity,
  "SupportedParameters"      -> DeleteCases["TopProbabilities" | "Reasoning"],
  "ModelList"                -> {"thvm-dsv2"},
  "Icon"                     -> None,
  "Hidden"                   -> False
|>;

(* ServiceType -> "User" (default) persists AND survives the framework's lazy
   ResetServices; a "System" registration would be wiped on the first LLM call. *)
RegisterService[LLMServices`Chat,       "THVM", thvmParams, OverwriteTarget -> True];
RegisterService[LLMServices`ChatSubmit, "THVM", thvmParams, OverwriteTarget -> True];

Print["THVM registered. endpoint=", $endpoint];
Print["  Chat:       ", LLMServices`RegisteredServiceQ[LLMServices`Chat, "THVM"]];
Print["  ChatSubmit: ", LLMServices`RegisteredServiceQ[LLMServices`ChatSubmit, "THVM"],
      " (appears in Chat Notebook model menu)"];
