#!/usr/bin/env wolframscript
(* gpt2_inference.wls - real GPT-2 117M inference on the thvm tensor
   surface as a PURE GRAPH: encode a text prompt to token ids, lift the
   whole Wolfram GPT-2 net through TFromNet[net, ids] (embedding -> 12
   pre-norm transformer blocks -> final LayerNorm -> tied LM head), run
   the forward, and greedily generate a few tokens autoregressively.

   No hand-assembled forward, no raw-array extraction: TFromNet traverses
   the real NetModel NetChain / NetGraph via its fromLayer cases and
   appends the tied token-embedding LM head, returning the {seq, 50257}
   logits TTerm.  The text encoder (NetExtract[net, "Input"]) and the
   output token Labels (NetExtract[net, "Output"]) are used in this
   example for host-side encode / decode; nothing stores them.

   Run from the repo root:
       wolframscript -f wl/THVMLink/Examples/gpt2_inference.wls
   Optional:  PROMPT="Once upon a time" NTOKENS=10 wolframscript ...   *)

PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];
Needs["NeuralNetworks`"];

print[args___] := WriteString["stdout", args, "\n"];

prompt  = Environment["PROMPT"]  /. $Failed -> "The quick brown fox";
nTokens = ToExpression[Environment["NTOKENS"] /. $Failed -> "8"];

print["Prompt:   ", prompt];
print["Loading GPT-2 117M (NetModel) ..."];

net = Quiet @ Check[NetModel["GPT2 Transformer Trained on WebText Data"], $Failed];
If[ net === $Failed,
    print["GPT-2 net not cached.  Run NetModel[\"GPT2 Transformer "
        <> "Trained on WebText Data\"] once (online) to download it."];
    Quit[1]];

(* host-side encode (text -> 1-indexed token ids) via the net's own BPE
   NetEncoder, and decode (ids -> string) via the token Labels of the
   LanguageModeling-task variant's output NetDecoder (the base feature-
   extractor net outputs the hidden state, so its Output port has no token
   labels).  Both are used in this example only; nothing is stored. *)
encoder = NetExtract[net, "Input"];
labels  = NetExtract[
    NetExtract[
        NetModel[{"GPT2 Transformer Trained on WebText Data",
                  "Task" -> "LanguageModeling"}], "Output"],
    "Labels"];
decode[ids_] := StringJoin[labels[[ids]]];

(* One greedy step: lift the running id sequence to its {seq, 50257} logits
   TTerm, realize, take the argmax of the last position (the next-token
   distribution).  The deep traversal over 12 transformer blocks needs a
   raised recursion limit during graph construction. *)
nextToken[ids_] := Block[{$RecursionLimit = 1*^6},
    Module[{logits, lastRow},
        logits  = TFromNet[net, ids];
        lastRow = Last @ Normal @ TTensorData @ TRealize[logits];
        Ordering[lastRow, -1][[1]]            (* 1-indexed argmax *)
    ]
];

ids = encoder[prompt];
print["Prompt token ids: ", ToString[ids, InputForm]];
print["Generating ", ToString[nTokens], " tokens (greedy argmax) ..."];

t0  = AbsoluteTime[];
gen = ids;
Do[
    gen = Append[gen, nextToken[gen]];
    print["  token ", ToString[i], ": ",
        decode[{Last[gen]}], "   (", ToString[AbsoluteTime[] - t0], "s elapsed)"],
    {i, nTokens}];

print[""];
print["=== Generated continuation ==="];
print["Full text:    ", decode[gen]];
print["Continuation: ", decode[gen[[Length[ids] + 1 ;;]]]];

(* Verification: for the default prompt the greedy next token is 19
   ("es"), confirmed against an independent numpy GPT-2 forward over the
   same weights (top-5: {19, 118, 63, 83, 50246}). *)
If[ prompt === "The quick brown fox",
    print[""];
    print["Verification (first generated token == GPT-2 reference argmax 19): ",
        ToString[ids =!= gen && gen[[Length[ids] + 1]] === 19]]];
