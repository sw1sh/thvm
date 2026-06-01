# thvm/atp — Equational Theorem Proving

`src/atp/` is thvm's equational theorem prover. Given a set of equational
axioms and a goal equation, it attempts a proof by Knuth-Bendix
unfailing completion: orient each axiom into a rewrite rule (by a
reduction ordering), generate critical pairs by superposition, normalize
them, and check whether the goal's two sides reach a common normal
form.

The engine lives in `src/atp/_.c` (the saturation loop, rule storage,
CP queue, indexes, memos) with the supporting reduction orderings,
matcher, unifier, and rewriter in sibling modules:

  /Users/swish/src/thvm/src/atp/_.c            -- saturation engine
  /Users/swish/src/thvm/src/atp/precedence.c   -- automatic precedence
  /Users/swish/src/thvm/src/atp/ft*.c          -- AtpFt: native flatterm
  /Users/swish/src/thvm/src/atp/lpo_cache.c    -- LPO orient cache
  /Users/swish/src/thvm/src/kbo/_.c            -- Knuth-Bendix ordering
  /Users/swish/src/thvm/src/lpo/_.c            -- Lexicographic Path Order
  /Users/swish/src/thvm/src/rewrite/_.c        -- match + subst-apply
  /Users/swish/src/thvm/src/unify/_.c          -- unification
  /Users/swish/src/thvm/src/cp/_.c             -- critical-pair enum

The Term representation inside the engine is heap-cell `Term` (a 64-bit
packed pointer into thvm's IC heap, shared with everything else in
thvm). An optional parallel representation — `AtpFt`, modeled on
Waldmeister's `TermzellenT` — lives in `src/atp/ft*.c` behind env
gates; see [engineering.md](engineering.md) for its layout and
interactions.

## Reading order

* **[algorithms.md](algorithms.md)** — the math the engine implements:
  equational completion, reduction orderings, matching/unification,
  critical pairs, normalization, redundancy criteria.

* **[engineering.md](engineering.md)** — how the algorithms map to
  data structures: rule storage, CP queue, discrimination trees,
  memos, the AtpFt port, env gates, soundness probes.

## Entry points

```c
AtpState *s = thvm_atp_init(kbo_cfg, /*step_cap=*/100000);
thvm_atp_set_lpo(s, lpo_cfg);          // optional: LPO mode
thvm_atp_add_equation(s, lhs, rhs);    // axioms
thvm_atp_set_goal(s, gl, gr);          // proof goal (single equation)

while (thvm_atp_step(s) == ATP_RUNNING) { /* ... */ }
// terminal status: ATP_PROVED, ATP_DISPROVED, ATP_SATURATED, ATP_BUDGET
```

The driver is `thvm_atp_step` in `src/atp/_.c:7184`. It pops the
top-priority CP, normalizes both sides, joins or commits-as-rule,
then runs interreduction. See [algorithms.md §Saturation
loop](algorithms.md#saturation-loop) for the contract.

## Test suite

`tests/test_atp.c` is the regression baseline (135624 assertions).
Stage-specific test binaries cover the AtpFt port:

  tests/test_atp.c              -- 135624 assertions, default build
  tests/test_atp_ft_rules.c     -- AtpFt rule-storage parity probe
  tests/test_ft.c               -- AtpFt constructors + round-trip
  tests/test_ft_match.c         -- AtpFt match + subst differential
  tests/test_ft_norm.c          -- AtpFt normalize differential
  tests/test_ft_order.c         -- AtpFt LPO/KBO 100k-pair differential
  tests/test_ft_ri.c            -- AtpFt discrim-tree differential
  tests/test_ft_cpq.c           -- AtpFt CP-queue dual-store invariants
  tests/test_lpo_cache.c        -- LPO orient cache differential
