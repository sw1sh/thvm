#!/usr/bin/env wolframscript
(* gpt2_inference.wls - real GPT-2 117M text generation on the thvm tensor
   surface, FAST: the whole Wolfram GPT-2 net is lifted ONCE through
   TFromNet[net, onehot] into a fixed-sequence forward over a {maxSeq, vocab}
   one-hot input (maxSeq is the one-hot's own shape, no integer window arg),
   captured ONCE with TJit, and then each generation step is a sub-second TJit
   replay that rebinds the fresh one-hot in place -- no per-step re-lift, no
   slot, no TSet.

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
       PROMPT="Once upon a time"  NTOKENS=30  TEMPERATURE=0.8  TOPK=40  SEED=42
       DEVICE=metal   -- run the whole forward on the GPU (token-identical) *)

PacletDirectoryLoad["wl/THVMLink"];
Get["WolframInstitute`THVMLink`"];
Needs["NeuralNetworks`"];

print[args___] := WriteString["stdout", args, "\n"];

prompt      = Environment["PROMPT"]      /. $Failed -> "Once upon a time";
nTokens     = ToExpression[Environment["NTOKENS"]     /. $Failed -> "30"];
temperature = ToExpression[Environment["TEMPERATURE"] /. $Failed -> "0.8"];
topK        = ToExpression[Environment["TOPK"]        /. $Failed -> "40"];
SeedRandom[ToExpression[Environment["SEED"] /. $Failed -> "42"]];

(* DEVICE="metal" runs the whole forward on the GPU: TToDevice pushes a device
   COPY to every weight + the one-hot input leaf, so each block COMPUTES on the
   GPU (token-identical to CPU).  TJit captures the Metal dispatch once and
   replays it per step.  At batch=1 the GPU wins once the sequence is long
   enough that the per-token matmuls outweigh the ~1.3k kernel launches (the
   crossover is ~128 tokens on an M3 Max); shorter windows stay CPU-bound. *)
device  = Environment["DEVICE"] /. $Failed -> "cpu";
forward = If[ device === "metal", TToDevice[TFromNet[net, #], "metal"] &,
                                  TFromNet[net, #] &];

print["Prompt:      ", prompt];
print["Device:      ", device];
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

(* Lift ONCE: a fixed-sequence forward over the one-hot input, captured by
   TJit on the first call and replayed each subsequent step.  maxSeq is the
   one-hot's own shape (no integer window arg to TFromNet); the TJit closure's
   argument IS the per-call one-hot, rebound in place on replay -- no slot,
   no TSet. *)
print["Lifting + materializing the fixed-", ToString[maxSeq], " forward (once) ..."];
t0   = AbsoluteTime[];
step = TJit[TRealize @ TWnf @ forward[#] &];
out  = step[oneHot[ids]];                         (* capture: lift+materialize+dispatch *)
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
    step[oneHot[gen]],                               (* fast replay, rebinds the one-hot *)
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
chkStep = TJit[TRealize @ TWnf @ forward[#] &];
chkOut  = chkStep[oneHot[chkIds]];
print[""];
print["Verification (greedy next-token after 'The quick brown fox' == 19): ",
    ToString[greedy[Normal @ TTensorData[chkOut][[chkN]]] === 19]];
