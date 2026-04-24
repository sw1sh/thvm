# DUP-SUP

Source: [src/interact/dup_sup.c](../../src/interact/dup_sup.c).
Tested by [tests/test_dup_sup.c](../../tests/test_dup_sup.c) and a
VerificationTest in [wl/THVMLink/Tests/core.wlt](../../wl/THVMLink/Tests/core.wlt).

## Rule

Two cases, dispatched by comparing the dup label and the sup label.

### Same label (annihilate, implemented)

```
! &L{x0, x1} = &L{a, b}
----------------------- DUP-SUP (L == L)
x0 <- a
x1 <- b
```

The dup and the sup were "siblings" sharing a label; they cancel.
Each projection takes the corresponding branch of the SUP.

### Different label (commute, deferred)

```
! &L{x0, x1} = &R{a, b}        L /= R
-------------------------------------- DUP-SUP (L /= R)
! &L{A0, A1} = a
! &L{B0, B1} = b
x0 <- &R{A0, B0}
x1 <- &R{A1, B1}
```

The dup and the sup carry different labels; they commute. Each branch
of the sup is itself duplicated under the original dup label, and the
results are repackaged into two new sups carrying the *original* sup
label. This is the rule that makes interaction nets non-confluent in
a controlled way.

Not yet implemented in this codebase. The current code marks the
active pair as stuck (rebuilds the DP node holding the SUP) so the
result is well-formed but unreduced. A test that exercises this case
will drive the implementation.

## Implementation

```c
fn Term interact_dup_sup(u32 lab, u64 loc, u8 side, Term sup) {
  u64 sup_loc = term_val(sup);
  u32 sup_lab = term_ext(sup);
  if (lab == sup_lab) {
    ITRS++;
    Term tm0 = heap_read(sup_loc + 0);
    Term tm1 = heap_read(sup_loc + 1);
    return heap_subst_cop(side, loc, tm0, tm1);
  }
  // commute: stuck for now
  heap_set(loc, sup);
  return term_new(0, side == 0 ? TAG_DP0 : TAG_DP1, lab, loc);
}
```

`side` is 0 for DP0, 1 for DP1; the active projection (whichever one
the reducer popped from the stack) gets its branch returned and the
other branch is installed at the dup loc with SUB=1 via
`heap_subst_cop` (see [heap.md](../heap.md)).

The commute fall-through writes the SUP back into the dup cell so a
later entry of the same projection sees the structure that produced
the stuck state, not a corrupted intermediate.

## Worked example

```
! &7{x0, x1} = &7{ERA, LAM}; <body uses x0>
```

Reducing x0:

```
enter DP0 -> heap_take(dup_loc) = SUP, push DP0 frame, enter SUP
SUP is WHNF, pop DP0 frame, dispatch DUP-SUP:
  lab(DP0) == lab(SUP) (both 7), annihilate
  tm0 = ERA, tm1 = LAM
  heap_subst_cop(side=0, loc=dup_loc, ERA, LAM):
    heap[dup_loc] = LAM with SUB=1   (DP1 will pick this up later)
    return ERA                        (DP0's result right now)
return ERA
```

A subsequent reduction of x1 finds `heap[dup_loc]` SUB-tagged and
picks up LAM directly without firing any new interaction.

## Cost (annihilate case)

- Two `heap_read` (sup branches)
- One `heap_set` (the substitution for the inactive side)
- Zero allocations
- One `ITRS++`
