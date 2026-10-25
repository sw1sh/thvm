# Wolfram Language bridge

[wl/THVMLink/](../../wl/THVMLink/) is the LibraryLink paclet
(`WolframInstitute`THVMLink``) that exposes the C runtime to Wolfram: IC terms
and the heap, tensors and UOps, kernels and autotuning, the NN layers, the
optimizers, the JIT, and the ATP (automated theorem proving) entries
(`TFindProof`). This page covers the bridge and the IC-level constructors; the
tensor and NN surfaces are documented with their pieces under [../NN/](../NN/),
the ATP surface under [../ATP/](../ATP/).

WL-side style rules live in [wl/GUIDE.md](../../wl/GUIDE.md), and the paclet
documentation rules in [wl/THVMLink/docs/DOC_GUIDE.md](../../wl/THVMLink/docs/DOC_GUIDE.md).
Read them before touching anything under `wl/`.

## Layout

```
wl/THVMLink/
  PacletInfo.wl                   paclet manifest
  CSource/thvmlink.c              LibraryLink bridge, single-TU (#includes src/thvm.c)
  CSource/thvmlink_atp.c          the ATP entries of the bridge
  Kernel/THVMLink.wl              loader hub: LibraryFunctionLoad bindings,
                                  constants, IC constructors
  Kernel/*.wl                     one sibling package per concern (Tensor, Uop,
                                  Kernel, Jit, Optim, Heap, Visualization, ...)
  Kernel/NN/                      NN layers and model examples
  Kernel/ATP/                     TFindProof and the external-prover methods
  Tests/*.wlt                     VerificationTest suites; Tests/run.wls runs them
  docs/                           paclet documentation source (Markdown)
  LibraryResources/<platform>/    compiled library (gitignored)
```

[wl/THVMLink/Kernel/README.md](../../wl/THVMLink/Kernel/README.md) maps each
Kernel file to the symbols it owns.

## C bridge

[wl/THVMLink/CSource/thvmlink.c](../../wl/THVMLink/CSource/thvmlink.c)
includes the entire runtime via `#include "../../../src/thvm.c"` (single-TU
build) and exports the `thvm_wl_*` `EXTERN_C DLLEXPORT` functions;
[thvmlink_atp.c](../../wl/THVMLink/CSource/thvmlink_atp.c) adds the ATP ones.
The IC core of that surface is scalar-in, scalar-out:

| Kind       | Symbol               | Wraps                     |
| ---------- | -------------------- | ------------------------- |
| lifecycle  | `thvm_wl_init`       | `thvm_init()`             |
|            | `thvm_wl_free`       | `thvm_free()`             |
|            | `thvm_wl_reset`      | zero heap + stack + ITRS  |
| term       | `thvm_wl_term_new`   | `term_new`                |
|            | `thvm_wl_term_tag`   | `term_tag`                |
|            | `thvm_wl_term_ext`   | `term_ext`                |
|            | `thvm_wl_term_val`   | `term_val`                |
|            | `thvm_wl_term_sub`   | `term_sub_get`            |
| heap       | `thvm_wl_heap_pos`   | read the heap pointer     |
|            | `thvm_wl_heap_alloc` | `heap_alloc`              |
|            | `thvm_wl_heap_read`  | `heap_read`               |
|            | `thvm_wl_heap_set`   | `heap_set`                |
| reduce     | `thvm_wl_wnf`        | `wnf`                     |
| stats      | `thvm_wl_itrs`       | read `ITRS`               |

Higher-level constructors that bind names (`TLam[x, body]` with HoldAll,
`TDup[body]` returning `{dp0, dp1}`) are synthesized on the WL side from these
scalars, which keeps the C surface small and testable from C alone. The NN
layers go through one flat entry, `thvm_wl_nn` (see [../NN/nn.md](../NN/nn.md)).

## IC constructors

[wl/THVMLink/Kernel/THVMLink.wl](../../wl/THVMLink/Kernel/THVMLink.wl) maps
each bridge function to a named `LibraryFunctionLoad` result and builds the
constructors on two shared helpers:

```wolfram
heapWith[fields__] := With[{loc = THeapAlloc[Length[{fields}]]},
    ScanIndexed[THeapSet[loc + First[#2] - 1, numCoerce[#1]] &, {fields}];
    loc
]

heapTerm[tag_Integer, ext_Integer, fields__] := packTerm[0, tag, ext, heapWith[fields]]
```

`numCoerce` lifts a bare Integer field to the NUM with that value. Then
`TApp[f, x]` is `heapTerm[$TagAPP, 0, f, x]`, `TSup[label, a, b]` is
`heapTerm[$TagSUP, label, a, b]`, and so on.

`TLam[x, body]` needs the binder loc *before* it can compute the body (the body
references the binder), so it does not share `heapWith`. It uses `HoldAll` so
the body stays unevaluated until `Function[x, body]` substitutes the
freshly allocated `TVarFor[loc]` for `x`:

```wolfram
SetAttributes[TLam, HoldAll]
TLam[x_Symbol, body_] := With[{loc = THeapAlloc[1]},
    THeapSet[loc, numCoerce[Function[x, body][TVarFor[loc]]]];
    packTerm[0, $TagLAM, $lamSealExtFn[loc, 0], loc]
]
```

`TLam[body]` is the lambda whose binder is unused. `TDup[label, body]` returns
the pair `{dp0, dp1}`: one dup cell, two projections sharing it. `TDup[body]`
draws a fresh label from `TFreshLabel[]`, and the CPS variants
`TDup[body, k]` / `TDup[label, body, k]` apply `k` to the pair, which reads
well inside a term builder: `TDup[s, {s0, s1} |-> ...]`.

## Sugar

Two convenience rules let you write IC terms close to lambda-calculus
syntax:

| Sugar                        | Means                                  |
| ---------------------------- | -------------------------------------- |
| `t[arg]` for `t : TTerm`     | `TApp[t, arg]`                         |
| `(var \|-> body)[t]`         | `TApp[TLam[var, body], t]`             |

So `(var |-> var)[TEra[]]` is a literal beta-redex; `id[TEra[]]`
applies the identity lambda to the eraser; `church2[f, x]` would
chain into `church2[f][x]` (left-associative application).

The `Function` UpValue is guarded by an internal flag so `TLam`'s own
binder call (`Function[x, body][TVarFor[loc]]`) does not trigger it.

## Inspection helpers

| Symbol          | Returns                                                                          |
| --------------- | -------------------------------------------------------------------------------- |
| `TTagName[t]`   | Human name (e.g. `"APP"`) for a tag id                                           |
| `TTermInfo[t]`  | `<\| "sub", "tag", "tagName", "ext", "val", "raw" \|>`                           |
| `THeap[]`       | `<\| "nextLoc" -> n, "cells" -> <\| loc -> info, ... \|>, "Graph" -> g \|>`      |
| `THeapGraph[]`  | A Wolfram `Graph[]` of the heap; same object as `THeap[]["Graph"]`               |
| `TItrs[]`       | Cumulative interaction counter                                                   |

`THeap[]` is the primary tool for eyeballing what the runtime did:
you get every live cell pre-decoded plus the IC-style graph snapshot
at `["Graph"]`. The graph model and worked examples live in
[heap_graph.md](heap_graph.md).

## Summary boxes (Format.wl)

`Kernel/Format.wl` defines `MakeBoxes` rules so heavy return values
render compactly in a notebook instead of dumping their raw shape.

| Object                               | Summary box                                  |
| ------------------------------------ | -------------------------------------------- |
| `THeap[<\| ... \|>]` snapshot result | One-line: `nextLoc`, cell count, mini graph  |
| `TTermInfo[<\| ... \|>]` result      | One-line: `tag`, `ext`, `val`, SUB indicator |
| `THeapGraph` `Graph[]` result        | Inherits Wolfram's Graph display + a label   |

Each formatter is a single `MakeBoxes[...] := ...` rule keyed off the
Association shape (or wrapper head) of the value. Adding a new object
means: declare it in `THVMLink.wl`, then add one rule in `Format.wl`.

The split keeps `THVMLink.wl` focused on API and bridge bindings; the
formatter file is the single place to look for "how does this thing
render".

## Build

```sh
make wl            # compile the bridge into LibraryResources/<platform>/
make wl-test       # build, then run every wl/THVMLink/Tests/*.wlt
make wl-cross      # libraries for all five Wolfram platforms (PacletPack)
```

The Makefile picks the newest `/Applications/Wolfram*.app` that ships the
LibraryLink headers; override with
`make WOLFRAM_APP="/Applications/Wolfram X.Y.app" wl`.

## Tests

[wl/THVMLink/Tests/run.wls](../../wl/THVMLink/Tests/run.wls) loads the paclet,
calls `TestReport` on every `.wlt` in the Tests directory, prints a one-line
summary, lists any failures, and exits non-zero on any failure.
[core.wlt](../../wl/THVMLink/Tests/core.wlt) holds the IC-level specs.
