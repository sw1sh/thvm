---
Template: TechNote
Name: Gpt2
Title: GPT-2 Inference with THVMLink
Context: THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/tutorial/Gpt2
Keywords: [GPT-2, GPT2, transformer, attention, self-attention, causal mask, multi-head, layer normalization, LayerNorm, GELU, embedding, language model, text generation, autoregressive, NetModel, TFromNet, inference]
RelatedGuides: [THVMLink]
RelatedTutorials: [Train, Tensors, Overview]
---

## A transformer on the tensor surface

The [Train](paclet:WolframInstitute/THVMLink/tutorial/Train) tutorial lifted a convolutional classifier through [TFromNet]() and trained it. This note lifts a *language model* - the 117M-parameter GPT-2 - and runs real inference: encode a prompt, run the transformer forward, and greedily generate a continuation.

GPT-2 is a stack of twelve identical pre-norm transformer blocks. Each block is two residual branches: causal multi-head self-attention around a [LayerNorm](paclet:WolframInstitute/THVMLink/ref/TLayerNorm), then a [GELU](paclet:WolframInstitute/THVMLink/ref/TGELU) feed-forward around a second LayerNorm. A token [EmbeddingLayer]() plus a learned positional embedding feed the stack; a final LayerNorm and a tied linear head turn the last hidden state into next-token logits over the 50257-token vocabulary. Every one of those pieces is a `TTerm` graph built from the same movement and reduce primitives the rest of THVMLink uses - `Dot`, [TSoftmax](), [TLayerNorm]() - so the whole model is one lazy graph you [TRealize]() at the end.

Each building block below is checked against the Wolfram layer it mirrors, to f32 tolerance. That parity is the correctness gate; the full inference script at the end strings the verified blocks into the published GPT-2 weights.

## Token and positional embeddings

A token id is just a row lookup into the embedding matrix. <code>[TEmbeddingMatrix]()[*table*, *ids*]</code> gathers the rows of a `{vocab, dim}` table for a host-side list of 0-indexed ids and stitches them into a `{Length[ids], dim}` matrix. Wolfram's [EmbeddingLayer]() is 1-indexed, so the lift subtracts one. Take a tiny `{6, 4}` table and gather three rows:

```wl
table = TTensorCreate[{{0., 1., 2., 3.}, {4., 5., 6., 7.}, {8., 9., 10., 11.},
                       {12., 13., 14., 15.}, {16., 17., 18., 19.}, {20., 21., 22., 23.}}];
Normal @ TRealize @ TEmbeddingMatrix[table, {0, 4, 2}]
```
<!-- => {{0., 1., 2., 3.}, {16., 17., 18., 19.}, {8., 9., 10., 11.}} -->

GPT-2 sums the token embedding and the positional embedding (the embedding sub-graph's `inputCombine` is [Plus]()). With the same table standing in for positions, the combined embedding of ids `{0, 4, 2}` at positions `{0, 1, 2}` is the elementwise sum:

```wl
table = TTensorCreate[{{0., 1., 2., 3.}, {4., 5., 6., 7.}, {8., 9., 10., 11.},
                       {12., 13., 14., 15.}, {16., 17., 18., 19.}, {20., 21., 22., 23.}}];
tok = TEmbeddingMatrix[table, {0, 4, 2}];
pos = TEmbeddingMatrix[table, {0, 1, 2}];
Normal @ TRealize[tok + pos]
```
<!-- => {{0., 2., 4., 6.}, {20., 22., 24., 26.}, {16., 18., 20., 22.}} -->

## Layer normalization

GPT-2's [NormalizationLayer]() standardizes each position across the feature axis - subtract the row mean, divide by the root of the (biased) row variance plus `1.*^-5` - then applies a learned per-feature scale and shift. <code>[TLayerNormAffine]()[*x*, *gamma*, *beta*]</code> is exactly that, and it matches the Wolfram layer's own output to about `1.*^-7`. Normalize a `{2, 4}` input with unit scale and zero shift, so each row comes out mean-zero, unit-variance:

```wl
x = TTensorCreate[{{1., 2., 3., 4.}, {-2., 0., 2., 4.}}];
gamma = TTensorCreate[{1., 1., 1., 1.}];
beta = TTensorCreate[{0., 0., 0., 0.}];
Normal @ TRealize @ TLayerNormAffine[x, gamma, beta]
```
<!-- => {{-1.34164, -0.447214, 0.447214, 1.34164}, {-1.34164, -0.447214, 0.447214, 1.34164}} -->

The bare [TLayerNorm]() (no learned scale or shift) does the standardize step alone; the affine form multiplies by *gamma* and adds *beta* along the last axis.

## The GELU activation

The feed-forward block's nonlinearity is the tanh-approximation GELU, `0.5 x (1 + tanh(sqrt(2/pi) (x + 0.044715 x^3)))`. [TGELU]() composes it from [TTanh](), which thvm computes as `2 sigmoid(2x) - 1` so the exponent stays negative and the result saturates cleanly to `+-1` instead of overflowing to NaN - GELU's cubic argument grows fast enough to matter. Apply it across a span that reaches into that tail:

```wl
x = TTensorCreate[{-4., -1., 0., 1., 4., 12.}];
Normal @ TRealize @ TGELU[x]
```
<!-- => {-0.00007, -0.15881, 0., 0.84119, 3.99993, 12.} -->

This matches Wolfram's `ElementwiseLayer[0.5 # (1 + Tanh[0.797885 (# + 0.044715 #^3)]) &]` to about `1.*^-7` across the same range, with no overflow even at `x = 60`.

## Causal multi-head self-attention

Attention is the heart of the block. Project the input to queries, keys, and values; for each head, score every query against every key with a scaled dot product, mask out future positions, softmax each row, and mix the values. <code>[TMultiHeadAttention]()[*Q*, *K*, *V*, *nHeads*, *mask*]</code> does this over flat `{seq, dim}` projections (it reshapes to `{nHeads, seq, dHead}` internally), with [TCausalMask]() supplying the additive `{seq, seq}` bias that is `0` on and below the diagonal and `-1.*^9` above it - so position *i* attends only to positions `<= i`.

Build a two-head example over a length-three sequence and confirm the causal structure: the first row attends only to itself, so its output is exactly the first value vector of each head:

```wl
q = TTensorCreate[{{1., 0., 0., 1.}, {0., 1., 1., 0.}, {1., 1., 0., 0.}}];
k = TTensorCreate[{{1., 0., 0., 1.}, {0., 1., 1., 0.}, {1., 1., 0., 0.}}];
v = TTensorCreate[{{2., 3., 4., 5.}, {6., 7., 8., 9.}, {10., 11., 12., 13.}}];
Normal @ TRealize @ TMultiHeadAttention[q, k, v, 2, TCausalMask[3]]
```
<!-- => {{2., 3., 4., 5.}, {4.67905, 5.67905, 6.67905, 7.67905}, {7.02094, 8.02094, 8., 9.}} -->

GPT-2's attention sub-network uses a bare-dot scoring net (no key transform), `Mask -> "Causal"`, and pre-scales the queries by `1/Sqrt[64]` in a separate node - arithmetically identical to the `1/Sqrt[dHead]` scale [TMultiHeadAttention]() folds in. The lift matches that spec, and a manual per-head bare-dot computation, to about `1.*^-8`.

## The whole net is one graph

There is no hand-assembled forward. <code>[TFromNet]()[*net*, *ids*]</code> traverses the real GPT-2 [NetModel]() - a [NetChain]() of an embedding [NetGraph]() and a twelve-block transformer [NetChain]() - through the same per-layer cases the building blocks above came from. The embedding [NetGraph]() consumes the 1-indexed `ids` (token plus positional, summed), each pre-norm block is a [NetGraph]() of `norm -> attention -> residual add` then `norm -> linear -> GELU -> linear -> residual add`, and the final [NormalizationLayer]() closes the stack. The base model's LM head is the *tied* token-embedding projection (not a layer), so [TFromNet]() appends `hidden . tokenEmbedding^T` to produce the `{seq, vocab}` logits [TTerm](). The last row is the next-token distribution. When the net already carries that head as a real layer - the `"Task" -> "LanguageModeling"` variant's tied `classifier` [LinearLayer]() - [TFromNet]() folds it and skips the manual projection, so either net yields the same logits (bit-for-bit).

## Loading the model

The GPT-2 weights ship in the Wolfram Neural Net Repository; [NetModel]() loads them as a [NetChain](). The `"Task" -> "LanguageModeling"` variant bundles everything this example needs in one net: its layers are `{embedding, decoder, last, classifier, probabilities}`, its input encoder (the `"Input"` port) turns a prompt into 1-indexed token ids, and its output decoder (the `"Output"` port's `"Labels"`) turns ids back into text. The `classifier` is the tied token-embedding head as a real [LinearLayer](), so dropping the final probabilities layer with <code>[NetDrop]()[*lm*, -1]</code> leaves a net that outputs the `{seq, 50257}` next-token logits directly - no hand-appended head, and one net for the forward, the encoder, and the decoder. (Run [NetModel]() once, online, to populate the cache.)

```wl
#| eval: false
lm = NetModel[{"GPT2 Transformer Trained on WebText Data", "Task" -> "LanguageModeling"}];
NetExtract[lm, "Input"]["The quick brown fox"]
```
<!-- => {209, 1813, 7331, 21576} -->

## A reusable fixed-sequence forward

Lifting the twelve-block net is a constant ~20 s regardless of sequence length - it is graph *construction*, not compute - so re-lifting the growing sequence every step (`TFromNet[net, ids]` in a loop) costs tens of seconds per token. The fix is to lift ONCE into a fixed-shape forward and reuse it. <code>[TFromNet]()[*net*, *onehot*]</code> does this for a token-LM [NetChain](): applied to a `{maxSeq, vocab}` ONE-HOT input it replaces the variable-length id gather with `onehot . tokenEmbedding`, so the whole graph - embedding, blocks, classifier head - has a fixed shape determined by the one-hot itself (`maxSeq` is the window you pad to - there is no separate integer argument). Capturing that forward once under [TJit]() makes it reusable across steps. (On a net carrying a [SequenceLastLayer](), which would collapse to the last *padded* row, the fixed-window lift drops it: the window emits all positions and the per-step read picks the running length.) Positions `0..maxSeq-1` are constant (no per-step positional gather), and the causal mask plus reading the logits at the current length keep the padded tail positions inert. The one-hot of a prompt feeds it exactly the same rows the gather would, so its logits match the variable-length path to f32 (argmax token-for-token; see the parity note below).

## Generating text

Generation is then: lift the classifier-headed forward ONCE and capture it with <code>[TJit]()[*forward*]</code> as a function of the one-hot input. `step[onehot]` is the whole per-token kernel: the first call captures the cached twelve-block dispatch over that one-hot, and every later call REBINDS the fresh one-hot in place and re-dispatches - the [TJit]() closure's argument *is* the per-call input (tinygrad's `input_replace`: it substitutes the new input's buffer at the captured read sites), and `step` returns the `{maxSeq, vocab}` logits handle whose buffer the replay rewrote. No explicit copy or slot - passing `step[hot[g]]` is the entire feed. The one-hot itself is <code>[TOneHot]()[*ids* - 1, *vocab*]</code> over the (0-indexed) ids, padded to `maxSeq` with an out-of-range label so the tail rows stay zero. Greedy [PositionLargest]() argmax is deterministic - a correctness anchor: on `"The quick brown fox"` the first generated token is id 19 (`"es"`), agreeing with an independent numpy GPT-2 forward (top-5 `{19, 118, 63, 83, 50246}`), so thvm reproduces GPT-2's own next-token prediction token-for-token.

```wl
#| eval: false
lm     = NetModel[{"GPT2 Transformer Trained on WebText Data", "Task" -> "LanguageModeling"}];
ids    = NetExtract[lm, "Input"]["Once upon a time"];   maxSeq = Length[ids] + 12;
labels = NetExtract[NetExtract[lm, "Output"], "Labels"];
hot    = seq |-> TOneHot[PadRight[seq - 1, maxSeq, 50257], 50257];
step   = TJit[TRealize @ TWnf @ TFromNet[NetDrop[lm, -1], #] &];
gen    = Nest[g |-> Append[g, First @ PositionLargest @ Normal[step[hot[g]]][[Length[g]]]], ids, 12];
StringJoin @ labels[[gen]]
```
<!-- => "Once upon a time, the world was a place of great beauty and great danger" -->

real GPT-2 117M text out of the lifted graph - and FAST: the lift + capture is paid once (~0.9 s), then every step is a [TJit]() replay of the cached forward, ~25 ms per token (no re-lift, just the input rebind + the twelve-block dispatch + the logit readback) against the ~tens of *seconds* per token the re-lift-every-step loop spent rebuilding the graph - a ~1000x per-token speedup. [`Examples/gpt2_inference.wls`](../../Examples/gpt2_inference.wls) wraps this with temperature / top-K sampling. The fixed-window forward matches the variable-length path's argmax token-for-token on identical input (logits differ by ~1e-4 from the different matmul shapes, the same f32 reduction-order noise as elsewhere), so over a long enough generation a near-tie can flip and the two paths' token streams diverge - both are valid GPT-2 output.

Everything in this note is the ordinary tensor surface: the model is one `TTerm`, the attention and norms and GELU are the same `Dot`, [TSoftmax](), and reduce primitives you write by hand, and [TRealize]() turns the lazy graph into the logits that drive the next token.
