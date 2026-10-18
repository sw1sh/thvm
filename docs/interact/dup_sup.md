# DUP-SUP

Source: [src/interact/dup_sup.c](../../src/interact/dup_sup.c).
Tested by [tests/test_dup_sup.c](../../tests/test_dup_sup.c) and a
VerificationTest in [wl/THVMLink/Tests/core.wlt](../../wl/THVMLink/Tests/core.wlt).

## Rule

Two cases, dispatched by comparing the dup label and the sup label.

### Same label (annihilate)

```
! &L{x0, x1} = &L{a, b}
----------------------- DUP-SUP (L == L)
x0 <- a
x1 <- b
```

The dup and the sup were "siblings" sharing a label; they cancel.
Each projection takes the corresponding branch of the SUP.

### Different label (commute)

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

## Implementation

```c
fn Term interact_dup_sup(u32 lab, u64 loc, u8 side, Term sup) {
  u64 sup_loc = term_val(sup);
  u32 sup_lab = term_ext(sup);
  ITRS++;
  if (lab == sup_lab) {
    Term tm0 = heap_read(sup_loc + 0);
    Term tm1 = heap_read(sup_loc + 1);
    return heap_subst_cop(side, loc, tm0, tm1);
  }
  Term a = heap_read(sup_loc + 0);
  Term b = heap_read(sup_loc + 1);
  u64 c = heap_alloc(6);
  heap_set(c + 0, a);                                // ! &L{A0, A1} = a
  heap_set(c + 1, b);                                // ! &L{B0, B1} = b
  heap_set(c + 2, term_new(0, TAG_DP0, lab, c + 0));
  heap_set(c + 3, term_new(0, TAG_DP0, lab, c + 1));
  heap_set(c + 4, term_new(0, TAG_DP1, lab, c + 0));
  heap_set(c + 5, term_new(0, TAG_DP1, lab, c + 1));
  Term x0 = term_new(0, TAG_SUP, sup_lab, c + 2);    // &R{A0, B0}
  Term x1 = term_new(0, TAG_SUP, sup_lab, c + 4);    // &R{A1, B1}
  return heap_subst_cop(side, loc, x0, x1);
}
```

(The source also emits a multicomputation trace event per case.)

`side` is 0 for DP0, 1 for DP1; the active projection (whichever one
the reducer popped from the stack) gets its result returned and the
other one is installed at the dup loc with SUB=1 via
`heap_subst_cop` (see [heap.md](../heap.md)).

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

## Cost

Annihilate:

- Two `heap_read` (sup branches)
- One `heap_set` (the substitution for the inactive side)
- Zero allocations
- One `ITRS++`

Commute: one 6-cell allocation, two `heap_read`, six `heap_set` and the
substitution, one `ITRS++`.
