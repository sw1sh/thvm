# Wolfram Language bridge

[wl/THVMLink/](../wl/THVMLink/) is a LibraryLink paclet that exposes
the C runtime to Wolfram. It is primarily an *observation* layer: you
construct terms from Wolfram, reduce them, inspect the heap as an
`Association`, and correlate results against `ITRS`. All higher-level
constructors (`TLam`, `TApp`, `TSup`, `TDup`) are synthesized on the
WL side from a minimal C surface.

WL-side style rules live in [wl/GUIDE.md](../wl/GUIDE.md). Read that
before touching anything under `wl/`.

## Layout

```
wl/THVMLink/
  PacletInfo.wl                   paclet manifest
  Kernel/THVMLink.wl              public API + LibraryFunctionLoad bindings
  Kernel/Format.wl                summary boxes for THeap, TTermInfo, ...
  CSource/thvmlink.c              LibraryLink bridge, single-TU
  Tests/core.wlt                  VerificationTest specs
  Tests/run.wls                   test runner script
  LibraryResources/<platform>/    compiled dylib (gitignored)
```

`Kernel/THVMLink.wl` `Get`s `Format.wl` after the public symbols are
declared so the formatter can attach `MakeBoxes` rules to them.

## C bridge (scalar-in, scalar-out)

[wl/THVMLink/CSource/thvmlink.c](../wl/THVMLink/CSource/thvmlink.c)
includes the entire runtime via
`#include "../../../src/thvm.c"` (single-TU build) and exports 14
`EXTERN_C DLLEXPORT` functions. Every function takes only `Integer`
arguments and returns a single `Integer`:

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
| heap       | `thvm_wl_heap_pos`   | read `HEAP_NEXT`          |
|            | `thvm_wl_heap_alloc` | `heap_alloc`              |
|            | `thvm_wl_heap_read`  | `heap_read`               |
|            | `thvm_wl_heap_set`   | `heap_set`                |
| reduce     | `thvm_wl_wnf`        | `wnf`                     |
| stats      | `thvm_wl_itrs`       | read `ITRS`               |

Tuple-returning constructors (`TLam` returning `{lam, var}`, `TDup`
returning `{dp0, dp1}`) are synthesized on the WL side from these
scalars. This keeps the C surface tiny and testable from C alone.

## WL package

[wl/THVMLink/Kernel/THVMLink.wl](../wl/THVMLink/Kernel/THVMLink.wl)
maps each scalar function to a named `LibraryFunctionLoad` result
(lazy, memoized via `:=`) and builds higher-level helpers on top.

High-level constructors use two shared helpers:

```wolfram
heapWith[fields__] := With[{loc = THeapAlloc[Length[{fields}]]},
    ScanIndexed[THeapSet[loc + First[#2] - 1, #1] &, {fields}];
    loc
]

heapTerm[tag_Integer, ext_Integer, fields__] :=
    TTermNew[0, tag, ext, heapWith[fields]]
```

Then `TApp[f, x]` is `heapTerm[$TagAPP, 0, f, x]`, `TSup[label, a, b]`
is `heapTerm[$TagSUP, label, a, b]`, and so on.

`TLam[builder]` needs the binder loc *before* it can compute the
body (the body references `TVarFor[loc]`), so it doesn't share
`heapWith`:

```wolfram
TLam[builder_] := With[{loc = THeapAlloc[1]},
    THeapSet[loc, builder[TVarFor[loc]]];
    TTermNew[0, $TagLAM, 0, loc]
]
```

`TDup[label, body, k]` uses a continuation to deliver the two
projections because `TDup` allocates a single dup cell but produces
two terms:

```wolfram
TDup[label_, body_, k_] := With[{loc = heapWith[body]},
    k[TTermNew[0, $TagDP0, label, loc],
      TTermNew[0, $TagDP1, label, loc]]
]
```

## Sugar

Two convenience rules let you write IC terms close to lambda-calculus
syntax:

| Sugar                        | Means                                  |
| ---------------------------- | -------------------------------------- |
| `t[arg]` for `t : TTerm`     | `TApp[t, arg]`                         |
| `(var \|-> body)[t]`         | `TApp[TLam[var \|-> body], t]`         |

So `(var |-> var)[TEra[]]` is a literal beta-redex; `id[TEra[]]`
applies the identity lambda to the eraser; `church2[f, x]` would
chain into `church2[f][x]` (left-associative application).

The `Function` UpValue is guarded by an internal flag so `TLam`'s own
binder call (`builder[TVarFor[loc]]`) does not trigger it.

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
make wl            # compile wl/THVMLink/CSource/thvmlink.c -> dylib
make wl-test       # build + run wl/THVMLink/Tests/run.wls
```

The Makefile auto-detects the newest `/Applications/Wolfram*.app`;
override with `make WOLFRAM_APP="/Applications/Wolfram X.Y.app" wl`.

## Tests

[wl/THVMLink/Tests/core.wlt](../wl/THVMLink/Tests/core.wlt) is a
sequence of `VerificationTest` entries. The runner
[wl/THVMLink/Tests/run.wls](../wl/THVMLink/Tests/run.wls) loads the
paclet, calls `TestReport` on every `.wlt` in the Tests directory,
prints a one-line summary to stdout, lists any failures, and exits
non-zero on any failure.
