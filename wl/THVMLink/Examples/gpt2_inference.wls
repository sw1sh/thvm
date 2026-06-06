#!/usr/bin/env wolframscript
(* gpt2_inference.wls - real GPT-2 117M text generation on the thvm tensor
   surface as a PURE GRAPH: encode a text prompt to token ids, lift the
   whole Wolfram GPT-2 net through TFromNet[net, ids] (embedding -> 12
   pre-norm transformer blocks -> final LayerNorm -> tied LM head), run the
   forward, and autoregressively generate a continuation.

   No hand-assembled forward, no raw-array extraction: TFromNet traverses
   the real NetModel NetChain / NetGraph via its fromLayer cases and appends
   the tied token-embedding LM head, returning the {seq, 50257} logits TTerm.
   The text encoder (NetExtract[net, "Input"]) and the output token Labels
   are used here for host-side encode / decode; nothing stores them.

   Sampling: each step temperature-scales the last-position logits, keeps the
   top-K tokens, and draws from that distribution -- proper text generation,
   not just greedy argmax (which repeats / stalls).  Greedy is still used for
   a deterministic correctness check.

   Run from the repo root:
       wolframscript -f wl/THVMLink/Examples/gpt2_inference.wls
   Optional env:
       PROMPT="Once upon a time"  NTOKENS=30  TEMPERATURE=0.8  TOPK=40  SEED=42

   Note: this re-lifts the full graph each step (the sequence grows, no KV
   cache), so generation is ~tens of seconds per token on CPU -- correct, not
   yet perf-tuned. *)

PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];
Needs["NeuralNetworks`"];

print[args___] := WriteString["stdout", args, "\n"];

prompt      = Environment["PROMPT"]      /. $Failed -> "Once upon a time";
nTokens     = ToExpression[Environment["NTOKENS"]     /. $Failed -> "12"];
temperature = ToExpression[Environment["TEMPERATURE"] /. $Failed -> "0.8"];
topK        = ToExpression[Environment["TOPK"]        /. $Failed -> "40"];
SeedRandom[ToExpression[Environment["SEED"] /. $Failed -> "42"]];

print["Prompt:      ", prompt];
print["Sampling:    temperature ", ToString[temperature], ", top-K ", ToString[topK]];
print["Loading GPT-2 117M (NetModel) ..."];

net = Quiet @ Check[NetModel["GPT2 Transformer Trained on WebText Data"], $Failed];
If[ net === $Failed,
    print["GPT-2 net not cached.  Run NetModel[\"GPT2 Transformer "
        <> "Trained on WebText Data\"] once (online) to download it."];
    Quit[1]];

(* host-side encode (text -> 1-indexed ids) via the net's BPE NetEncoder, and
   decode (ids -> string) via the token Labels of the LanguageModeling variant
   (the base feature net outputs the hidden state, no token labels). *)
encoder = NetExtract[net, "Input"];
labels  = NetExtract[
    NetExtract[
        NetModel[{"GPT2 Transformer Trained on WebText Data",
                  "Task" -> "LanguageModeling"}], "Output"],
    "Labels"];
decode[ids_] := StringJoin[labels[[ids]]];

(* last-position logits of the running id sequence: lift to {seq, 50257},
   realize, take the final row.  The deep 12-block traversal needs a raised
   recursion limit during graph construction. *)
nextLogits[ids_] := Block[{$RecursionLimit = 1*^6},
    Last @ Normal @ TTensorData @ TRealize @ TFromNet[net, ids]];

greedy[logits_]  := Ordering[logits, -1][[1]];     (* 1-indexed argmax *)

(* temperature + top-K sampling: restrict to the top-K logits, soften by
   temperature, draw one. *)
sampleTok[logits_, temp_, k_] := Module[{idx, w},
    idx = Ordering[logits, -k];
    w   = Exp[(logits[[idx]] - Max[logits[[idx]]]) / temp];
    RandomChoice[w -> idx]];

ids = encoder[prompt];
print["Prompt token ids: ", ToString[ids, InputForm]];
print["Generating ", ToString[nTokens], " tokens ..."];
print[""];

t0  = AbsoluteTime[];
gen = ids;
Do[
    gen = Append[gen, sampleTok[nextLogits[gen], temperature, topK]];
    WriteString["stdout", decode[{Last[gen]}]],
    {nTokens}];
print[""];

print[""];
print["=== Generated text ==="];
print[decode[gen]];
print[""];
print["(", ToString[nTokens], " tokens in ",
    ToString[Round[AbsoluteTime[] - t0, 0.1]], "s)"];

(* Deterministic correctness check: the greedy next token after "The quick
   brown fox" is 19 ("es"), confirmed against an independent numpy GPT-2
   forward over the same weights (top-5: {19, 118, 63, 83, 50246}). *)
print[""];
print["Verification (greedy next-token after 'The quick brown fox' == 19): ",
    ToString[greedy[nextLogits[encoder["The quick brown fox"]]] === 19]];
