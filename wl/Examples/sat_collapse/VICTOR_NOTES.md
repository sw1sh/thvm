# Notes from Victor Taelin's "Simple SAT Solver via Superpositions"

Source gist: https://gist.github.com/VictorTaelin/9061306220929f04e7e6980f23ade615

## Key insight that flips our approach

The whole trick is that **booleans are Church-encoded LAMs, not NUMs**.
With Church bools, AND and OR are written as APP chains -- which means
SUP propagates through them via `APP-SUP` (the rule we already have)
instead of needing an `OP2-SUP` rule (which we don't).

```
T  = λt.λf.t
F  = λt.λf.f
(And a b) = (a (λx.x) (λx.F) b)     -- if a then b else F
(Or  a b) = (a (λx.T) (λx.x) b)     -- if a then T else b
(Not a)   = λt.λf.(a f t)
```

When `a = SUP^L{T, F}`, then `(a (λx.x) (λx.F) b)` triggers `APP-SUP`
which distributes the head over both SUP branches.  No `OP2-SUP` rule
needed; the existing `interact_app_sup` is enough.

## Variable encoding

Each variable gets its own SUP label:

```
x_i = SUP^A_i { T, F }
```

DUP-SUP at the same label annihilates pairwise; at different labels
commutes (cross product).  So distinct labels per var = full 2^V
enumeration; same label per var = pairwise correlation.

## Collapser tree (to extract sat assignments)

To turn a SUP-tree of T/F leaves into a Church-list of satisfying
assignments:

```
Cons h t = λc.λn.(c h (t c n))
Nil      = λc.λn.n
Join a b = λc.λn.(a c (b c n))                 -- list concat

Col_i = λx. let #A_i{x0, x1} = x; (Join x0 x1)  -- DUP^i around result
```

Wrapping order: `Col_0(Col_1(Col_2(...(Foo x_0 x_1 x_2 ...))))`.  The
outer collapser unwraps the OUTERMOST DUP layer, which corresponds to
the innermost SUP push.

Each leaf is wrapped via `Log` to emit a Cons with the assignment
when the formula reduces to `T`, or Nil when it reduces to `F`.

## Performance claims

> a random instance with 32 variables takes 3 minutes via brute-force
> in Rust but solves in 1 second in HVM

Speedup mechanism: shared sub-formulas across the 2^V branches
reduce **once** thanks to DUP, not 2^V times.  Doesn't beat CDCL
asymptotically (SAT is still NP-hard), but the constant factor /
sharing wins big when the formula has structure.

## Caveats

- **One level of duplication at a time**.  Bend docs warn: if both
  `List/map` and the inner lambda duplicate something, you get
  "destructive interference".  The collapsers are explicit DUPs; the
  variables are explicit SUPs; the formula in between should not
  itself introduce DUPs over already-SUP'd values.

- **No magic against unsatisfiable formulas**: the speedup comes
  from sharing SAT subproblems.  If every branch fails for different
  reasons, sharing has nothing to amortize against.

- **Cross-product through OP2 needs the right rule** -- which is why
  Victor's encoding goes through APP, not OP2.  Lesson: don't try
  to bolt SAT onto numeric ops; use lambda-encoded boolean algebra.

## Implications for thvm

Our `cnf_blowup.wls` repro is barking up the wrong tree -- not a
runtime bug per se but a missing rule (`OP2-SUP`).  The Victor-shaped
SAT solver doesn't hit that path because every operation is APP, and
we DO have `interact_app_sup`.

Roadmap update:
1. Rewrite `baseline.wls` using Church-encoded T/F + APP-built AND/OR.
2. Variables = `TSup[fresh_label, T, F]`.
3. First milestone: just check satisfiability by running TCollapse
   on the formula and seeing if any leaf is T.
4. Second milestone: implement the collapser tree to extract
   assignments.
5. Then bench against `SatisfiabilityInstances`.

Eventual `OP2-SUP` rule is still worth adding for general numeric
search problems (Pythagorean triples, subset sum, etc.) -- but it's
not on the critical path for SAT.
