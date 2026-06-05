#!/usr/bin/env wolframscript
(* forward.wls -- forward pass over a small synthetic GPT-2 config.
   Phase 11 of the tinygrad-parity arc -- exercises the GPT-2
   building blocks now living in NN.wl (TEmbedding,
   TEmbeddingMatrix, TGELU, TCausalMask, TLayerNormAffine,
   TMultiHeadAttention) plus TLinear's broadcast bias-add.

   Synthetic config (tiny on purpose; eager-mode constant-arg
   kernels do not hit the kernel-program-cache, so each block
   costs O(seconds) without TJit -- see docs/bench/history.md):

       vocab    = 32
       dim      = 16
       n_heads  = 2         -> d_head = 8
       n_layers = 1
       seq      = 4

   All weights are TTerms from initialisation onwards; nothing
   round-trips through `Normal`.  Sanity check at the end:
   logits are finite + correct shape.

   Run from repo root:
       wolframscript wl/Examples/gpt2/forward.wls
*)

PacletDirectoryLoad["wl/THVMLink"];
Get["THVMLink`"];

SeedRandom[42];

vocab   = 32;
dim     = 16;
nHeads  = 2;
nLayers = 1;
seq     = 4;
ffMult  = 4;
dHead   = dim / nHeads;
ffDim   = ffMult * dim;

Print["gpt2-mini config: vocab=", vocab, " dim=", dim,
      " n_heads=", nHeads, " n_layers=", nLayers,
      " seq=", seq, " ff_dim=", ffDim];

(* === Per-layer profile (eager, no JIT) ======================
   Establishes a baseline cost for each piece BEFORE running the
   e2e.  Numeric-constant arithmetic (TGELU's 0.5 / sqrt(2/Pi) /
   etc., TLayerNormAffine's residual chains) does not hit the
   kernel-program cache, so each call pays clang JIT cost every
   time -- the dominant phase-11 perf gap.  Phase 12 follow-up. *)

bench[name_, expr_] := Module[{t},
    t = First @ AbsoluteTiming[TRealize @@ {expr}];
    Print["  ", name, ": ", NumberForm[1000. * t, {6, 2}], " ms"];
    t]

xProf = TGlorot[{seq, dim}];
TRealize[xProf + xProf];   (* warm caches *)

Print["per-layer profile (eager):"];
gP = TOnes[{dim}];   bP = TZeros[{dim}];
qP = TGlorot[{seq, dim}];
kP = TGlorot[{seq, dim}];
vP = TGlorot[{seq, dim}];
maskP = TCausalMask[seq];
wP = TGlorot[{dim, dim}];
bDimP = TZeros[{dim}];

bench["TLayerNormAffine[{seq,dim}]", TLayerNormAffine[xProf, gP, bP]];
bench["TGELU[{seq,dim}]",            TGELU[xProf]];
bench["TMultiHeadAttention",         TMultiHeadAttention[qP, kP, vP, nHeads, maskP]];
bench["TLinear[{seq,dim},{dim,dim}]", TLinear[xProf, wP, bDimP]];
bench["TEmbeddingMatrix[seq]",
    TEmbeddingMatrix[
        TGlorot[{vocab, dim}],
        Range[0, seq - 1]]];

(* === Init weights as TTerms ONCE ============================
   Each weight is a TTensorCreate'd TTerm immediately; the
   model Association stores TTerms, not host NumericArrays.
   The transformer block reads them directly. *)

mkLayer[] := <|
    "g1"   -> TOnes[{dim}],
    "b1"   -> TZeros[{dim}],
    "wq"   -> TGlorot[{dim, dim}],
    "bq"   -> TZeros[{dim}],
    "wk"   -> TGlorot[{dim, dim}],
    "bk"   -> TZeros[{dim}],
    "wv"   -> TGlorot[{dim, dim}],
    "bv"   -> TZeros[{dim}],
    "wo"   -> TGlorot[{dim, dim}],
    "bo"   -> TZeros[{dim}],
    "g2"   -> TOnes[{dim}],
    "b2"   -> TZeros[{dim}],
    "wff1" -> TGlorot[{dim, ffDim}],
    "bff1" -> TZeros[{ffDim}],
    "wff2" -> TGlorot[{ffDim, dim}],
    "bff2" -> TZeros[{dim}]
|>;

layers   = Table[mkLayer[], nLayers];
tokEmb   = TGlorot[{vocab, dim}];
posEmb   = TGlorot[{seq, dim}];
gFinal   = TOnes[{dim}];
bFinal   = TZeros[{dim}];
wOut     = TGlorot[{dim, vocab}];

ids = {7, 13, 21, 30};

(* === Single transformer block ===============================
   Pre-LN flavor (matches GPT-2).  Operates entirely on TTerms;
   no host round-trip. *)

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

(* === Whole model ============================================ *)

forward[idsArg_List] := Module[{mask, x, lnf, logits},
    mask = TCausalMask[seq];
    x    = TEmbeddingMatrix[tokEmb, idsArg] +
           TEmbeddingMatrix[posEmb, Range[0, seq - 1]];
    Do[ x = block[x, layers[[i]], mask], {i, nLayers}];
    lnf    = TLayerNormAffine[x, gFinal, bFinal];
    logits = TMatMul[lnf, wOut];
    logits
]

Print[""];
Print["forward through ", nLayers, " layer(s) on prompt ids=", ids];
t0 = AbsoluteTime[];
logitsTT = TRealize @ forward[ids];
Print["  e2e wall: ",
    NumberForm[1000. * (AbsoluteTime[] - t0), {6, 0}], " ms"];

logits = Normal @ TTensorData @ logitsTT;
Print["  logits shape: ", Dimensions[logits]];
Print["  logits[1] (first row, first 4): ", Take[logits[[1]], 4]];

(* === Sanity assertions ====================================== *)

If[ Dimensions[logits] =!= {seq, vocab},
    Print["FAIL: expected shape {", seq, ", ", vocab,
          "}, got ", Dimensions[logits]];
    Exit[1]];

flat = Flatten[logits];
hasBad = AnyTrue[flat, !NumericQ[#] || Abs[#] > 1.0*^4 &];
If[ hasBad,
    Print["FAIL: logits contain NaN/Inf or magnitude > 1e4"];
    Print["  max abs: ", Max[Abs[flat]]];
    Exit[1]];

Print["  max abs logit: ", NumberForm[Max[Abs[flat]], {6, 4}]];
Print["PASS: thvm gpt2 forward produced finite logits of shape {",
      seq, ", ", vocab, "}"];
