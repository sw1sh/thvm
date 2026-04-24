# APP-ERA

Source: [src/interact/app_era.c](../../src/interact/app_era.c).
Tested by [tests/test_era.c](../../tests/test_era.c) and a
VerificationTest in [wl/THVMLink/Tests/core.wlt](../../wl/THVMLink/Tests/core.wlt).

## Rule

```
(* arg)
------- APP-ERA
*
```

The eraser absorbs anything applied to it, including its argument.

## Implementation

```c
fn Term interact_app_era(void) {
  ITRS++;
  return term_new(0, TAG_ERA, 0, 0);
}
```

The `arg` term is left in the heap; it is dangling but unreachable
from any live root, which is fine in an interaction-net runtime
without GC. (In a multithreaded, long-running setting we would want
to either pair the arg with another ERA or run a periodic compaction
pass; today's single-test runtime does neither.)

## Cost

- Zero `heap_read` / `heap_set` calls
- Zero allocations (the returned ERA is a leaf, no heap cell)
- One `ITRS++`
