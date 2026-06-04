---
Template: Symbol
Name: TInit
Context: THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TInit
Keywords: [init, runtime, backend, setup]
SeeAlso: [TRealize, TMaterialize, TTensorCreate, TKernelCount, TScheduleGraph]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TInit]()[]</code> initializes the runtime and returns [True]().

## Details & Options

- The runtime initializes itself on first use, so an explicit <code>[TInit]()[]</code> is rarely needed; it earns its place only to reset state in isolation.
- Idempotent: calling it again on an already-initialized runtime simply returns [True]().
- Selects the active backend from the `DEV` environment variable (CPU when unset); see <code>[TTensorCreate]()</code> for how tensor buffers bind to that backend.

## Basic Examples

Initialize the runtime:

```wl
TInit[]
```
<!-- => True -->

## Properties and Relations

After initialization the kernel side table holds only the bootstrap entry; <code>[TKernelCount]()</code> grows as graphs are realized:

```wl
TInit[];
TKernelCount[]
```
<!-- => 1 -->
