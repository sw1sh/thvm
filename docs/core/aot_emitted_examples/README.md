# Emit snapshots

Commit-tracked snapshots of what `TAOTEmit[name]` produces for canonical
def shapes. Diff across emitter commits to see what changed without
having to re-run the WL kernel.

| File | Pattern | What it exercises |
|---|---|---|
| `identity_lam.c` | `TLam[x, x]` | minimal scaffolding (FN_<name>, par_<name>_entry, dispatch, register); body falls into the iter-1 stub since identity isn't an App-of-Mat shape |
| `tree_sum.c` | `sum(leaf{v}) = v; sum(node{l, r}) = sum(l) + sum(r)` | dead-arm pruning (CTR-only chain), multi-LAM CTR destructure, sibling-pair `TOp2(+)` → `R_SPLIT` + cont, OP2 fold cont with `lv + rv` constant-folded from OP_ADD |
| `tree_build.c` | `build(0, x) = leaf{x}; build(d, x) = node{build(d-1, x), build(d-1, x)}` (Bend2 reference shape) | sibling-pair `TCtr2` → `R_SPLIT` + cont (cont wraps in `aot_make_ctr2(2, c0, c1)` for `node{l, r}`), 2-arity self-call, **DP* emit** (auto-dup of `dd` and `x` since each is used twice — `aot_heap_alloc` + `heap_set` + `term_new(TAG_DP0/DP1, label, loc)` per encounter), wnf-force at dispatch so DP-wrapped CTRs unwrap before tag check |

## Regenerate

```
bash docs/aot_emitted_examples/_gen.sh
```

Each example uses a fresh `wolframscript` invocation because chaining
multiple `TInit`/`TDef`/`TAOTEmit` cycles in one session segfaults the
kernel (a known issue going back to the legacy AOT, unrelated to the
new emit code).

## Notes on the DP* emit

Both `tree_build` and `count(p, p)`-style defs use the same LAM-bound
var twice in the body. thvm auto-dups multiply-used vars into
`TAG_DP0` / `TAG_DP1` projections of a heap-side dup cell. The emit
now lowers these:

- **value position**: `aot_heap_alloc(1)` + `heap_set(loc, body)` +
  `term_new(0, TAG_DP0/DP1, label, loc)` per encounter
- **dispatch site**: `Term dv = wnf(t->args[0])` so DP-wrapped CTRs
  fire their DUP-CTR interaction and the underlying CTR is what hits
  the tag check

Limitation: every DP encounter currently allocates a FRESH dup cell.
`DP0` and `DP1` of the same source dup_loc don't share at runtime —
each gets its own copy of the body. Functionally correct (`count(p, p)`
still returns 2^N) but allocates 2× per multiply-used var. A memo-based
sharing pass would close that overhead; deferred.
