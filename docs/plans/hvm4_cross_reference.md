# thvm vs HVM4 cross-reference

Snapshot taken after PLAN.md steps 0-9 land. Cross-checked against
the `TinyHVM/HVM4/clang/` reference. Refresh whenever `src/` gains a
new interaction or whenever HVM4 itself moves.

## 1. Bit layout and term accessors

**Identical.** Same field widths, shifts, masks, and accessor signatures.

| Item              | HVM4                                                             | thvm                                                  |
| ----------------- | ---------------------------------------------------------------- | ----------------------------------------------------- |
| Layout            | `[SUB:1][TAG:7][EXT:18][VAL:38]`                                 | same -- [src/thvm.h](../../src/thvm.h)                |
| `term_new`        | [HVM4 clang/term/new.c](../../TinyHVM/HVM4/clang/term/new.c)     | [src/term/new.c](../../src/term/new.c) -- same body   |
| `term_tag/ext/val`| [clang/term/tag.c et al.](../../TinyHVM/HVM4/clang/term/tag.c)   | [src/term/](../../src/term/) -- same shifts           |
| `term_sub_get/set`| [clang/term/sub/get.c](../../TinyHVM/HVM4/clang/term/sub/get.c)  | [src/term/sub/get.c](../../src/term/sub/get.c) -- same|

## 2. Tag set

**Major divergence** -- HVM4 has ~46 tags, we have 8.

| Common (we have)   | HVM4-only                                                                          |
| ------------------ | ---------------------------------------------------------------------------------- |
| APP, LAM, VAR, ERA | `REF` (named def), `NAM` (free var), `DRY` (stuck dryad), `ALO` (lazy alloc)       |
| DP0, DP1, SUP, DUP | `MAT`/`SWI` (pattern match), `USE` (let), `OP2`/`NUM` (numeric arith)              |
|                    | `DSU`/`DDU` (dynamic labels), `EQL`/`AND`/`OR`/`ANY`/`INC` (type enumeration)      |
|                    | `UNS` (unscoped binder), `BJV`/`BJ0`/`BJ1` (quoted de Bruijn), `PRI` (primitive)   |
|                    | `C00`..`C16` (17 constructor tags)                                                 |
|                    | `F_OP2_NUM`, `F_EQL_L`, `F_EQL_R` (internal stack-frame tags, range `0x40+`)       |

Tag numbering also differs: we use `APP=0, LAM=1, VAR=2, ERA=3, ...`;
HVM4 uses `APP=0, VAR=1, LAM=2, DP0=3, DP1=4, SUP=5, DUP=6, ALO=7, ...`.
Numbers are local choices; what matters is that we never compare tag
values across the boundary.

## 3. Heap

**Simplified.** Functionally same allocator, single-threaded.

| Item                 | HVM4                                                              | thvm                                                    |
| -------------------- | ----------------------------------------------------------------- | ------------------------------------------------------- |
| Layout               | per-thread bump (`HEAP_NEXT[tid * HEAP_STRIDE]`) with `HEAP_END[]`| single global `HEAP_NEXT` -- [src/heap/alloc.c](../../src/heap/alloc.c) |
| Capacity             | `HEAP_CAP = 1ULL << 38` (~256 GiB worth of `u64`)                 | `HEAP_CAP = 1ULL << 24` (128 MiB)                       |
| Threads              | up to `MAX_THREADS = 64`, work-stealing for parallel collapse     | single-threaded                                         |
| `heap_subst_var`     | identical body                                                    | [src/heap/subst_var.c](../../src/heap/subst_var.c)      |
| `heap_subst_cop`     | uses `heap_set_rel` (release-store atomic for parallel collapse)  | uses plain `heap_set` -- [src/heap/subst_cop.c](../../src/heap/subst_cop.c); equivalent in 1-thread |
| `heap_set_rel`       | distinct release-ordered store                                    | absent                                                  |
| `heap_subst_var_dup` | wraps subst for DUP-NOD path                                      | folded into `heap_subst_cop` (no DUP-NOD)               |

## 4. WNF stack-machine reducer

**Same shape, vastly trimmed.**

`enter` / `apply` two-phase loop is structurally identical to HVM4's
[clang/wnf/_.c](../../TinyHVM/HVM4/clang/wnf/_.c) -- same
`goto enter` / `goto apply` skeleton, same `base = WNF_S_POS`
snapshot, same WHNF-walks-down semantics.

What we omit from HVM4's enter switch:

- `REF` -- named book definitions; needs a parser + book table.
- `ALO` -- lazy allocation of static (book) terms with their bind-list.
- `PRI` -- primitive function dispatch.
- `OP2`, `EQL`, `AND`, `OR`, `DSU`, `DDU` -- push specialized frames,
  descend into strict argument.
- `UNS` -- unscoped-binder rewrite at enter time.

What we omit from HVM4's apply switch:

- All the frames listed above (we only handle APP, DP0, DP1).
- For APP frame: only LAM and ERA dispatched; HVM4 also dispatches
  SUP, INC, MAT/SWI, USE, NAM, BJV, BJ0, BJ1, DRY, NUM, C00..C16.
- For DP frame: only SUP and ERA; HVM4 also dispatches NAM/BJ*, LAM,
  ANY, PRI, NUM, DRY, MAT, SWI, USE, INC, OP2, DSU, DDU, C00..C16
  (most via `wnf_dup_nod` or `wnf_dup_nam`).

Other reducer features in HVM4 that we don't have:

- `wnf_rebuild` -- cooperative cut-off for `STEPS_ITRS_LIM`. Walks
  the stack and rebuilds heap-backed nodes so reduction can resume
  later.
- `wnf_at(loc)` -- reduce-at-loc and memoize the result back into the
  heap cell.
- `wnf_steps_at(loc)` -- single-step debug driver; sets
  `STEPS_LAST_ITR` after each interaction so a tracing UI can print
  state.
- `DEBUG` flag with `printf` traces at each enter/apply.
- `ITRS_INC(name)` takes a string label for the last-fired
  interaction. We use bare `ITRS++`.
- Per-thread WNF banks (`WnfBank`) with cache-aligned padding.

## 5. Implemented interactions

### APP-LAM

**Simplified.** Same body as HVM4's
[wnf/app_lam.c](../../TinyHVM/HVM4/clang/wnf/app_lam.c), minus the
`LAM_ERA_MASK` short-circuit:

```diff
  fn Term interact_app_lam(Term lam, Term arg) {
    ITRS++;
    u64  loc  = term_val(lam);
    Term body = heap_read(loc);
-   if (lam_ext & LAM_ERA_MASK) {
-     return body;          // erase the arg without writing the binder
-   }
    heap_subst_var(loc, arg);
    return body;
  }
```

We don't have `LAM_ERA_MASK` because we don't yet have LAMs whose
binder is statically known to be unused -- that flag is set by HVM4's
parser. When we add a parser, we can revisit.

### APP-ERA

**Identical.** HVM4's [wnf/app_era.c](../../TinyHVM/HVM4/clang/wnf/app_era.c)
returns `term_new_era()`; we return `term_new(0, TAG_ERA, 0, 0)`.
Same effect.

### DUP-SUP

**Same-label identical, commute deferred.**

HVM4's [wnf/dup_sup.c](../../TinyHVM/HVM4/clang/wnf/dup_sup.c)
implements both:

```c
  if (lab == sup_lab) { /* annihilate -- same as ours */ }
  else {
    u64 base = heap_alloc(4);
    Copy A   = term_clone_at(sup_loc + 0, lab);
    Copy B   = term_clone_at(sup_loc + 1, lab);
    Term s0  = term_new_sup_at(base + 0, sup_lab, A.k0, B.k0);
    Term s1  = term_new_sup_at(base + 2, sup_lab, A.k1, B.k1);
    return heap_subst_cop(side, loc, s0, s1);
  }
```

Our [src/interact/dup_sup.c](../../src/interact/dup_sup.c) leaves the
commute branch stuck (writes the SUP back into the dup cell and
returns the projection). Implementing it requires `term_clone_at`
(HVM4's static-term cloner that spawns DP0/DP1 over book terms) --
which depends on the static-term/ALO infrastructure we don't have. A
straight dynamic clone is doable; we just haven't needed it.

### DUP-ERA

**Special case of HVM4's DUP-NOD with arity 0.**

HVM4 does not have a dedicated `dup_era`; ERA is handled by the
`ari == 0` branch of
[wnf_dup_nod](../../TinyHVM/HVM4/clang/wnf/dup_nod.c):

```c
if (ari == 0) {
  heap_subst_var_dup(loc, term);
  return term;
}
```

Our [src/interact/dup_era.c](../../src/interact/dup_era.c) is the
same shape but specialized:
`heap_subst_cop(side, loc, era, era)`. The reducer dispatches it
directly from the DP frame's `case TAG_ERA` instead of going through
a generic `dup_nod`.

## 6. Interactions present in HVM4 but absent here

Anything below would need new files under `src/interact/` plus
dispatch entries in [src/wnf/_.c](../../src/wnf/_.c) before the
related test could land.

| HVM4 file                   | Active pair                       | Why deferred                        |
| --------------------------- | --------------------------------- | ----------------------------------- |
| `wnf/dup_sup.c` commute     | DP * SUP, different labels        | Needs dynamic cloner (no test yet)  |
| `wnf/dup_lam.c`             | DP * LAM (clones the lambda)      | Non-trivial; no test yet            |
| `wnf/dup_nod.c` (ari > 0)   | DP * CTR (clones n-ary node)      | We don't have constructors          |
| `wnf/dup_nam.c`             | DP * NAM/BJV/BJ0/BJ1              | We don't have NAM/BJ tags           |
| `wnf/app_sup.c`             | APP * SUP (commute)               | No test yet                         |
| `wnf/app_inc.c`             | APP * INC                         | We don't have INC                   |
| `wnf/app_mat_*`             | MAT/SWI dispatch                  | We don't have MAT/SWI/CTR           |
| `wnf/op2_*`, `wnf/eql_*`,   | OP2 / EQL / AND / OR / DSU / DDU  | Whole numeric + type-enum machinery |
| `wnf/and_*`, `wnf/or_*`,    |                                   |                                     |
| `wnf/dsu_*`, `wnf/ddu_*`    |                                   |                                     |
| `wnf/use_*`                 | USE (let) frame dispatch          | We don't have USE                   |

## 7. Surrounding infrastructure absent

Whole subsystems HVM4 has and we don't:

- **Parser** (`clang/parse/*`): textual `.hvm` source -> static book
  terms. We construct terms by hand in tests / WL.
- **Book + REF** (`BOOK[]` global, `wnf` REF case, `prim/*`): named
  definitions, primitives.
- **CNF / collapse** (`clang/cnf/_.c`, `clang/eval/collapse.c`,
  `data/pq.c`, `data/wspq.c`): readback of superposed branches with
  priority queue, parallel work-stealing.
- **AOT** (`clang/aot/`): ahead-of-time C codegen for static book terms.
- **FFI** (`clang/ffi/api.c`, `ffi/load.c`): dlopen-based extension loading.
- **Worker threads** (`clang/thread/*`, the per-thread
  `WnfBank`/`WnfItrsBank`/heap arrays).
- **Print + naming** (`clang/print/*`, `clang/nick/*`,
  `clang/table/*`): pretty-printer, alpha-name generation, base64-ish
  nick encoding, global name table.
- **System utilities** (`clang/sys/*`): file IO, mmap helpers, error
  formatting.
- **Runtime entry** (`clang/runtime/*`, `clang/main.c`): CLI binary,
  prepare/eval entry points.
- **Step / debug tracing** (`STEPS_*`, `DEBUG`, `STEPS_ROOT_LOC`).

## 8. Style / convention parity

- **Path = function name**: same in both.
  [src/wnf/_.c](../../src/wnf/_.c) defines `wnf()`,
  [src/interact/app_lam.c](../../src/interact/app_lam.c) defines
  `interact_app_lam()`. Naming convention follows HVM4's
  [STYLEGUIDE.md](../../TinyHVM/HVM4/clang/STYLEGUIDE.md).
- **Single TU**: both projects build via one `#include` chain. HVM4's
  hub is [clang/hvm.c](../../TinyHVM/HVM4/clang/hvm.c) (533 lines of
  `#include`s); ours is [src/thvm.c](../../src/thvm.c) (~50 lines).
- **`fn` macro**: same alias for `static inline`.
- **Switch-on-tag**: same convention.

## 9. Naming we diverge on

| HVM4               | thvm                            | Reason                                                  |
| ------------------ | ------------------------------- | ------------------------------------------------------- |
| `wnf_<pair>`       | `interact_<pair>`               | We separated the *interaction rule* (under `src/interact/`) from the *dispatch loop* (under `src/wnf/`); HVM4 puts both in `wnf/`. |
| `term_new_era()`   | `term_new(0, TAG_ERA, 0, 0)`    | We don't yet have the per-tag `term_new_<tag>()` constructors HVM4 generates. |
| `LAM_ERA_MASK`     | (absent)                        | Set by parser; we have no parser.                       |

## Summary

We have a faithful subset: same bit layout, same heap substitution
model, same enter/apply skeleton, four interactions that match HVM4's
behaviour where they overlap. The big gaps are:

1. **Tag-set surface** -- we cover the IC core (lambda calculus +
   DUP/SUP), HVM4 covers IC + numeric + ICC type machinery + constructors.
2. **Cloner / commute rules** -- we punt to "stuck" instead of
   allocating + cloning.
3. **Multi-threading** -- we drop the per-thread arrays, atomics,
   work-stealing.
4. **Frontend + readback** -- no parser, no book, no CNF collapse, no
   printer.

For step 6's stated goal ("just enough to pass initial unit tests"),
this is the right surface. As tests drive in DUP-LAM, APP-SUP, or
commute, each interaction can be ported nearly verbatim from the
corresponding HVM4 file.
