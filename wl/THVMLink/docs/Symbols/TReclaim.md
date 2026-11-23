---
Template: Symbol
Name: TReclaim
Context: WolframInstitute`THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TReclaim
Keywords: [memory, buffer, reclaim, free, garbage collection]
SeeAlso: [TGCCollect, TExternPinCount, TReset, TJit, TRealize]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TReclaim]()[]</code> frees the tensor buffers that nothing reaches any more and gives the number of bytes freed.

<!-- #| annotation: 26.11.23: TReclaim wraps thvm_wl_reclaim (wl/THVMLink/CSource/thvmlink.c), the Wolfram-side twin of the Python bridge's py_reclaim: a collection that ends in the low semi-space (gc_collect_low) drops the dead heap cells, then mark_gc_preserve_host_roots (the extern pin table, that is every TTerm wrapper still held, plus the definitions and the pending wnf stack), mark_heap_rooted_preserve (every tensor a surviving heap cell names) and jit_capture_mark_preserved mark the live buffers, and cpu_buf_pool_free_unpreserved frees the rest. It acts on the central processing unit backend only: Metal frees unpreserved buffers at each realize. A buffer that a fired kernel read stays, because the kernel's entry keeps its source graph until TReset. Nothing is freed during a TJit capture. -->

## Details & Options

- A tensor's buffer lives as long as a term held by the Wolfram Language, a [TJit]() capture or a definition reaches it. On the central processing unit (CPU) backend the buffer of a dropped result is not freed on its own: [TReclaim]() frees it.
- [TReclaim]() first runs a garbage collection, as [TGCCollect]() does, so the terms held by the Wolfram Language stay valid and their wrappers follow the cells.
- The buffers a [TJit]() capture replays into are kept, whether or not their results are still held.
- A buffer that a realized kernel read as an input stays until <code>[TReset]()[]</code>.
- The result is the number of bytes freed, 0 when there was nothing to free.

## Basic Examples

Start from an empty runtime:

```wl
TReset[];
```

Realize a tensor to keep:

```wl
keep = TRealize[TTensorCreate[ConstantArray[1., 1000]] + 1.]
```
<!-- => a TTerm summary box with tag TEN -->

Realize a larger one:

```wl
drop = TRealize[TTensorCreate[ConstantArray[1., 100000]] * 2.]
```
<!-- => a TTerm summary box with tag TEN -->

While both are held there is nothing to free:

```wl
TReclaim[]
```
<!-- => 0 -->

Drop the larger one:

```wl
Clear[drop]
```

Its 100000 numbers of 4 bytes each are freed:

```wl
TReclaim[]
```
<!-- => 400000 -->

The tensor still held is intact:

```wl
Take[Normal[keep], 3]
```
<!-- => {2., 2., 2.} -->

## Properties and Relations

Start from an empty runtime:

```wl
TReset[];
```

Make an input tensor:

```wl
x = TTensorCreate[ConstantArray[1., 50000]]
```
<!-- => a TTerm summary box with tag TEN -->

Capture a function with [TJit]():

```wl
double = TJit[TRealize[# * 2.] &]
```
<!-- => a TJitClosure summary box -->

Run it once:

```wl
y = double[x]
```
<!-- => a TTerm summary box with tag TEN -->

Drop the result:

```wl
Clear[y]
```

The capture keeps the buffer it replays into:

```wl
TReclaim[]
```
<!-- => 0 -->

A replay still works:

```wl
Take[Normal[double[x]], 2]
```
<!-- => {2., 2.} -->
