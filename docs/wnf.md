# WNF stack-machine reducer

`wnf(t)` reduces a `Term` to weak normal form: enough reduction to
expose the head constructor (LAM, ERA, SUP, ...) but no work inside
its arguments. Defined in [src/wnf/_.c](../src/wnf/_.c). Modeled on
[HVM4's clang/wnf/_.c](../TinyHVM/HVM4/clang/wnf/_.c).

## Two phases

```
enter:                          apply:
   walk left until WHNF             pop frames LIFO
   pushing eliminator               dispatch each frame's
   frames as we go                  active-pair interaction
```

The reducer alternates: enter walks down, apply walks back up,
interactions can produce a new sub-term that we re-enter, and we
finish when there are no frames left.

State is two globals:

```c
extern Term *WNF_STACK;
extern u32   WNF_S_POS;
```

We snapshot `base = WNF_S_POS` at entry so a hypothetical nested
`wnf()` call would still terminate at its own base. The runtime is
single-threaded today; nesting is moot but the pattern matches HVM4
and costs nothing.

## Enter

```c
enter:
  switch (term_tag(next)) {
    case TAG_VAR:
      // unsubstituted VAR -> WHNF
      // substituted VAR -> strip SUB, re-enter the value
    case TAG_DP0:
    case TAG_DP1:
      // unsubstituted -> push DP frame, descend into dup body
      // substituted   -> strip SUB, re-enter the value
    case TAG_APP:
      // push APP frame, descend into the function position
    case TAG_DUP:
      // walk past: read body, re-enter it
    case TAG_LAM:
    case TAG_ERA:
    case TAG_SUP:
    default:
      // already WHNF
      goto apply;
  }
```

Three eliminator tags push frames onto the stack: `APP` pushes itself,
`DP0` and `DP1` push themselves. Everything else either short-circuits
to `apply` (already WHNF) or transparently chases an indirection
(`VAR` and projections that found a SUB-tagged cell, `DUP` walking
past its carrier).

The `DP0`/`DP1` enter uses `heap_take` (read + zero) on the dup cell
because we are about to descend into it; the cell itself is treated as
a one-shot work queue. If the *other* projection later interacts and
substitutes its own value at this loc, that write will set SUB so the
second projection picks it up cleanly.

## Apply

```c
apply:
  while (s_pos > base) {
    Term frame = stack[--s_pos];
    switch (term_tag(frame)) {
      case TAG_APP: {
        // dispatch by tag of whnf:
        //   LAM -> interact_app_lam, re-enter result
        //   ERA -> interact_app_era, continue with new whnf
        //   default -> stuck, rebuild APP node
      }
      case TAG_DP0:
      case TAG_DP1: {
        // dispatch by tag of whnf:
        //   SUP -> interact_dup_sup, re-enter result
        //   ERA -> interact_dup_era, continue with new whnf
        //   default -> stuck, rebuild DP node
      }
    }
  }
  return whnf;
```

Frames pop LIFO. Each interaction either:

- **produces a new term to reduce** (`goto enter` with the result;
  used when the result is structurally new and needs head-reduction
  again, e.g. APP-LAM returning a body that may itself be reducible);
- **produces a fresh WHNF directly** (`continue` with the result as
  the new `whnf`; used when the result is a leaf or constructor that
  the next frame can dispatch against without re-entry, e.g. APP-ERA
  returning ERA);
- **gets stuck** (no interaction matches the active pair). The
  reducer puts the WHNF back into the original heap cell and treats
  the frame itself as the new WHNF. The result is a well-formed but
  unreduced node.

## Dispatch table (current)

| Frame | WHNF tag | Action                           | Source                                        |
| ----- | -------- | -------------------------------- | --------------------------------------------- |
| APP   | LAM      | `interact_app_lam`, re-enter     | [interact/app_lam.md](interact/app_lam.md)    |
| APP   | ERA      | `interact_app_era`, continue     | [interact/app_era.md](interact/app_era.md)    |
| APP   | other    | stuck, rebuild                   |                                               |
| DP0/1 | SUP      | `interact_dup_sup`, re-enter     | [interact/dup_sup.md](interact/dup_sup.md)    |
| DP0/1 | ERA      | `interact_dup_era`, continue     | [interact/dup_era.md](interact/dup_era.md)    |
| DP0/1 | other    | stuck, rebuild                   |                                               |

Adding an interaction means:

1. Write `src/interact/<active_pair>.c` defining
   `interact_<active_pair>(...)`.
2. Declare it in [src/thvm.h](../src/thvm.h).
3. Include it from [src/thvm.c](../src/thvm.c).
4. Add a case to the relevant frame's dispatch in
   [src/wnf/_.c](../src/wnf/_.c).
5. Add or extend a test under [tests/](../tests/).
6. Document the rule under [docs/interact/](interact/).

## Tracing

`ITRS` is a global counter. Each `interact_*` increments it once
when its rule fires. Tests assert the delta to verify the right rule
ran (and only ran the expected number of times).

```c
extern u64 ITRS;   // src/thvm.h
ITRS++;            // each interact_*.c after argument validation
```

A future tracing facility (planned for step 15) will piggyback on
this counter to snapshot the heap after each step.
