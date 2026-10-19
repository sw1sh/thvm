---
Template: TechNote
Name: Gpt2
Title: GPT-2 Inference with THVMLink
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/tutorial/Gpt2
Keywords: [GPT-2, GPT2, transformer, attention, self-attention, causal mask, multi-head, layer normalization, LayerNorm, GELU, embedding, language model, text generation, autoregressive, NetModel, TFromNet, inference]
RelatedGuides: [THVMLink]
RelatedTutorials: [Train, Tensors, Overview]
---

GPT-2 (Generative Pre-trained Transformer 2) is a language model: given a sequence of tokens, it gives a score for every possible next token. Its smallest version, with 117 million parameters, is a stack of twelve identical transformer blocks. Each block has two residual branches: causal multi-head self-attention after a layer normalization, then a feed-forward network with a GELU (Gaussian error linear unit) activation after a second layer normalization. A token embedding plus a learned position embedding feeds the stack, and a final layer normalization and a linear head, which shares its weights with the token embedding, turn the last hidden state into scores, called logits, over the 50257 tokens of the vocabulary.

THVMLink builds each of these pieces as a lazy tensor graph from the same operations as any other [TTerm]() tensor, so the whole model is one graph that [TRealize]() computes. The sections below build each piece on small inputs, then lift the published GPT-2 model from the Wolfram Neural Net Repository and generate text with it.

## Token and position embeddings

An embedding looks up one row of a matrix for each token. A table of six rows of four numbers stands in for the embedding matrix:

```wl
table = TTensorCreate[{{0., 1., 2., 3.}, {4., 5., 6., 7.}, {8., 9., 10., 11.}, {12., 13., 14., 15.}, {16., 17., 18., 19.}, {20., 21., 22., 23.}}]
```

`TEmbeddingMatrix` gathers the rows for a list of token ids, counted from 0, into a matrix with one row per id:

```wl
Normal @ TRealize @ TEmbeddingMatrix[table, {0, 4, 2}]
```
<!-- => {{0., 1., 2., 3.}, {16., 17., 18., 19.}, {8., 9., 10., 11.}} -->

GPT-2 adds the embedding of each token to the embedding of its position. The token embeddings of the ids 0, 4 and 2:

```wl
tok = TEmbeddingMatrix[table, {0, 4, 2}]
```

The embeddings of the positions 0, 1 and 2, here read from the same table:

```wl
pos = TEmbeddingMatrix[table, {0, 1, 2}]
```

Their sum is the input of the first block:

```wl
Normal @ TRealize[tok + pos]
```
<!-- => {{0., 2., 4., 6.}, {20., 22., 24., 26.}, {16., 18., 20., 22.}} -->

[EmbeddingLayer]() counts token ids from 1, so lifting a Wolfram Language net subtracts 1 from each id.

## Layer normalization

A layer normalization standardizes each position across its features: it subtracts the mean of the row and divides by the square root of the row's variance, taken with denominator *n*, plus $10^{-5}$. It then multiplies by a learned scale and adds a learned shift for each feature. `TLayerNormAffine` takes the input, the scale and the shift. A batch of two positions with four features:

```wl
hidden = TTensorCreate[{{1., 2., 3., 4.}, {-2., 0., 2., 4.}}]
```

A scale of 1 for each feature:

```wl
gamma = TTensorCreate[{1., 1., 1., 1.}]
```

A shift of 0 for each feature:

```wl
beta = TTensorCreate[{0., 0., 0., 0.}]
```

With unit scale and zero shift, each row comes out with mean 0 and variance close to 1:

```wl
normed = Normal @ TRealize @ TLayerNormAffine[hidden, gamma, beta]
```
<!-- => {{-1.34164, -0.447212, 0.447212, 1.34164}, {-1.34164, -0.447213, 0.447213, 1.34164}} -->

The largest difference from the same formula computed in machine precision is at the level of 32-bit rounding:

```wl
Max @ Abs[normed - ((# - Mean[#])/Sqrt[Mean[(# - Mean[#])^2] + 1.*^-5] & /@ {{1., 2., 3., 4.}, {-2., 0., 2., 4.}})]
```
<!-- => 4.5653*^-8 -->

`TLayerNorm` does the standardization alone, without the scale and the shift.

## The GELU activation

The feed-forward branch uses the tanh approximation of GELU, $\frac{1}{2} x \left(1 + \tanh\left(\sqrt{2/\pi} \left(x + 0.044715 x^3\right)\right)\right)$. `TGELU` builds it from `TTanh`, which computes $\tanh(x)$ as $2 \sigma(2 x) - 1$ with the logistic sigmoid $\sigma$, so a large argument saturates to 1 instead of overflowing. Values that reach into both tails:

```wl
span = TTensorCreate[{-4., -1., 0., 1., 4., 12.}]
```

The activation:

```wl
gelu = Normal @ TRealize @ TGELU[span]
```
<!-- => {-0.0000702459, -0.158808, 0., 0.841192, 3.99993, 12.} -->

[ElementwiseLayer]() computes the same formula in the Wolfram Language neural network framework:

```wl
ElementwiseLayer[0.5 # (1 + Tanh[Sqrt[2/Pi] (# + 0.044715 #^3)]) &][{-4., -1., 0., 1., 4., 12.}]
```
<!-- => {-0.0000702143, -0.158808, 0., 0.841192, 3.99993, 12.} -->

The two agree to 32-bit rounding:

```wl
Max @ Abs[gelu - ElementwiseLayer[0.5 # (1 + Tanh[Sqrt[2/Pi] (# + 0.044715 #^3)]) &][{-4., -1., 0., 1., 4., 12.}]]
```
<!-- => 2.38419*^-7 -->

At 60 the cubic term is large, and the activation still gives 60 rather than an overflow:

```wl
Normal @ TRealize @ TGELU[TTensorCreate[{60.}]]
```
<!-- => {60.} -->

## Causal multi-head self-attention

Attention projects the input to queries, keys and values. For each head, it scores every query against every key with a dot product scaled by $1/\sqrt{d}$, where *d* is the size of a head, masks out the positions that come later, applies a softmax to each row of scores, and mixes the values with the resulting weights. `TMultiHeadAttention` takes the queries, the keys and the values as matrices with one row per position, the number of heads and an additive mask. Queries for a sequence of three positions with four features, split into two heads of two features each:

```wl
q = TTensorCreate[{{1., 0., 0., 1.}, {0., 1., 1., 0.}, {1., 1., 0., 0.}}]
```

The keys, here the same as the queries:

```wl
kk = TTensorCreate[{{1., 0., 0., 1.}, {0., 1., 1., 0.}, {1., 1., 0., 0.}}]
```

The values:

```wl
vv = TTensorCreate[{{2., 3., 4., 5.}, {6., 7., 8., 9.}, {10., 11., 12., 13.}}]
```

`TCausalMask` gives the mask, 0 on and below the diagonal and $-10^9$ above it, so that the position *i* attends only to the positions up to *i*:

```wl
mask = TCausalMask[3]
```

The mask as a matrix:

```wl
Normal[mask]
```
<!-- => {{0., -1.*^9, -1.*^9}, {0., 0., -1.*^9}, {0., 0., 0.}} -->

The first position attends only to itself, so the first row of the result is the first row of the values in both heads:

```wl
Normal @ TRealize @ TMultiHeadAttention[q, kk, vv, 2, mask]
```
<!-- => {{2., 3., 4., 5.}, {4.67905, 5.67905, 6.67905, 7.67905}, {7.02094, 8.02094, 8., 9.}} -->

The attention part of the GPT-2 net scores with a plain dot product, uses a causal mask, and multiplies the queries by $1/\sqrt{64}$ in a separate node before scoring; with heads of size 64 this is the same scaling `TMultiHeadAttention` applies.

## Lifting the whole net

`TFromNet` turns a Wolfram Language net into one lazy graph by lifting each layer to the operations above. <code>TFromNet[*net*, *ids*]</code> lifts the GPT-2 net applied to a list of token ids, counted from 1 as the net's input encoder gives them. The GPT-2 model in the Wolfram Neural Net Repository is a [NetChain]() of an embedding [NetGraph](), which sums the token and position embeddings, and a [NetChain]() of the twelve blocks. Each block is a [NetGraph]() that applies a normalization, the attention and a residual addition, then a normalization, a linear layer, the GELU activation, a second linear layer and a second residual addition. The base model has no output layer, so `TFromNet` appends the product of the last hidden state with the transposed token embedding matrix, which gives the logits as a matrix with one row per position and one column per token of the vocabulary. The last row scores the next token.

The `"Task" -> "LanguageModeling"` variant of the model already ends in that product, as a [LinearLayer]() named `classifier` that shares its weights with the token embedding, followed by a softmax. `TFromNet` lifts the [LinearLayer]() like any other layer, so either variant gives the same logits.

## Loading the model

[NetModel]() loads the language-modeling variant of GPT-2 from the Wolfram Neural Net Repository. The download runs once and is then cached, so the cells below are shown without being evaluated:

```wl
#| eval: false
lm = NetModel[{"GPT2 Transformer Trained on WebText Data", "Task" -> "LanguageModeling"}]
```

The net's input encoder turns a text into token ids, counted from 1:

```wl
#| eval: false
NetExtract[lm, "Input"]["The quick brown fox"]
```
<!-- => {209, 1813, 7331, 21576} -->

The layers of this net are named `embedding`, `decoder`, `last`, `classifier` and `probabilities`, and its output decoder holds the text of each token. <code>[NetDrop]()[*lm*, -1]</code> removes the final softmax, which leaves a net whose output is the logits.

## A forward pass of fixed length

Lifting the twelve blocks takes about the same time whatever the length of the sequence, because the time goes into building the graph rather than computing it. Generation adds one token at a time, so lifting the net again for every new token repeats that construction at every step. Instead, the net is lifted once for a fixed window of positions and the lifted forward pass is reused.

<code>TFromNet[*net*, *onehot*]</code> lifts the net applied to a one-hot matrix: one row for each position of the window, with a 1 in the column of the token at that position. The lift multiplies the one-hot matrix by the token embedding matrix in place of the lookup of ids, so the graph has a fixed shape, set by the one-hot matrix. The position embeddings are those of the positions of the whole window. Rows past the end of the text are zero, and because of the causal mask they do not change the logits of the earlier positions, so the logits of the text are read from the row of its last token. If the net ends in a [SequenceLastLayer](), which would read the last padded row, the lift for a window drops it. On the same text, the one-hot forward pass and the lift from ids pick the same next token.

## Generating text

`TOneHot` builds the one-hot matrix from ids counted from 0. Padding the ids with 50257, a label outside the vocabulary, leaves the rows past the end of the text at zero. [TJit]() records the lifted forward pass as a function of the one-hot matrix: its first application builds, schedules and runs the graph, and each later application with a new one-hot matrix of the same shape replays the recorded kernels on it and gives the logits, without building the graph again. Each step reads the logits at the last position of the text and appends the token with the largest score, found with [PositionLargest](). Choosing the largest score at every step makes the generation deterministic.

The prompt as token ids:

```wl
#| eval: false
ids = NetExtract[lm, "Input"]["Once upon a time"]
```

A window long enough for the prompt and 12 new tokens:

```wl
#| eval: false
maxSeq = Length[ids] + 12
```

The text of each token, read from the output decoder:

```wl
#| eval: false
labels = NetExtract[NetExtract[lm, "Output"], "Labels"]
```

The one-hot matrix of a list of ids, padded to the window:

```wl
#| eval: false
hot = seq |-> TOneHot[PadRight[seq - 1, maxSeq, 50257], 50257]
```

The recorded forward pass of the net without its softmax:

```wl
#| eval: false
step = TJit[TRealize @ TWnf @ TFromNet[NetDrop[lm, -1], #] &]
```

Twelve steps of greedy generation, each appending the most likely next token:

```wl
#| eval: false
gen = Nest[g |-> Append[g, First @ PositionLargest @ Normal[step[hot[g]]][[Length[g]]]], ids, 12]
```

The prompt and its continuation as text:

```wl
#| eval: false
StringJoin @ labels[[gen]]
```
<!-- => "Once upon a time, the world was a place of great beauty and great danger" -->

The one-hot forward pass and the lift from ids pick the same token at each step on the same input. Their logits differ by about $10^{-4}$, because the matrix products have different shapes and sum in a different order, so over a long generation a near tie between two tokens can resolve differently and the two token sequences can part.
