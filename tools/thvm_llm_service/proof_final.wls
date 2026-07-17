(* ============================================================================
   AUTHORITATIVE PROOF: custom "THVM" LLM backend in Wolfram 15.0, routed to a
   local OpenAI-compatible mock server (stand-in for the thvm DSV2 pod).
   Proves: LLMSynthesize, LLMFunction, ChatEvaluate round-trips + Chat Notebook
   service/model discovery + the notebook ChatSettings option expression.
   ============================================================================ *)

Needs["LLMServices`"];

(* --- 0. System-type registration lives only in this kernel session ($LLMServices,
       in-memory) and evaporates on exit, so this script is hermetic as written. --- *)

$endpoint = "http://127.0.0.1:8321/v1/chat/completions";

(* WL message list -> OpenAI messages *)
toOpenAIMessages[msgs_List] := Map[
  <|"role" -> ToLowerCase[Lookup[#, "Role", "user"]],
    "content" -> Replace[Lookup[#, "Content", ""], s_String :> s, {0}]|> &, msgs];
toOpenAIMessages[o_] := {<|"role" -> "user", "content" -> ToString[o]|>};

(* the actual backend call: POST OpenAI /v1/chat/completions, parse the reply *)
thvmRequest[so_, requestName_, params_, opts___] := Module[{msgs, model, body, resp, json, content},
  msgs = toOpenAIMessages @ Lookup[params, "Messages", {}];
  model = Lookup[params, "Model", "thvm-dsv2"];
  body = ExportString[<|"model" -> model, "messages" -> msgs, "stream" -> False|>, "JSON"];
  resp = URLRead[HTTPRequest[$endpoint, <|
      "Method" -> "POST",
      "Headers" -> {"Content-Type" -> "application/json", "Authorization" -> "Bearer sk-thvm-dummy"},
      "Body" -> body|>], {"StatusCode", "Body"}];
  json = ImportString[resp["Body"], "RawJSON"];
  content = json["choices"][[1]]["message"]["content"];
  <|"Role" -> "Assistant", "Content" -> content,
    "FinishReason" -> Lookup[json["choices"][[1]], "finish_reason", "stop"],
    "Model" -> Lookup[json, "model", model], "Timestamp" -> Now,
    "Usage" -> <|"History" -> Quantity[0, "Tokens"], "Completion" -> Quantity[0, "Tokens"]|>,
    "CallID" -> Lookup[json, "id", CreateUUID[]], "ToolRequests" -> {}, "RawResponseWXF" -> Missing[]|>
];

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

(* --- 1. register THVM for the Chat (sync) and ChatSubmit (menu/streaming) endpoints.
       ServiceType -> "User" (the default) persists via PersistentSymbol and SURVIVES the
       framework's lazy ResetServices; "System" registrations get wiped on first LLM call. --- *)
Print["reg Chat:       ", Head @ RegisterService[LLMServices`Chat,       "THVM", thvmParams, OverwriteTarget -> True]];
Print["reg ChatSubmit: ", Head @ RegisterService[LLMServices`ChatSubmit, "THVM", thvmParams, OverwriteTarget -> True]];

cfg = LLMConfiguration[<|"Model" -> <|"Service" -> "THVM", "Name" -> "thvm-dsv2"|>|>];

Print["==== PROOF 1: LLMSynthesize ===="];
Print[LLMSynthesize["say hi", LLMEvaluator -> cfg]];

Print["==== PROOF 2: LLMFunction ===="];
f = LLMFunction["Greet `who` politely", LLMEvaluator -> cfg];
Print[f[<|"who" -> "world"|>]];

Print["==== PROOF 3: ChatEvaluate (conversation primitive behind Chat Notebooks) ===="];
chat = ChatEvaluate[ChatObject[LLMEvaluator -> cfg], "hello there"];
Print[chat["Messages"][[-1]]["Content"]];

Print["==== PROOF 4: Chat Notebook service+model discovery (source Chatbook reads) ===="];
info = LLMServices`LLMServiceInformation[LLMServices`ChatSubmit];
Print["THVM in ChatSubmit registry: ", KeyExistsQ[info, "THVM"]];
Print["THVM model menu list:        ", info["THVM", "ModelList"]];

Print["==== PROOF 5: Chat Notebook option expression (TaggingRules) ===="];
chatSettings = <|"Model" -> <|"Service" -> "THVM", "Name" -> "thvm-dsv2"|>|>;
nbOption = TaggingRules -> <|"ChatNotebookSettings" -> chatSettings|>;
Print[InputForm[nbOption]];

(* --- cleanup so this proof is repeatable (drops the persistent User entries) --- *)
LLMServices`UnregisterService[LLMServices`Chat, "THVM"];
LLMServices`UnregisterService[LLMServices`ChatSubmit, "THVM"];
Print["cleaned up: RSQ[Chat,THVM]=", LLMServices`RegisteredServiceQ[LLMServices`Chat, "THVM"]];
