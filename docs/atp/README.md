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

  src/atp/_.c            -- saturation engine
  src/atp/precedence.c   -- automatic precedence
  src/atp/ft*.c          -- AtpFt: native flatterm
  src/atp/lpo_cache.c    -- LPO orient cache
  src/kbo/_.c            -- Knuth-Bendix ordering
  src/lpo/_.c            -- Lexicographic Path Order
  src/rewrite/_.c        -- match + subst-apply
  src/unify/_.c          -- unification
  src/cp/_.c             -- critical-pair enum

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
  memos, the AtpFt port, controls (build defines, C setters, env
  knobs), soundness probes, profiling.

* **[roadmap.md](roadmap.md)** — what's covered today, what's open,
  ranked by impact and difficulty.  Where the next arcs live.

* **[survey.md](survey.md)** — where thvm sits among the established
  reasoners: a peer-level UEQ prover with a complete-but-single-theory
  (EUF) SMT core.  Honest feature matrices vs SMT solvers (Z3/CVC5),
  FOL ATPs (Vampire/E), and UEQ provers (Waldmeister/Twee).

* **[unification_roadmap.md](unification_roadmap.md)** — the design arc
  toward Z3-class SMT (multi-theory, CDCL(T), quantifier instantiation)
  *unified* with the completion engine, built on the fact that
  congruence closure and completion are the same calculus.

The WL `Method` / preset / portfolio surface (and the proof-object
API a user actually calls) lives in
`wl/THVMLink/docs/Tutorials/ATP.md` and `AtpMethods.md`.  The C-level
docs in this directory describe what's *underneath* the WL surface
— the engine internals, the C-side controls, and what `Method`'s
suboptions translate to.

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
Feature-gated test binaries cover the AtpFt port + opt-in caches:

  tests/test_atp.c              -- 135624 assertions, default build
  tests/test_atp_ft_rules.c     -- AtpFt rule-storage parity probe
  tests/test_ft.c               -- AtpFt constructors + round-trip
  tests/test_ft_match.c         -- AtpFt match + subst differential
  tests/test_ft_norm.c          -- AtpFt normalize differential
  tests/test_ft_order.c         -- AtpFt LPO/KBO 100k-pair differential
  tests/test_ft_ri.c            -- AtpFt discrim-tree differential
  tests/test_ft_cpq.c           -- AtpFt CP-queue dual-store invariants
  tests/test_lpo_cache.c        -- LPO orient cache differential
  tests/test_ac.c               -- AC declarations + canonical-form flatten

The strongest soundness gate is `bin/test_atp_ft_norm_verify` —
`tests/test_atp.c` built with the full AtpFt stack plus
`THVM_ATPFT_NORM_VERIFY=1`, which runs every push-norm through
both the Term path and the AtpFt path and aborts on disagreement.

## Related docs

- `algorithms.md` — the algorithmic content (selection, redundancy, AC).
- `engineering.md` — implementation details (AtpFt, indexes, GC).
- `roadmap.md` — coverage today + open arcs.
- `vampire_port.md` — Vampire-flag → thvm-option mapping table, with
  high-value port targets surfaced from the NotableTheorems baseline.
