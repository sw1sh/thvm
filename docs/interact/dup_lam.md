# DUP-LAM

Source: [src/interact/dup_lam.c](../../src/interact/dup_lam.c).
Tested by [tests/test_dup_lam.c](../../tests/test_dup_lam.c) and a
VerificationTest in [wl/THVMLink/Tests/core.wlt](../../wl/THVMLink/Tests/core.wlt).

## Rule

```
! &L{F0, F1} = lam x.body
------------------------- DUP-LAM
F0 <- lam x0.b0
F1 <- lam x1.b1
x  <- &L{x0, x1}
! &L{b0, b1} = body
```

Cloning a lambda: produce two fresh lambdas that share the same
body via a new dup, and rebind the original variable so leftover
occurrences of `x` in the rest of the program resolve to the new
superposition `&L{x0, x1}`.  This is the rule that lets things like
Church 2 (`lam s. lam z. s (s z)`) reduce: the `s` is duplicated
before the inner application uses it twice.

## Implementation

```c
fn Term interact_dup_lam(u32 lab, u64 loc, u8 side, Term lam) {
  ITRS++;
  u64  lam_loc = term_val(lam);
  Term body    = heap_read(lam_loc);

  u64 a = heap_alloc(5);
  heap_set(a + 4, body);
  heap_set(a + 0, term_new(0, TAG_DP0, lab, a + 4));
  heap_set(a + 1, term_new(0, TAG_DP1, lab, a + 4));
  heap_set(a + 2, term_new(0, TAG_VAR, 0,   a + 0));
  heap_set(a + 3, term_new(0, TAG_VAR, 0,   a + 1));

  Term sup = term_new(0, TAG_SUP, lab, a + 2);
  Term l0  = term_new(0, TAG_LAM, 0,   a + 0);
  Term l1  = term_new(0, TAG_LAM, 0,   a + 1);

  heap_subst_var(lam_loc, sup);
  return heap_subst_cop(side, loc, l0, l1);
}
```

One 5-cell allocation; the layout is documented inline.

## Worked example

`!&L{F0, F1} = lam x.x` followed by `(F0 ERA)`:

1. DUP-LAM fires.  Two cloned identity lambdas exist; the original
   `x` is bound to a fresh `SUP{x0, x1}`; the original body cell
   becomes a DUP carrier.
2. `(F0 ERA)` reduces via APP-LAM, which substitutes `ERA` at F0's
   binder loc.
3. Resolving F0's body chases through DP0 of the dup body (which
   holds `VAR(original-binder)`).  The original binder now holds
   the SUP, so DP0 of SUP fires (annihilating same-label) and yields
   `VAR(x0)` at the new LAM0's binder.
4. That VAR resolves via the substitution from step 2 to ERA.

Net effect: `(F0 ERA) -> ERA`, exactly as if F0 were the original
identity lambda.  Tested by
[tests/test_dup_lam.c](../../tests/test_dup_lam.c) and the WL
"Church 2 applied to id and ERA reduces to ERA" verification test.

## Cost

- One `heap_alloc(5)`
- Five `heap_set` for the new cells
- One `heap_set` (via `heap_subst_var`) for the original binder
- One `heap_set` (via `heap_subst_cop`) for the inactive projection
- Zero recursion: the body itself is not cloned, only wrapped in a
  new DUP that the future projections will descend into lazily.
- One `ITRS++`.

## Why this is the right shape

Naive lambda cloning would deep-copy the body, which is exponential
in the worst case.  The IC trick is to share the body via a new dup
and let further reductions of `F0` / `F1` clone only the parts of
the body that they actually inspect.  This laziness is what gives
Lamping / Mazza / HVM their optimal-reduction property.
