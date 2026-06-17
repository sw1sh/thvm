---
Template: Symbol
Name: TUOpAdd
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TUOpAdd
Keywords: [UOp, add, elementwise, tensor]
SeeAlso: [TUOpMul, TUOpReduce, TUOpReshape, TRealize, TMaterialize, TGrad]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TUOpAdd]()[$a$, $b$]</code> builds a `UOP_ADD` node representing the elementwise sum of two UOp inputs.

## Details & Options

- Lazy: the node sits in the heap until <code>[TMaterialize]()</code> schedules it or <code>[TRealize]()</code> fires it. No buffer is allocated or written until then.
- Broadcasts when one side carries a broadcastable shape. For an explicit broadcast, wrap with <code>[TUOpExpand]()</code> first.
- Differentiable: <code>[TGrad]()</code> threads the cotangent through both inputs.
- `TTensor` leaves and <code>[TUOpConst]()</code> leaves are accepted directly; bare integers and reals are NOT lifted automatically into UOPs (use <code>[TUOpConst]()</code> or wrap in <code>[TTensorCreate]()</code>).

## Basic Examples

Add two small tensors:

```wl
Needs["WolframInstitute`THVMLink`"];
TInit[];
a = TTensorCreate[{1., 2., 3., 4.}];
b = TTensorCreate[{10., 20., 30., 40.}];
TTensorData @ TRealize @ TUOpAdd[a, b]
```
<!-- => NumericArray[{11., 22., 33., 44.}, "Real32"] -->

## Scope

Compose with other UOps; the whole graph is lazy until <code>[TRealize]()</code>:

```wl
x = TTensorCreate[{1., 2., 3., 4.}];
y = TTensorCreate[{5., 6., 7., 8.}];
TTensorData @ TRealize @ TUOpMul[TUOpAdd[x, y], TUOpAdd[x, y]]
```
<!-- => NumericArray[{36., 64., 100., 144.}, "Real32"] -->

## Applications

`TUOpAdd` underlies the addition operator surface on `TTerm`s; <code>[TLinear]()</code> uses it for `x @ W + b`:

```wl
W = TGlorot[{3, 8}];
x = TTensorCreate[ConstantArray[1., {1, 3}]];
b = TZeros[{8}];
TTensorShape @ TRealize @ TLinear[x, W, b]
```
<!-- => {1, 8} - TLinear lowers to TMatMul, which expects rank >= 2 inputs -->

## Properties and Relations

Addition is commutative, so the realized buffers compare equal regardless of source order:

```wl
a = TTensorCreate[{1., 2., 3.}];
b = TTensorCreate[{4., 5., 6.}];
TTensorData @ TRealize @ TUOpAdd[a, b] === TTensorData @ TRealize @ TUOpAdd[b, a]
```
<!-- => True -->

---

The backward rule is identity in both inputs; the cotangent passes through unchanged:

```wl
w = TTensorCreate[{1., 2., 3.}];
x = TTensorCreate[{10., 20., 30.}];
TRealize @ TGrad @ TUOpReduce[TUOpAdd[w, x], 0, "SUM"];
TTensorData @ TRealize @ TGradOf[w]
```
<!-- => NumericArray[{1., 1., 1.}, "Real32"] -->

## Possible Issues

A shape mismatch is rejected at schedule time, not at construction:

```wl
TMaterialize @ TUOpAdd[TTensorCreate[{1., 2., 3.}], TTensorCreate[{1., 2.}]]
```
<!-- => Failure["shape-mismatch", ...] -->
