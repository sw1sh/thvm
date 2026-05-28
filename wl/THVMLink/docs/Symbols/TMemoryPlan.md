---
Template: Symbol
Name: TMemoryPlan
Context: THVMLink`
Paclet: WolframInstitute/THVMLink
URI: WolframInstitute/THVMLink/ref/TMemoryPlan
Keywords: [memory, scheduler, buffer, alive span, alias, Gantt]
SeeAlso: [TMemoryPlanReport, TMemoryPlanGantt, TScheduleGraph, TKernel, TCpuBufTable, TMetalBufSummary]
RelatedGuides: [THVMLink]
---

## Usage

<code>[TMemoryPlan]()[]</code> returns a `TMemoryPlan[<|...|>]` snapshot of the live `thvm` schedule with per-kernel topological depths and per-buffer alive-span intervals derived from the producer-kid / input-tid edges.

## Details & Options

- The snapshot is alias-aware: `TenDesc`s sharing one `buf_id` collapse into a single `Bufs` entry whose `alias_tids` lists every contributing tid.
- Each `Bufs` row carries `alloc_depth`, `last_use_depth`, `alive_span`, `nbytes`, `backend_id`, and a `status` tag (live / dead / external).
- Pass the result through <code>[TMemoryPlanReport]()</code> for a top-k Column or <code>[TMemoryPlanGantt]()</code> for a Gantt visualization of buffer life cycles.
- The plan is a snapshot, not a live view; re-call after each <code>[TMaterialize]()</code> or <code>[TRealize]()</code> to refresh.

## Basic Examples

Snapshot the plan for a tiny forward pass:

```wl
Needs["THVMLink`"];
TInit[];
W = TGlorot[{4, 8}]; b = TZeros[{8}]; x = TTensorCreate[ConstantArray[1., {4}]];
TMaterialize @ TLinear[x, W, b];
TMemoryPlan[]
```
<!-- => TMemoryPlan[<|"Kernels" -> {...}, "Tens" -> {...}, "Bufs" -> {...}, "Peak" -> <|"peak_bytes" -> _, ...|>|>] -->

## Scope

Report the headline metrics as a Column:

```wl
TMemoryPlanReport @ TMemoryPlan[]
```
<!-- => Column[{top-5 by bytes, top-5 by span, status counts, total live bytes}] -->

---

Render the alive spans as a Gantt:

```wl
TMemoryPlanGantt @ TMemoryPlan[]
```
<!-- => a Graphics with one horizontal bar per buffer, colour-coded by status -->

## Applications

Track peak memory across two graph shapes - the loss-adorned variant should retain more bytes than the bare forward:

```wl
peakBytes = Function[ step,
    TMaterialize[step];
    First @ TMemoryPlan[]["Peak"]["peak_bytes"]
];
peakBytes /@ {TLinear[x, W, b], TL2Loss @ TLinear[x, W, b]}
```
<!-- => {bytes1, bytes2} -- bytes2 >= bytes1 since L2Loss adds intermediates -->

## Properties and Relations

The plan's `Bufs` records sum to roughly the same live-byte total as the lower-level <code>[TCpuBufTable]()</code> for the CPU back end - both count the same backing buffers:

```wl
liveBufs   = Select[ First[TMemoryPlan[]]["Bufs"], #["backend_id"] === 1 &];
totalPlan  = Total[ #["nbytes"] & /@ liveBufs ];
{totalPlan, TTotalBufBytes[]}
```
<!-- => {planBytes, tableBytes} - same order of magnitude, often equal -->

## Possible Issues

The `nbytes` field reports the planner's view of the buffer size and may diverge from the runtime allocator's actual reservation when alignment padding or pooling is in play. Use the back-end-specific summaries (<code>[TMetalBufSummary]()</code>, <code>[TCpuBufTable]()</code>) when you need the allocator-true number.
