# Heap

A flat array of `Term` cells with a single bump pointer. Single
threaded today. Defined in [src/thvm.h](../src/thvm.h)
(`HEAP`, `HEAP_NEXT`, `HEAP_CAP`), allocated by `thvm_init` in
[src/thvm.c](../src/thvm.c).

## Capacity

```c
#define HEAP_CAP (1ULL << 24)   // 16M cells * 8 bytes = 128 MiB
```

The cap is sized for tests and exploration, not production. Bump it
when needed.

## Allocator

[src/heap/alloc.c](../src/heap/alloc.c):

```c
fn u64 heap_alloc(u64 size) {
  u64 at   = HEAP_NEXT;
  u64 next = at + size;
  if (next > HEAP_CAP) {
    fprintf(stderr, "heap_alloc: out of memory ...");
    exit(1);
  }
  HEAP_NEXT = next;
  return at;
}
```

Bump-only, no freelist, no GC. Interaction-net programs do not need
a GC: terms become unreachable when they are erased by an interaction
(the eraser end of an active pair leaves dangling cells that are
never visited again). A periodic compaction pass could reclaim them
if memory pressure ever matters; today, it doesn't.

## Read / write / take

[src/heap/read.c](../src/heap/read.c),
[src/heap/set.c](../src/heap/set.c),
[src/heap/take.c](../src/heap/take.c):

```c
fn Term heap_read(u64 loc) { return HEAP[loc]; }
fn void heap_set(u64 loc, Term t) { HEAP[loc] = t; }
fn Term heap_take(u64 loc) { Term t = HEAP[loc]; HEAP[loc] = 0; return t; }
```

`heap_take` is "read and zero". The reducer uses it on DP0/DP1 entry
to claim the dup cell exactly once: the first projection to arrive
sees the body (and pushes a DP frame); the second sees a zero or, if
the first projection has already fired its interaction, sees a
SUB-tagged cell holding its value.

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

[src/heap/subst_var.c](../src/heap/subst_var.c):

```c
fn void heap_subst_var(u64 loc, Term value) {
  HEAP[loc] = term_sub_set(value, 1);
}
```

For DUP-style interactions, both projections need values, but only
one of them is "active" right now (the one whose frame is on the
stack). The active side gets the result returned; the other side gets
a substitution installed at the dup cell.

[src/heap/subst_cop.c](../src/heap/subst_cop.c):

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

`thvm_init()` calloc's `HEAP_CAP` cells (zeroed) and resets
`HEAP_NEXT` and the WNF stack. `thvm_free()` releases the buffers.
`TReset[]` from the WL paclet zeroes the heap and reinitializes the
counters without freeing.

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
