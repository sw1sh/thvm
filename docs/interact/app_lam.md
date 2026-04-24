# APP-LAM (beta reduction)

Source: [src/interact/app_lam.c](../../src/interact/app_lam.c).
Tested by [tests/test_app_lam.c](../../tests/test_app_lam.c) and
several VerificationTests in [wl/THVMLink/Tests/core.wlt](../../wl/THVMLink/Tests/core.wlt).

## Rule

```
(lam x.body) arg
---------------- APP-LAM
x <- arg
body
```

Apply a lambda to an argument: substitute the argument at the
binder's heap cell, return the body to be reduced next.

## Implementation

```c
fn Term interact_app_lam(Term lam, Term arg) {
  ITRS++;
  u64  loc  = term_val(lam);
  Term body = heap_read(loc);
  heap_subst_var(loc, arg);
  return body;
}
```

`lam.val` is the binder loc, which doubles as the cell that holds the
body and the slot that substitution writes to. The reducer dispatches
to this rule from the APP frame's apply phase
([wnf.md](../wnf.md)) and `goto enter`s the returned `body`.

## Worked example

`(lam x. x) ERA`:

```
before:                   after interact_app_lam:
  heap[lam_loc] = VAR       heap[lam_loc] = ERA with SUB=1
                            return value  = VAR(val=lam_loc)
```

The reducer re-enters `VAR(val=lam_loc)`. `heap_read(lam_loc)` gives
the SUB-tagged ERA; the VAR enter rule strips SUB, sets `next = ERA`,
and re-enters. ERA is WHNF; the result is ERA. One interaction
counted.

## Cost

- One `heap_read`
- One `heap_set` (via `heap_subst_var`)
- Zero allocations
- One `ITRS++`
