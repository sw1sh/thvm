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
  u32  lam_ext = term_ext(lam);
  u64  loc     = term_val(lam);
  Term body    = heap_read(loc);
  if (lam_ext & LAM_ERA_MASK) {
    return body;        // (plus the JIT-materialize step, see below)
  }
  heap_subst_var(loc, arg);
  return body;
}
```

`lam.val` is the binder loc, which doubles as the cell that holds the
body and the slot that substitution writes to. The reducer dispatches
to this rule from the APP frame's apply phase
([normal_form.md](../normal_form.md)) and `goto enter`s the returned `body`.

### LAM_ERA_MASK fast-path

When the binder is unused inside the body (no `VAR(loc)` reference),
substituting `arg` would write a cell that nothing reads. The
`LAM_ERA_MASK` bit (bit 17 of the LAM's ext) records this property.
When set, the rule skips `heap_subst_var` and just returns the body.

The bit is set at construction time by `lam_body_uses_var`
([src/lam/body_uses_var.c](../../src/lam/body_uses_var.c)), called
from every site that builds a fresh LAM: `interact_dup_lam`,
`interact_app_bri`, `interact_ann_lam`, `alo_realize`,
`book/from_dynamic`, and the WL bridge's `thvm_wl_term_new` shim.
Mirrors HVM4's `parse/term/lam.c` decision logic, but applied at
runtime against the live heap rather than at parse time.

### JIT-materialize step

When the body is a `TAG_UOP` graph and not yet a `UOP_KERNEL`, the
rule routes through `thvm_materialize` to compile it into a kernel
before returning. This step is independent of the LAM_ERA fast-path
and runs in both branches.

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
