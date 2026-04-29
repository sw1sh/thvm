# wl/THVMLink/Kernel — file map

Each `.wl` here is a sibling package: every file does
`BeginPackage["THVMLink`"]` + `Begin["`Private`"]` and is auto-loaded
in alphabetical order by `THVMLink.wl` (which Get's every sibling
through a `Sort @ FileNames` scan). They all share the
`THVMLink`Private` context, so cross-file references resolve via
`SetDelayed` regardless of load order.

When adding a new symbol, **place its `::usage` and definition in
the file that owns its semantic concern**, not in `THVMLink.wl`. The
loader hub there is for LibraryFunctionLoad bindings only.

## Ownership matrix

| File                | What it owns                                                                                                                                    |
| ------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------- |
| `THVMLink.wl`       | LibraryLink loaders (`load["thvm_wl_*"]`), tag/dtype/opcode constants, `ensureInit`, `TWnf`/`TNf`/`TStep`/`TItrs`/`TInteract`, `TTermExpr`/`TTermTree`, `TLam`/`TApp`/`TSup`/`TDup` constructors, `TFreshLabel`, low-level `TTerm` accessors. |
| `Tensor.wl`         | `TTensor`, `TTensorCreate`, `TTensorShape`, `TTensorData`, `TRealize`, `TMaterialize`, NumericArray bridges.                                    |
| `Uop.wl`            | UOp graph constructors (`TUOpAdd`, `TUOpMul`, `TUOpReshape`, ...) and `TUOpKind`/`TUOpSrcs` introspection.                                      |
| `Kernel.wl`         | `TKernel` typed wrapper + property surface (`Information[k]`, `k["Name"]`, `k[]`). Top-level kid-keyed accessors: `TKernelCount`, `TKernelInfo`, `TKernelProgramCacheSize`, `TKernelSourceC`/`SourceMetal`, `TKernelFlops`, `TKernelDispatchKind`/`Count`/`TotalUs`, `TKernelJitDylibPath`, `TKernelProfile`, `TProfileAll`. |
| `MemoryPlan.wl`     | mp1 bridge tables: `TKernelTable`, `TKernelInputs`, `TTensTable`, `TCpuBufTable`, `TMetalBufTable`, `TTensCount`, `TTotalBufBytes`. The `TMemoryPlan` snapshot + `TMemoryPlanReport`. Helpers (`linearScanPack`, `peakConcurrentLive`, `statusFill`/`Edge`, `formatBytes`, `backendsActive`) shared with the Gantt renderer. |
| `Heap.wl`           | `THeap`, heap walker, `THeapCells`, def-table accessors.                                                                                        |
| `Visualization.wl`  | `THeapGraph` (heap as IC string-diagram), `TScheduleGraph` (kernel DAG), and `TMemoryPlanGantt` (buffer-lifecycle Gantt over a `TMemoryPlan`).  |
| `Diagram.wl`        | `THeapDiagram` (DiagramNetwork rendering).                                                                                                      |
| `Format.wl`         | `MakeBoxes` summary boxes for every atomic THVMLink object: `THeap`, `TTerm` (+ TEN/UOP/NUM specializations), `TKernel`, `TMemoryPlan`, `TContext`. |
| `Style.wl`          | Shared color/shape primitives: `$nodeStyle`, `nodeShapeFn`, `edgeStyleDirective`. Everything in `Visualization.wl`/`Diagram.wl`/`Kernel.wl`/`MemoryPlan.wl` paints through these so KERNEL/TEN/UOP look the same everywhere. |
| `Context.wl`        | `TContext` multi-heap handle.                                                                                                                   |
| `Switch.wl`         | `TSwitch` runtime branching primitive.                                                                                                          |
| `Pri.wl`            | `TPri` pinned-foreign-callback bridge (FFI/CompiledFunction slots).                                                                             |
| `Bench.wl`          | `TBench` micro-benchmark harness.                                                                                                               |
| `Optim.wl`          | `TOptim` optimizer wrappers (SGD, Adam, ...).                                                                                                   |
| `NN.wl`             | Neural-net building blocks (Dense, etc.).                                                                                                       |
| `ATP.wl`            | `TATP` autodiff/term-pin programs.                                                                                                              |
| `Profile.wl`        | `TProfile` aggregate (TItrs, kernel counts, heap pressure) -- distinct from `TKernelProfile` which is per-kernel.                               |
| `Ref.wl`            | `TRef` mutable-cell primitive.                                                                                                                  |
| `Shape.wl`          | `TTermShape` shape inference + scalar-text utilities.                                                                                           |

## Inter-file conventions

- Every `Begin["`Private`"]` block lives in the same `THVMLink`Private`
  namespace. Helper symbols are visible across files; just call them.
  No `THVMLink`Private`$xxxFn` qualification needed (and adding one
  is a sign the symbol is being misused).
- LibraryLink loaders (`$kernelTableFn`, `$kernelInfoFn`, ...) live in
  `THVMLink.wl` only. Other files reference them by name; the
  `SetDelayed` resolution happens at call time, after every sibling
  has been Get'd.
- Public `::usage` strings always sit beside their definition. If a
  function is defined in `Kernel.wl`, its `::usage` is in `Kernel.wl`.
  `THVMLink.wl` is no longer a usage hub.
- Visual primitives go through `Style.wl`. Don't write
  `RGBColor[...]` or `LightDarkSwitched[Lighter[...], Darker[...]]`
  in a renderer file; add a category to `$nodeStyle` and route through
  `nodeShapeFn`/`drawNode`. (See `wl/GUIDE.md` for the dark-mode rules.)
- Summary boxes go through `Format.wl`, not next to the constructor.
  The constructor file owns `tFooQ` predicates and the actual
  `TFoo[<|...|>]` shape; `Format.wl` owns the rendering.

## When adding a new TKernel property

1. Add its `::usage` line in `Kernel.wl` next to the rest.
2. Append the property name to `$tKernelProperties` (the public
   contract for `Information[k, "Properties"]`).
3. Add a `tKernelProp[k:TKernel[a_Association], "NewProp"] := ...`
   clause inside `Begin["`Private`"]`.
4. If you want a top-level kid-keyed convenience accessor, add
   `TKernelNewProp[kid_Integer] := TKernel[kid]["NewProp"]` so the
   property surface stays the single source of truth.
