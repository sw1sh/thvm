#!/usr/bin/env wolframscript
(* gpt2_inference.wls - real GPT-2 117M text generation on the thvm tensor
   surface, FAST: the whole Wolfram GPT-2 net is lifted ONCE through
   TFromNet[net, maxSeq] into a fixed-sequence forward (a TLam over a
   {maxSeq, vocab} one-hot input), materialized + captured ONCE with TJit,
   and then each generation step is a sub-second TJit replay over a refreshed
   one-hot -- no per-step re-lift.

   Why this is the fast path: lifting the 12-block net is a CONSTANT ~20s
   regardless of sequence length (it is graph construction, not compute), and
   it dominated the old grow-the-sequence loop (~tens of seconds per token).
   Here the variable-length id gather is replaced by a one-hot tensor input
   (onehot . tokenTable), so the graph has a FIXED shape, is built once, and
   TJit replays the captured kernel dispatch each step.  Positions 0..maxSeq-1
   are constant (no per-step positional gather) and causal masking + reading
   the logits at the current length keep the padded tail positions inert.

   Pure graph, no stored net: TFromNet traverses the real NetModel
   NetChain / NetGraph via its fromLayer cases and appends the tied
   token-embedding LM head.  The text encoder (NetExtract[net, "Input"]) and
   the output token Labels are host-side encode / decode boundary callbacks;
   nothing stores them.

   Sampling: each step temperature-scales the current-position logits, keeps
   the top-K tokens, and draws from that distribution -- proper text
   generation, not just greedy argmax (which repeats / stalls).  Greedy is
   still used for a deterministic correctness check.

   Run from the repo root:
       wolframscript -f wl/THVMLink/Examples/gpt2_inference.wls
   Optional env:
       PROMPT="Once upon a time"  NTOKENS=30  TEMPERATURE=0.8  TOPK=40  SEED=42 *)

PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];
Needs["NeuralNetworks`"];

print[args___] := WriteString["stdout", args, "\n"];

prompt      = Environment["PROMPT"]      /. $Failed -> "Once upon a time";
nTokens     = ToExpression[Environment["NTOKENS"]     /. $Failed -> "30"];
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
vocab = Length[labels];

ids    = encoder[prompt];
maxSeq = Length[ids] + nTokens;             (* fixed window: prompt + generated *)
print["Prompt token ids: ", ToString[ids, InputForm]];

(* one-hot {maxSeq, vocab}: row i has a 1 at the 1-indexed token id; unfilled
   tail rows are zero (their outputs are causally masked away). *)
oneHot[seq_] := TTensorCreate @ NumericArray[
    Normal @ SparseArray[
        Table[{i, seq[[i]]} -> 1., {i, Length[seq]}], {maxSeq, vocab}],
    "Real32"];

(* Lift ONCE: a fixed-sequence forward over a one-hot input slot, captured by
   TJit on the first call and replayed each subsequent step. *)
print["Lifting + materializing the fixed-", ToString[maxSeq], " forward (once) ..."];
slot = oneHot[ids];
t0   = AbsoluteTime[];
lam  = Block[{$RecursionLimit = 1*^6}, TFromNet[net, maxSeq]];
step = TJit[Function[{}, TRealize @ TWnf @ TApp[lam, slot]]];
out  = Block[{$RecursionLimit = 1*^6}, step[]];   (* capture: lift+materialize+dispatch *)
print["  lifted + captured ", ToString[TJitOpCount[step]], " kernel dispatches in ",
    ToString[Round[AbsoluteTime[] - t0, 0.1]], "s (one-time)"];

logitsAt[pos_]   := Normal @ TTensorData[out] // (#[[pos]] &);
greedy[logits_]  := Ordering[logits, -1][[1]];     (* 1-indexed argmax *)
sampleTok[logits_, temp_, k_] := Module[{idx, w},
    idx = Ordering[logits, -k];
    w   = Exp[(logits[[idx]] - Max[logits[[idx]]]) / temp];
    RandomChoice[w -> idx]];

print["Generating ", ToString[nTokens], " tokens (TJit replay per step) ..."];
print[""];

t0 = AbsoluteTime[];
gen = ids;
Do[
    With[{tok = sampleTok[logitsAt[Length[gen]], temperature, topK]},
        gen = Append[gen, tok];
        WriteString["stdout", decode[{tok}]]];
    TSet[slot, oneHot[gen]];
    step[],                                          (* fast replay *)
    {nTokens}];
print[""];

print[""];
print["=== Generated text ==="];
print[decode[gen]];
print[""];
print["(", ToString[nTokens], " tokens in ",
    ToString[Round[AbsoluteTime[] - t0, 0.1]], "s after the one-time lift)"];

(* Deterministic correctness check: the greedy next token after "The quick
   brown fox" is 19 ("es"), confirmed against an independent numpy GPT-2
   forward over the same weights (top-5: {19, 118, 63, 83, 50246}). *)
chkIds  = encoder["The quick brown fox"];
chkN    = Length[chkIds];
chkSlot = oneHot[chkIds];
chkLam  = Block[{$RecursionLimit = 1*^6}, TFromNet[net, maxSeq]];
chkStep = TJit[Function[{}, TRealize @ TWnf @ TApp[chkLam, chkSlot]]];
chkOut  = Block[{$RecursionLimit = 1*^6}, chkStep[]];
print[""];
print["Verification (greedy next-token after 'The quick brown fox' == 19): ",
    ToString[greedy[Normal @ TTensorData[chkOut][[chkN]]] === 19]];
