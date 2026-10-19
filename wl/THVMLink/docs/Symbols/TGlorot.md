---
Template: Symbol
Name: TGlorot
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TGlorot
Keywords: [init, initialization, He, Kaiming, Glorot, weight, random, normal]
SeeAlso: [TTensorCreate, TMatMul, TGrad, RandomVariate, NormalDistribution]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TGlorot]()[*shape*]</code> gives a new f32 tensor of the given *shape* whose entries are drawn from a normal distribution with mean 0 and standard deviation $\sqrt{2/n_{\text{in}}}$, where $n_{\text{in}}$ is the fan-in of *shape*.

<code>[TGlorot]()[*shape*, *dtype*]</code> gives the tensor with the floating-point *dtype*.

<!-- #| annotation: 26.10.19: Despite the name, the scale is He (Kaiming) initialization, sqrt(2/fan_in), not Glorot/Xavier's sqrt(2/(fan_in + fan_out)). The fan-in rule follows thvm's two weight layouts: a linear weight is {in, out} because TLinear computes x . W, so fan_in is the first dimension; a convolution weight is {C_out, C_in, kh, kw}, so fan_in is the product of the dimensions after the first. The draw is WL-side RandomVariate, then one TTensorCreate. -->

## Details & Options

- The entries are independent samples of <code>[NormalDistribution]()[0, [Sqrt]()[2/*n*]]</code>, with *n* the fan-in, the scaling suited to a layer followed by a rectified linear unit.
- For a two-dimensional *shape* `{`*in*`, `*out*`}`, the fan-in is the first dimension *in*, the layout of a weight *W* applied as *x* . *W*.
- For a *shape* of rank 3 or more, such as a convolution weight `{`*out*`, `*in*`, `*h*`, `*w*`}`, the fan-in is the product of every dimension after the first. For rank 1 it is the one dimension.
- The samples come from <code>[RandomVariate]()</code>, so <code>[SeedRandom]()</code> makes them reproducible.
- The result is a realized tensor holding its values, so <code>[Normal]()</code> reads it with no <code>[TRealize]()</code>.
- The default *dtype* is `"f32"`; `"f16"` gives a half-precision tensor.
- <code>[TGrad]()</code> differentiates every floating-point tensor a loss reaches, so a [TGlorot]() weight trains with no further marking.

## Basic Examples

Fix the random seed, so the draw below is reproducible:

```wl
SeedRandom[1];
```

A 2 x 4 weight:

```wl
w = TGlorot[{2, 4}]
```

<!-- => a TTensor summary box: shape 2 x 4, dtype f32 -->

Its entries:

```wl
Normal[w]
```

<!-- => {{0.485679, 0.40914, 0.261881, 0.63566}, {1.5443, 0.676805, 0.1307, 1.09863}} -->

## Scope

A half-precision weight:

```wl
h = TGlorot[{2, 2}, "f16"]
```

<!-- => a TTensor summary box: shape 2 x 2, dtype f16 -->

Its dtype:

```wl
TTensorDType[h]
```

<!-- => "f16" -->

---

A vector, whose fan-in is its length:

```wl
TGlorot[{3}]
```

<!-- => a TTensor summary box: shape 3, dtype f32 -->

---

A convolution weight with 16 output channels, 3 input channels and a 3 x 3 window:

```wl
TGlorot[{16, 3, 3, 3}]
```

<!-- => a TTensor summary box: shape 16 x 3 x 3 x 3, dtype f32 -->

## Applications

A weight for a linear layer from 3 inputs to 2 outputs:

```wl
W = TGlorot[{3, 2}]
```

<!-- => a TTensor summary box: shape 3 x 2, dtype f32 -->

An input row:

```wl
x = TTensorCreate[{{1., 2., 3.}}]
```

<!-- => a TTensor summary box: shape 1 x 3, dtype f32 -->

The layer's output has one row and 2 columns:

```wl
Dimensions[TRealize[x . W]]
```

<!-- => {1, 2} -->

## Properties and Relations

Fix the random seed:

```wl
SeedRandom[2];
```

A 256 x 64 weight, whose fan-in is 256:

```wl
w = TGlorot[{256, 64}]
```

<!-- => a TTensor summary box: shape 256 x 64, dtype f32 -->

The standard deviation of its entries:

```wl
StandardDeviation[Flatten[Normal[w]]]
```

<!-- => 0.0882601 -->

It is close to $\sqrt{2/256}$:

```wl
N[Sqrt[2/256]]
```

<!-- => 0.0883883 -->

---

Fix the random seed:

```wl
SeedRandom[3];
```

A convolution weight, whose fan-in is $3 \cdot 3 \cdot 3 = 27$:

```wl
w = TGlorot[{16, 3, 3, 3}]
```

<!-- => a TTensor summary box: shape 16 x 3 x 3 x 3, dtype f32 -->

The standard deviation of its entries:

```wl
StandardDeviation[Flatten[Normal[w]]]
```

<!-- => 0.282489 -->

It is close to $\sqrt{2/27}$:

```wl
N[Sqrt[2/27]]
```

<!-- => 0.272166 -->

---

Each application draws new values:

```wl
Normal[TGlorot[{3}]] === Normal[TGlorot[{3}]]
```

<!-- => False -->

## Possible Issues

Fix the random seed:

```wl
SeedRandom[4];
```

For a two-dimensional shape the fan-in is the first dimension, so a 4 x 1000 weight has standard deviation near $\sqrt{2/4}$, not $\sqrt{2/1000}$:

```wl
StandardDeviation[Flatten[Normal[TGlorot[{4, 1000}]]]]
```

<!-- => 0.710323 -->
