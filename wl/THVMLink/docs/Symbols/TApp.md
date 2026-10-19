---
Template: Symbol
Name: TApp
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TApp
Keywords: [apply, application, lambda, beta reduction, interaction combinator, kernel]
SeeAlso: [TLam, TWnf, TCollapse, TDup, TRealize, TKernelCount]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TApp]()[*f*, *x*]</code> builds the application of the term *f* to the argument *x*.

<!-- #| annotation: 26.10.19: APP is a two-cell heap node [fun, arg] built through heapTerm/heapWith in THVMLink.wl, which is also where a bare Integer is lifted to an i32 NUM (numCoerce). The beta step is APP-LAM (src/interact/app_lam.c); it registers the bound variable's shape and materializes a UOp body into a UOP_KERNEL on the first tensor application, so the JIT rides on the beta step rather than on a separate compile step. APP-SUP and APP-ERA follow HVM4. The t[x] operator form is a SubValue on TTerm, defined next to TLam. -->

## Details & Options

- The application is a [TTerm]() with tag `APP`. It stays unreduced until <code>[TWnf]()</code>, <code>[TNf]()</code>, <code>[TCollapse]()</code> or <code>[TRealize]()</code> reduces it.
- A bare integer in either position is read as the corresponding <code>[TNum]()</code> term.
- A [TTerm]() *f* applied to an argument, *f*[*x*], is the same as <code>[TApp]()[*f*, *x*]</code>.
- Applying a <code>[TLam]()</code> substitutes *x* for the bound variable in the body (beta reduction).
- Applying a superposition <code>[TSup]()[*a*, *b*]</code> applies each branch, and applying the eraser <code>[TEra]()[]</code> gives <code>[TEra]()[]</code>.
- Applying a term that is not a function, such as a number or a constructor, stays an `APP` term.
- When the lambda body is a graph of UOp (micro-operation) nodes and *x* is a tensor, the first reduction compiles the body into a kernel whose input is the bound variable, and <code>[TRealize]()</code> runs it.
- Reduction consumes the cells of *f* and *x*, so a function applied more than once is shared through <code>[TDup]()</code>.

## Basic Examples

Apply the successor function to 41:

```wl
app = TApp[TLam[x, x + 1], TNum[41]]
```
<!-- => a TTerm summary box with tag APP -->

Reduce the application to weak head normal form:

```wl
TWnf[app]
```
<!-- => a TNum summary box showing the number 42 -->

## Scope

A bare integer argument is read as a number term:

```wl
TWnf[TApp[TLam[x, x + 1], 41]]
```
<!-- => a TNum summary box showing the number 42 -->

---

A term applied to an argument builds the same application:

```wl
TWnf[TLam[x, x + 1][TNum[41]]]
```
<!-- => a TNum summary box showing the number 42 -->

---

Nested applications apply a curried function to one argument at a time:

```wl
Term[TWnf[TApp[TApp[TLam[a, TLam[b, a - b]], 10], 3]]]
```
<!-- => Term["NUM", 7] -->

---

Applying a superposition of two functions applies each of them:

```wl
TTermVal /@ TCollapse[TApp[TSup[TLam[x, x + 1], TLam[x, 10 x]], 4]]
```
<!-- => {5, 40} -->

---

Applying a function to a superposition applies it to each branch:

```wl
TTermVal /@ TCollapse[TApp[TLam[x, x + 1], TSup[1, 2]]]
```
<!-- => {2, 3} -->

---

A lambda whose body is a UOp graph compiles to a kernel when applied to a tensor:

```wl
Normal[TRealize[TApp[TLam[w, TUOpMul[w, w]], TTensorCreate[{1., 2., 3., 4.}]]]]
```
<!-- => {1., 4., 9., 16.} -->

## Properties and Relations

Applying the eraser gives the eraser:

```wl
Term[TWnf[TApp[TEra[], TNum[4]]]]
```
<!-- => Term["ERA"] -->

---

A number is not a function, so its application does not reduce:

```wl
Term[TWnf[TApp[TNum[3], TNum[4]]]]
```
<!-- => Term["APP", Term["NUM", 3], Term["NUM", 4]] -->

---

A function used twice is shared with <code>[TDup]()</code>, whose two projections are applied separately, here to 1 and to 10:

```wl
Term[TWnf[TDup[TLam[x, x + 1], TOp2["+", TApp[#1, 1], TApp[#2, 10]] &]]]
```
<!-- => Term["NUM", 13] -->

## Possible Issues

[TApp]() takes exactly two arguments, and any other number returns unevaluated:

```wl
TApp[TNum[1], TNum[2], TNum[3]]
```
<!-- => TApp[TNum box 1, TNum box 2, TNum box 3], unevaluated -->
