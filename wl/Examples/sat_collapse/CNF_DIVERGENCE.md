# cnf semantics divergence vs HVM4

## TL;DR

Our Phase 1+2 made plain `TAG_DP0` / `TAG_DP1` Levy-opaque under
`wnf`, deferring DUP-XXX firing to `cnf` readback.  HVM4 does NOT
do this.  In HVM4, **plain `DP0` / `DP1` fire DUP-XXX at wnf time**;
they have a SEPARATE pair of tags `BJ0` / `BJ1` for the Levy-opaque
book-time projections.

We collapsed both roles onto our single `DP0`/`DP1`, then chose the
opaque semantics to make Lazy.wl's recursive helpers (`defPermsLex`,
`defSplits`, ...) not blow up.  That choice broke the
formula-evaluation pattern where `auto_dup` wraps each var-use in a
DP chain that needs to fire at wnf time.

## What HVM4 has

Two separate tag families for projections:

```
TinyHVM/HVM4/clang/wnf/_.c

  case DP0: case DP1: {
    u64 loc = term_val(next);
    Term cell = heap_take(loc);
    if (term_sub_get(cell)) {
      next = term_sub_set(cell, 0);   // already substituted -> follow SUB chain
      goto enter;
    }
    stack[s_pos++] = next;             // push DP frame
    next = cell;                        // descend into body
    goto enter;                         // drive body to WHNF, dispatch DUP-XXX
  }
```

So `DP0`/`DP1` are wnf-time redexes -- DUP-LAM, DUP-SUP, DUP-NUM,
DUP-CTR all fire as soon as the body reaches WHNF.

```
TinyHVM/HVM4/clang/cnf/_.c

  case BJ0:
  case BJ1: {
    return term;        // book-time projections: Levy-opaque under cnf
  }
```

`BJ0`/`BJ1` are the book-time projections, Levy-opaque under both
wnf AND cnf.  These are the ones inserted by parse-time auto-dup
into book templates, which alo_realize unfolds carefully via
`alo_dup_share` to keep recursive bodies bounded.

## What we did

`docs/plans/levy_optimal.md` Phase 1+2: made `TAG_DP0`/`TAG_DP1`
Levy-opaque under wnf, moved DUP-XXX firing to `cnf_dp` (`src/cnf/_.c`).
Phase 4 enabled `lam_seal_ext_with_auto_dup` for recursive lambdas
(dropped the REF/ALO bail).

Net: every plain `DP0`/`DP1` that appears in a dyn-heap term --
whether from auto_dup, from manual `TDup`, or from a SUP-distribution
during pattern matching -- waits for cnf to resolve.

## Why this breaks the SAT solver

The SAT formula has each variable used many times across clauses.
auto_dup wraps each use in a DP^L_internal projection.  After
APP-SUP commute distributes a SUP-typed variable through the
formula, leaves are shaped:

```
APP[..., APP[DP0[lab, APP[DP0[lab', APP[..., LAM[...]]], ...]],
              LAM[...]] ...]
```

`cnf_dp` tries to fire the matching DUP-XXX rule:

```c
Term body_cnf = cnf_at(body, depth);
switch (term_tag(body_cnf)) {
  case TAG_LAM: ...DUP-LAM...
  case TAG_NUM: ...DUP-NUM...
  ...
  default:                         // body cnfs to APP-of-DP -- bail
    heap_set(loc, body_cnf);
    return dp;
}
```

When the body cnfs to another `APP[DP, ...]` (because IT'S a deeper
DP-of-APP-of-DP chain), the default case writes it back and bails.
HVM4 never hits this because its plain DPs fire at wnf time, so by
the time you reach cnf the DPs are already DUP-XXX'd into LAMs /
NUMs / SUPs.  No "DP whose body is an APP chain that needs further
DP firing" stuck shape ever forms.

## Possible fixes (in increasing scope)

### (1) `cnf_dp` deep-drive on stuck APP-of-DP

Smallest patch.  In `cnf_dp`'s default case, before bailing, retry:

```c
default: {
  if (term_tag(body_cnf) == TAG_APP) {
    Term redriven = cnf_at(wnf(body_cnf), depth);
    if (redriven != body_cnf) {
      body_cnf = redriven;
      goto retry_dispatch;        // re-switch on the new tag
    }
  }
  heap_set(loc, body_cnf);
  return dp;
}
```

Plausible.  Doesn't address the divergence-from-HVM4 architecturally,
just makes our cnf walk deeper.  Risk: pathological terms loop.

### (2) Add a separate book-time projection tag (`TAG_BJ0`/`TAG_BJ1`)

Mirrors HVM4 directly.  Plain `TAG_DP0`/`TAG_DP1` go back to firing
DUP-XXX at wnf time.  Auto-dup's book-template inserts `TAG_BJ0`/
`TAG_BJ1` for recursive sharing; alo_realize unfolds them carefully.

Largest scope.  Requires re-doing Phase 1+2's wnf-side opacity work
and adding the new tag pair end-to-end (interactions, cnf, walkers,
wlt tests, visualization).

### (3) Eagerise `TAG_DP0`/`TAG_DP1` at wnf, restore "opaque" only
for grad-flag DPs

Middle ground: revert the Phase 1+2 "DPs opaque under wnf" choice
for plain DPs, keep grad-flag DPs as the Levy-opaque path.
auto_dup's recursive blowup needs an alternative fix -- maybe
something `alo_dup_share`-shaped at the dyn-heap level, or scoped
per-call-site.

This is the option that aligns us with HVM4 conceptually without
the new tag.  Has to be re-tested against the original Phase 1+2
motivating cases (Lazy.wl perms / splits / etc).

## Recommendation

Start with (1) -- it's local, fixable, and either resolves the SAT
solver immediately or surfaces a deeper case that motivates (2) or
(3).  If (1) loops or doesn't progress on the actual stuck shape,
escalate to (2) or (3).
