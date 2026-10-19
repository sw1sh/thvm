---
Template: Symbol
Name: TInit
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TInit
Keywords: [initialize, runtime, heap, reset, backend]
SeeAlso: [TReset, TInitialize, TKernelCount, TItrs, TFreshLabel, THeapPos]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TInit]()[]</code> initializes the runtime, or initializes it again, and gives [True]().

<code>[TInit]()[*ctx*]</code> initializes the runtime context *ctx*.

<!-- #| annotation: 26.10.19: TInit[] sets the WL label counter to 1, marks the current context slot initialized and runs thvm_init (src/thvm.c), which reallocates the heap, splits it into the two GC semi-spaces, clears the kernel, tensor and pin tables and installs the default backend from getenv("DEV"). Every heap-touching function runs ensureInit[] first, so a session never needs an explicit TInit. -->

## Details & Options

- The runtime initializes itself the first time a term or a tensor is built, so an explicit <code>[TInit]()[]</code> is rarely needed.
- Initializing again starts the runtime from scratch: the dynamic heap, the interaction counter of <code>[TItrs]()</code>, the label counter of <code>[TFreshLabel]()</code>, the pin table and the table of compiled kernels all start empty.
- Terms built before a new initialization refer to cells that no longer exist and are not to be used afterwards.
- The default device for new tensors is chosen when the runtime initializes, from the `DEV` environment variable, and is the central processing unit (CPU) when the variable is unset.
- *ctx* is a [TContext]() handle, such as `TContext[0]` for the default context.
- <code>[TReset]()</code> empties the heap and the counters without initializing the runtime again.

## Basic Examples

Initialize the runtime:

```wl
TInit[]
```
<!-- => True -->

## Scope

Build a term, which takes cells on the heap:

```wl
TSup[1, 2] + 3
```
<!-- => a TTerm summary box with tag OP2 -->

Initialize again:

```wl
TInit[]
```
<!-- => True -->

The heap is empty:

```wl
THeapPos[]
```
<!-- => 0 -->

---

Initialize the default context by its handle:

```wl
TInit[TContext[0]]
```
<!-- => True -->

---

Reduce a term, which fires interactions:

```wl
TTermVal /@ TCollapse[TSup[1, 2] + 3]
```
<!-- => {4, 5} -->

Initialize again:

```wl
TInit[]
```
<!-- => True -->

The interaction counter starts from zero:

```wl
TItrs[]
```
<!-- => 0 -->

The label counter starts from one:

```wl
TFreshLabel[]
```
<!-- => 1 -->

---

Realize a tensor sum, which compiles a kernel:

```wl
Normal[TRealize[TTensorCreate[{1., 2.}] + TTensorCreate[{3., 4.}]]]
```
<!-- => {4., 6.} -->

Initialize again:

```wl
TInit[]
```
<!-- => True -->

The kernel table holds only its initial entry:

```wl
TKernelCount[]
```
<!-- => 1 -->

## Properties and Relations

Initializing a runtime that is already initialized still gives [True]():

```wl
TInit[]
```
<!-- => True -->

---

<code>[TReset]()</code> also empties the heap, and gives 1:

```wl
TReset[]
```
<!-- => 1 -->

The heap is empty after the reset:

```wl
THeapPos[]
```
<!-- => 0 -->

## Possible Issues

[TInit]() takes no arguments, and other forms return unevaluated:

```wl
TInit[5]
```
<!-- => TInit[5] -->
