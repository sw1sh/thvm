# DUP-ERA

Source: [src/interact/dup_era.c](../../src/interact/dup_era.c).
Tested by [tests/test_era.c](../../tests/test_era.c).

## Rule

```
! &L{x0, x1} = *
---------------- DUP-ERA
x0 <- *
x1 <- *
```

Both projections receive the eraser. There is nothing to clone: the
eraser is its own duplicate.

## Implementation

```c
fn Term interact_dup_era(u8 side, u64 loc, Term era) {
  ITRS++;
  return heap_subst_cop(side, loc, era, era);
}
```

`heap_subst_cop` substitutes one ERA at the dup loc (for the inactive
projection to find later) and returns the other ERA (for the active
projection to continue with). Since both arguments are equal, we
could equivalently `return era` and `heap_subst_var(loc, era)`
inline; using `heap_subst_cop` keeps the DUP-rule shape uniform with
[dup_sup.md](dup_sup.md).

The label is irrelevant (both sides agree, no commute possible).

## Cost

- One `heap_set` (via `heap_subst_cop`)
- Zero allocations
- One `ITRS++`
