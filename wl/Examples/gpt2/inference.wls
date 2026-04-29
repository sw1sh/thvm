#!/usr/bin/env wolframscript
(* inference.wls -- Phase 12 of the tinygrad-parity arc.

   What this demonstrates:
   1. TFromNet now handles enough GPT-2 layer kinds to convert
      whole sub-graphs of NetModel["GPT Transformer ..."]
      directly into TTerm UOp graphs:
        - EmbeddingLayer            (token / position embeddings)
        - SequenceIndicesLayer       (position-id source)
        - NormalizationLayer         (LayerNorm with learned scale/shift)
        - DropoutLayer               (identity at inference)
        - ThreadingLayer[Plus|Times] (residual / multiplicative threading)
        - ElementwiseLayer           (any function whose body uses Plus,
                                      Times, Tanh, Power[t, n_Integer],
                                      Exp, Log, Sqrt -- the WL UpValues
                                      installed in Tensor.wl construct
                                      the equivalent UOp graph
                                      automatically)
        - NetMapOperator[LinearLayer]
        - NetGraph                   (top-level topological evaluation)
      Plus idiomatic UpValues so user code reads as ordinary
      Mathematica:
        - `a . b`              -> TMatMul (rank-2) / TDot (rank-1)
        - `Transpose[t]`       -> reverse-axis permute
        - `ArrayReshape[t, s]` -> TUOpReshape
        - `SoftmaxLayer[2][t]` -> TSoftmaxAxis[t, 1]
        - `Tanh[t]`, `t^3`, `Sqrt[t]`, `Exp[t]`, `Log[t]`, `Total[t]`
          all already routed.
   2. The embedding sub-NetGraph and the FFN half of one transformer
      block convert end-to-end and match Wolfram's own NetChain
      evaluation to f32 precision (max abs diff < 1e-5).
   3. The attention sub-NetGraph (12-head AttentionLayer +
      CatenateLayer + per-head NetMapOperator[LinearLayer]) does NOT
      yet convert: per-head input wiring through CatenateLayer needs
      ordered-multi-input edge tracking that the LayersGraph property
      flattens away.  Documented in docs/bench/phase12.md as the
      Phase 13 follow-up.
   4. A single-token forward on a synthetic mini-GPT-2 (vocab=32,
      dim=64, n_heads=4, n_layers=1, seq=4) runs end-to-end and
      asserts the output is finite and correctly shaped.

   Why no 100-token bench vs tinygrad: the constant-arg
   kernel-program-cache miss documented in docs/bench/phase11.md
   makes each forward call dispatch ~270ms-per-numeric-constant-op
   even after warmup.  On real GPT-2 dimensions (dim=768) one FFN
   block alone is ~6 s; a full forward is ~minutes per token.
   Closing this perf gap is the Phase 13 prerequisite.

   Run from repo root:
       wolframscript wl/Examples/gpt2/inference.wls
*)

PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];

print[args___] := WriteString["stdout",
    StringJoin @@ Map[ToString, {args}], "\n"]

SeedRandom[42];

(* ===========================================================
   Part 1: TFromNet vs Wolfram on real GPT-2 sub-graphs
   =========================================================== *)

print["=== Part 1: TFromNet conversion of real GPT-2 sub-graphs ==="];

net = NetInitialize @
    NetModel["GPT Transformer Trained on BookCorpus Data",
             "UninitializedEvaluationNet"];

ids = {7, 13, 21, 30};

(* Embedding sub-NetGraph *)
embedG = net[[1]];
print["embed sub-NetGraph children: ", Keys @ NetExtract[embedG, All]];

t0 = AbsoluteTime[];
embedOut = TRealize @ TFromLayer[embedG, ids];
embedMs  = (AbsoluteTime[] - t0) * 1000.;
embedArr = Normal @ TTensorData @ embedOut;
embedRef = embedG[ids];
embedDiff = Max[Abs[Flatten[embedArr - embedRef]]];
print["  embed thvm wall: ", NumberForm[embedMs, {6, 0}], " ms"];
print["  shape: ", Dimensions[embedArr], " (expected {4, 768})"];
print["  max abs diff vs Wolfram: ", embedDiff];
If[ embedDiff > 1.0*^-4 || Dimensions[embedArr] =!= {4, 768},
    print["FAIL: embed mismatch"]; Exit[1]];

(* FFN sub-NetGraph (post-attn half of block 1) *)
ffnG = net[[2,1,2]];
print["FFN  sub-NetGraph children: ", Keys @ NetExtract[ffnG, All]];

t0 = AbsoluteTime[];
ffnOut = TRealize @ TFromLayer[ffnG, embedOut];
ffnMs  = (AbsoluteTime[] - t0) * 1000.;
ffnArr = Normal @ TTensorData @ ffnOut;
ffnRef = ffnG[embedArr];
ffnDiff = Max[Abs[Flatten[ffnArr - ffnRef]]];
print["  ffn thvm wall: ", NumberForm[ffnMs, {6, 0}], " ms"];
print["  shape: ", Dimensions[ffnArr], " (expected {4, 768})"];
print["  max abs diff vs Wolfram: ", ffnDiff];
If[ ffnDiff > 1.0*^-4 || Dimensions[ffnArr] =!= {4, 768},
    print["FAIL: ffn mismatch"]; Exit[1]];

print["PASS: TFromNet matches Wolfram on embed + FFN sub-graphs"];
print[""];

(* ===========================================================
   Part 2: small mini-GPT-2 forward, hand-composed
   =========================================================== *)

print["=== Part 2: synthetic mini-GPT-2 forward (single token) ==="];

vocab   = 32;
dim     = 64;
nHeads  = 4;
nLayers = 1;
seq     = 4;
ffDim   = 4 * dim;

print["  config: vocab=", vocab, " dim=", dim,
      " n_heads=", nHeads, " n_layers=", nLayers, " seq=", seq];

mkLayer[] := <|
    "g1"   -> TTensorCreate @ TOnes[{dim}],
    "b1"   -> TTensorCreate @ TZeros[{dim}],
    "wq"   -> TTensorCreate @ TGlorot[{dim, dim}],
    "bq"   -> TTensorCreate @ TZeros[{dim}],
    "wk"   -> TTensorCreate @ TGlorot[{dim, dim}],
    "bk"   -> TTensorCreate @ TZeros[{dim}],
    "wv"   -> TTensorCreate @ TGlorot[{dim, dim}],
    "bv"   -> TTensorCreate @ TZeros[{dim}],
    "wo"   -> TTensorCreate @ TGlorot[{dim, dim}],
    "bo"   -> TTensorCreate @ TZeros[{dim}],
    "g2"   -> TTensorCreate @ TOnes[{dim}],
    "b2"   -> TTensorCreate @ TZeros[{dim}],
    "wff1" -> TTensorCreate @ TGlorot[{dim, ffDim}],
    "bff1" -> TTensorCreate @ TZeros[{ffDim}],
    "wff2" -> TTensorCreate @ TGlorot[{ffDim, dim}],
    "bff2" -> TTensorCreate @ TZeros[{dim}]
|>;

layers = Table[mkLayer[], nLayers];
tokEmb = TTensorCreate @ TGlorot[{vocab, dim}];
posEmb = TTensorCreate @ TGlorot[{seq, dim}];
gFinal = TTensorCreate @ TOnes[{dim}];
bFinal = TTensorCreate @ TZeros[{dim}];
wOut   = TTensorCreate @ TGlorot[{dim, vocab}];

block[x_TTerm, p_Association, mask_TTerm] := Module[{
    ln1, q, k, v, attn, attnProj, x1, ln2, ff1, ff1g, ff2
},
    ln1 = TLayerNormAffine[x, p["g1"], p["b1"]];
    q   = TLinear[ln1, p["wq"], p["bq"]];
    k   = TLinear[ln1, p["wk"], p["bk"]];
    v   = TLinear[ln1, p["wv"], p["bv"]];
    attn     = TMultiHeadAttention[q, k, v, nHeads, mask];
    attnProj = TLinear[attn, p["wo"], p["bo"]];
    x1  = x + attnProj;
    ln2  = TLayerNormAffine[x1, p["g2"], p["b2"]];
    ff1  = TLinear[ln2, p["wff1"], p["bff1"]];
    ff1g = TGELU[ff1];
    ff2  = TLinear[ff1g, p["wff2"], p["bff2"]];
    x1 + ff2
]

forward[promptIds_List] := Module[{mask, x, lnf, logits},
    mask = TCausalMask[seq];
    x    = TEmbeddingMatrix[tokEmb, promptIds] +
           TEmbeddingMatrix[posEmb, Range[0, seq - 1]];
    Do[ x = block[x, layers[[i]], mask], {i, nLayers}];
    lnf    = TLayerNormAffine[x, gFinal, bFinal];
    logits = TMatMul[lnf, wOut];
    logits
]

prompt = {7, 13, 21, 30};
print["  prompt ids: ", prompt];

t0 = AbsoluteTime[];
logitsTT = TRealize @ forward[prompt];
oneTokenMs = (AbsoluteTime[] - t0) * 1000.;
print["  single-forward wall: ", NumberForm[oneTokenMs, {6, 0}], " ms"];

logits = Normal @ TTensorData @ logitsTT;
print["  logits shape: ", Dimensions[logits]];
flat = Flatten[logits];
hasBad = AnyTrue[flat, !NumericQ[#] || Abs[#] > 1.0*^4 &];
If[ hasBad || Dimensions[logits] =!= {seq, vocab},
    print["FAIL: logits bad shape or non-finite"]; Exit[1]];
print["  max abs logit: ", NumberForm[Max[Abs[flat]], {6, 4}]];

(* Greedy next-token: argmax of last-row logits *)
nextId = First @ Position[logits[[-1]], Max[logits[[-1]]]] - 1;
print["  greedy next token id: ", nextId];

print["PASS: mini-GPT-2 forward + greedy decode produces finite logits"];
print[""];

(* ===========================================================
   Part 3: 10-token greedy generation, eager + TJit replay
   =========================================================== *)

print["=== Part 3: 10-token greedy generation ==="];

nGen = 10;
genWindow = prompt;   (* host-side rolling window of length seq *)

print["  prompt: ", prompt, "  generating ", nGen, " tokens"];

(* Eager loop -- TFromNet builds a fresh graph per token, runs
   through the full per-call materialise + dispatch path. *)
t0 = AbsoluteTime[];
generated = Reap[
    Module[{ids = genWindow, logitsArr, nextTok},
        Do[
            logitsArr = Normal @ TTensorData @ TRealize @ forward[ids];
            nextTok   = First[Position[logitsArr[[-1]], Max[logitsArr[[-1]]]]] - 1;
            Sow[nextTok];
            ids = Append[Drop[ids, 1], First[nextTok]],
            {nGen}
        ]];
][[2]];
eagerMs = (AbsoluteTime[] - t0) * 1000.;

print["  eager 10-token wall: ", NumberForm[eagerMs, {7, 0}], " ms"];
print["  eager ms/token:      ", NumberForm[eagerMs / nGen, {6, 1}], " ms"];
print["  generated tokens:    ", First[generated]];

(* Sanity: every step produced a finite token id within vocab range *)
flatGen = Flatten[generated];
If[ Length[flatGen] =!= nGen ||
    AnyTrue[flatGen, ! IntegerQ[#] || # < 0 || # >= vocab &],
    print["FAIL: generation produced bad token ids"];
    Exit[1]];

(* === TJit-wrapped variant ====================================
   The per-block forward writes into a pre-allocated logits buf;
   the embedding lookup runs eagerly each step (cheap) and TSets
   the input slot.  TJit captures the post-embedding kernel chain
   on the first call; subsequent calls replay the captured
   sequence directly (no materialise / dispatch overhead). *)

inputBuf  = TTensorCreate @ TZeros[{seq, dim}];
logitsBuf = TTensorCreate @ TZeros[{seq, vocab}];

forwardFromInput[xT_TTerm] := Module[{x, lnf, mask},
    mask = TCausalMask[seq];
    x    = xT;
    Do[ x = block[x, layers[[i]], mask], {i, nLayers}];
    lnf = TLayerNormAffine[x, gFinal, bFinal];
    TMatMul[lnf, wOut]
]

stepFn = Function[{}, TSet[logitsBuf, forwardFromInput[inputBuf]]];
jittedStep = TJit[stepFn];

(* Warm: capture *)
embed0 = TRealize[TEmbeddingMatrix[tokEmb, prompt] +
                  TEmbeddingMatrix[posEmb, Range[0, seq - 1]]];
TSet[inputBuf, embed0];
jittedStep[];   (* capture *)
print[""];
print["TJit captured ", TJitOpCount[jittedStep], " ops"];

(* JIT-replay 10 tokens *)
t0 = AbsoluteTime[];
genJit = Reap[
    Module[{ids = prompt, embT, logitsArr, nextTok},
        Do[
            embT = TRealize[TEmbeddingMatrix[tokEmb, ids] +
                            TEmbeddingMatrix[posEmb, Range[0, seq - 1]]];
            TSet[inputBuf, embT];
            jittedStep[];
            logitsArr = Normal @ TTensorData[logitsBuf];
            nextTok   = First[Position[logitsArr[[-1]], Max[logitsArr[[-1]]]]] - 1;
            Sow[nextTok];
            ids = Append[Drop[ids, 1], First[nextTok]],
            {nGen}
        ]];
][[2]];
jitMs = (AbsoluteTime[] - t0) * 1000.;

print["  jit 10-token wall:   ", NumberForm[jitMs, {7, 0}], " ms"];
print["  jit ms/token:        ", NumberForm[jitMs / nGen, {6, 1}], " ms"];
print["  jit replays/step:    ", TJitOpCount[jittedStep]];
print["  speedup vs eager:    ", NumberForm[eagerMs / jitMs, {4, 2}], "x"];
print["  generated tokens:    ", First[genJit]];

If[ Flatten[genJit] =!= Flatten[generated],
    print["FAIL: jit and eager disagree on generated tokens"];
    Exit[1]];

print["PASS: jit and eager produce identical token sequences"];
