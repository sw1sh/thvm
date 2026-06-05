#!/usr/bin/env wolframscript
(* gpt2_inference.wls - real GPT-2 117M inference on the thvm tensor
   surface: encode a text prompt, lift the transformer through TFromNet's
   T* building blocks, run the forward, and greedily generate a few
   tokens autoregressively.

   GPT-2's high-level NetModel deserialiser is broken in NeuralNetworks
   15.0.3 (a post-hoc EmbeddingLayer $Scales array makes the upgrade
   raise InvalidPort), so we read the cached WLNet directly and pull the
   weights out of the node tree -- see Examples/gpt2_weights.wl.  The BPE
   NetEncoder and the output decoder's token Labels survive intact and do
   the host-side encode / decode.

   Run from the repo root:
       wolframscript -f wl/THVMLink/Examples/gpt2_inference.wls
   Optional:  PROMPT="Once upon a time" NTOKENS=10 wolframscript ...   *)

PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];
Get[FileNameJoin[{"wl", "THVMLink", "Examples", "gpt2_weights.wl"}]];

print[args___] := WriteString["stdout", args, "\n"];

prompt  = Environment["PROMPT"]  /. $Failed -> "The quick brown fox";
nTokens = ToExpression[Environment["NTOKENS"] /. $Failed -> "8"];

print["Prompt:   ", prompt];
print["Loading GPT-2 117M weights (~117M params) ..."];

arrays  = GPT2ExtractArrays[];
encoder = GPT2Encoder[];
labels  = GPT2Labels[];
If[ arrays === $Failed || encoder === $Failed,
    print["GPT-2 net not cached.  Run NetModel[\"GPT2 Transformer "
        <> "Trained on WebText Data\"] once (online) to download it."];
    Quit[1]];

(* Encode -> 1-indexed token ids; decode a label list back to a string. *)
decode[ids_] := StringJoin[labels[[ids]]];

(* One greedy step: build + run the forward over the running id sequence,
   take the argmax of the last position's logits (the next-token
   distribution).  The deep Fold over 12 transformer blocks needs a
   raised recursion limit during graph construction. *)
nextToken[ids_] := Block[{$RecursionLimit = 1*^6},
    Module[{logits, lastRow},
        logits  = TGPT2FromArrays[arrays, ids];
        lastRow = Last @ Normal @ TTensorData @ TRealize[logits];
        Ordering[lastRow, -1][[1]]            (* 1-indexed argmax *)
    ]
]

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
   same extracted weights (top-5: {19, 118, 63, 83, 50246}). *)
If[ prompt === "The quick brown fox",
    print[""];
    print["Verification (first generated token == GPT-2 reference argmax 19): ",
        ToString[ids =!= gen && gen[[Length[ids] + 1]] === 19]]];
