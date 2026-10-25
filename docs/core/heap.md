# Heap

A flat array of `Term` cells with a bump pointer per context, collected by a
Cheney copying GC. `HEAP` and `HEAP_CAP` are defined in
[src/thvm.h](../../src/thvm.h); the arenas are mapped by `thvm_init` in
[src/thvm.c](../../src/thvm.c).

## Capacity

```c
#define HEAP_CAP (1ULL << 28)   // 256M cells * 8B = 2 GiB
```

`THVM_HEAP_CELLS` overrides the allowance once per process
(`thvm_heap_cells`, [src/heap/collect.c](../../src/heap/collect.c)). The dyn
heap and the book heap (static terms, `THVM_BOOK_CELLS`) are `mmap(MAP_ANON)`
arenas: reserving the range costs no resident memory until cells are touched,
and the pages are page-aligned so Metal can wrap the book heap as a shared
buffer.

## Allocator

[src/heap/alloc.c](../../src/heap/alloc.c): `heap_alloc(size)` bumps
`CURRENT_CTX->heap_next` with an atomic fetch-add, so the parallel `nf` pool
([src/wnf/pool.c](../../src/wnf/pool.c)) allocates without losing cells. The
limit is the end of the GC from-space when the GC is on, else the heap
allowance. Exhaustion calls `thvm_fatal`, which returns an error to Wolfram
when a LibraryLink entry installed a recovery point and exits otherwise.

## Garbage collection

[src/heap/collect.c](../../src/heap/collect.c) splits `HEAP` into two
semi-spaces. After a realize under heap or device pressure
(`thvm_realize_maybe_gc`, [src/schedule/realize.c](../../src/schedule/realize.c);
`THVM_GC_KB`, `THVM_GC_DEVICE_MB`) `gc_collect` evacuates the roots and every
side table that holds Terms; tables keyed by loc re-key through forward
pointers, and the hash-cons and upload caches are cleared. A permanent tail
past the semi-spaces (`THVM_HEAP_PERM_CELLS`, `heap_perm_alloc`) holds
atom-only nodes that are never collected.

## Read / write

[src/heap/read.c](../../src/heap/read.c), [src/heap/set.c](../../src/heap/set.c)
are plain loads and stores. The concurrent variants pair up for the parallel
pool: `heap_set_rel` ([src/heap/set_rel.c](../../src/heap/set_rel.c)) and
`heap_subst_var` release-store, `heap_read_acq`
([src/heap/read_acq.c](../../src/heap/read_acq.c)) acquire-loads, and
`heap_take_atomic` ([src/heap/take_atomic.c](../../src/heap/take_atomic.c))
exchanges a cell with 0, so of the workers racing for a DP body exactly one
gets it.

## Substitution model

The heart of how interactions communicate. There is no separate
"substitution table"; the heap *is* the table.

When an active pair fires, one side often needs to bind a value to
something the other side will eventually look up. The convention:

1. The looker-up (a `VAR`, `DP0`, or `DP1` term) holds the binder loc
   in its `val` field.
2. The binder writes the bound value at that loc with the SUB bit set.
3. When the looker-up is entered next, it reads the cell, sees SUB=1,
   strips the bit with `term_sub_set(cell, 0)`, and continues with
   that value as if the indirection never existed.

[src/heap/subst_var.c](../../src/heap/subst_var.c):

```c
fn void heap_subst_var(u64 loc, Term value) {
  __atomic_store_n(&HEAP[loc], (u64)term_sub_set(value, 1), __ATOMIC_RELEASE);
  WIRE_PROV_BUMP(loc);
}
```

For DUP-style interactions, both projections need values, but only
one of them is "active" right now (the one whose frame is on the
stack). The active side gets the result returned; the other side gets
a substitution installed at the dup cell.

[src/heap/subst_cop.c](../../src/heap/subst_cop.c):

```c
fn Term heap_subst_cop(u8 side, u64 loc, Term r0, Term r1) {
  heap_subst_var(loc, side == 0 ? r1 : r0);
  return side == 0 ? r0 : r1;
}
```

If side 0 (DP0) is active: substitute `r1` at `loc` for DP1 to find
later, return `r0` for DP0 to continue with right now. Symmetric for
side 1.

## Lifecycle

`thvm_init()` maps the arenas and resets the heap pointer and the WNF stack;
`thvm_free()` unmaps them. `TReset[]` from the WL paclet zeroes the heap and
reinitializes the counters without unmapping.

## Worked example: APP-LAM substitution flow

Reducing `(lam x. x) ERA`:

```
before:
  heap[lam_loc]     = VAR(val=lam_loc)
  heap[app_loc + 0] = LAM(val=lam_loc)
  heap[app_loc + 1] = ERA

interact_app_lam(LAM, ERA) called by apply phase:
  body = heap_read(lam_loc) = VAR(val=lam_loc)
  heap_subst_var(lam_loc, ERA)
    heap[lam_loc] = ERA with SUB=1
  return body                   // VAR(val=lam_loc)

reducer re-enters body:
  cell = heap_read(lam_loc) = ERA with SUB=1
  SUB set, so next = term_sub_set(cell, 0) = ERA
  enter ERA
  ERA is WHNF; return
```

One heap write to install the binding, no allocation, no extra
indirection. The substitution is consumed exactly once because only
one VAR refers to `lam_loc`. (In the duplication case, multiple
references can exist; that is what DP0/DP1 + `subst_cop` handle.)
