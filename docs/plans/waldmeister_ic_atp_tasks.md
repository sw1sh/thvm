# IC-native ATP autonomous task queue

Goal: from stages 1-4 done (commits 940619b through ee6fb65) to a
full-fledged IC-native equational ATP that can prove small TPTP-UEQ
problems competitively with Twee.

Design memo: [waldmeister_ic_atp.md](waldmeister_ic_atp.md).
Glossary (HVM-SUP vs ATP-superposition, paramodulation, CP,
bisubstitution): [../glossary.md](../glossary.md).

## Process (mirrors `/Users/swish/src/thvm/TASKS.md`)

Pick the topmost `[ ]` item.  If it's too big for one cron firing
(rough cap: ~100 LOC, ≤30 min of work), replace it with smaller
`[ ]` sub-items, commit the decomposition, and exit.  Otherwise
implement it, verify (`make test` + `make wl-test`), commit,
mark `[x]`.

If you genuinely fail an item 3 times in a row (consecutive cron
fires on the same item), mark it `[blocked: <one-line reason>]`
and pick the next item.  Blocked items are skipped, not deleted.

Keep commits atomic.  Do not delete entries -- only flip state.

**Do not touch [TASKS.md](../../TASKS.md)** -- that's the
memory/kernelization arc, owned by a different cron lineage.

## Stage 0 -- sanity

- [x] confirm baseline: `make test` (48 C executables, 166
      sub-checks) and `make wl-test` (295 WL VerificationTests)
      both green at HEAD `f49f267`.

## Stage 5 -- saturation loop driven by collapse

Plan sec.5.5: outer loop normalizes the goal; if not closed, expand
`R` with one CP from priority collapse; repeat.  Fairness via
`TAG_INC`-priority + step cap.

- [x] 5.0 sketch the saturation state machine in
      [`docs/plans/saturation_loop.md`](saturation_loop.md): full
      step algorithm, AtpState struct, fairness story, termination
      conditions, mapping from C-side primitives, open questions.
- [x] 5.1 `AtpState` struct: rules `R`, CP queue, goal, KboConfig,
      step counter; `thvm_atp_init` / `_free` / `_add_equation` /
      `_set_goal` helpers in `src/atp/_.c`.  AtpStatus enum +
      ATP_MAX_RULES (256) + ATP_MAX_CPS (4096) public in
      `src/thvm.h`.  Tests in `tests/test_atp.c` cover init/free,
      queue overflow, goal set/clear.
- [x] 5.2a `thvm_atp_select_cp`: FIFO pop in `src/atp/_.c`; shifts
      tail down to keep the array dense; returns 0 on empty queue,
      1 on success with out-params populated.  Tests in
      `tests/test_atp.c` cover empty pop, three-element FIFO order,
      and post-pop densification.  5.3 upgrades to INC-priority.
- [x] 5.2b `thvm_atp_orient_and_add`: KBO + atomic push to R via
      `AtpAddedRange { first, count }`.  GT/LT -> count=1 (LT
      swaps); UN -> count=2 (unfailing fallback, atomic on
      capacity); EQ or full -> count=0.  Tests in
      `tests/test_atp.c` cover all four branches with the standard
      group-axiom KBO config.
- [x] 5.2d `thvm_atp_generate_cps`: enumerate (new x all_R) +
      (old x new) via new `thvm_critical_pairs_range` (added to
      `src/cp/_.c`); push survivors onto the CP queue.  Temp
      buffer ATP_CP_BATCH (1024 CPs); drops overflow silently.
      Tests cover empty-added no-op, single-rule self-overlap, and
      old-times-new with assoc + left-id.
- [x] 5.2c `thvm_atp_interreduce`: walks R[0..added.first), copies
      the new rules' Terms by value, normalizes each old rule's LHS
      under those new rules; if it reduces, drops the old rule
      (compacts the array) and requeues `(reduced, old_rhs)` onto
      the CP queue.  Top-only rewriting today; 5.4's recursive
      descent will widen coverage automatically.  Tests cover the
      empty-added no-op, drop-on-specialization, keep-on-irreducible,
      and the no-old-rules edge case (added.first == 0 underflow guard).
- [x] 5.2e `thvm_atp_goal_check`: normalizes both goal sides under
      R via `thvm_rewrite_normalize` (NORM_CAP=64), returns
      ATP_PROVED on kbo_eq hit / ATP_RUNNING otherwise.  Skips when
      goal_lhs == 0 (completion mode).  Top-only today; 5.4
      widens.  Tests cover no-goal, trivial e==e, close-under-one-
      rule, and doesn't-close.
- [x] 5.2f `thvm_atp_step` + `thvm_atp_run` glue 5.2a..5.2e plus
      normalize/trivialize into the full saturation step.  Order:
      goal_check -> step_cap -> select_cp -> normalize ->
      trivialize -> orient_and_add -> interreduce (with
      post-interreduce range adjustment so 5.2d targets the
      correct slots) -> generate_cps -> goal_check.  Tests cover
      empty-queue queue-empty, trivial-goal-proves, step_cap=0
      timeout, the headline one-step prove via `thvm_atp_run`
      (`f(a, e) == a` under axiom `f(x, e) = x`), and completion-
      mode saturation that returns QUEUE_EMPTY.
- [x] 5.3 `thvm_atp_select_cp` now wraps each CP as
      `INC^k(CTR_label=idx [lhs, rhs])` with k = symbol_count(lhs)
      + symbol_count(rhs) (the `--add` heuristic), folds them into
      a SUP tree, and runs `thvm_collapse_ordered` to enumerate
      cheapest-first.  The CTR label decodes back to the original
      queue index for popping.  Singleton case skips the SUP/INC
      plumbing.  FIFO test in `tests/test_atp.c` upgraded to
      `select-cp-priority-order` confirming l2/r2 (k=2) -> l3/r3
      (k=2) -> l1/r1 (k=4) order.
- [x] 5.4 `thvm_rewrite_step` now does outermost-leftmost recursive
      descent: try the top first; on no top-match, descend into
      TAG_CTR children left-to-right, recurse, return on the first
      sub-rewrite.  One step still fires exactly one redex.
      `thvm_rewrite_normalize` now reaches sub-positions through
      successive calls; 5.2c interreduce + 5.2e goal_check + 5.2f
      step driver inherit the wider coverage automatically.  Tests:
      `tests/test_rewrite.c` adds 3 new cases (subterm fires,
      multi-level descent, top-tried-before-children precedence).
- [x] 5.5 demo lands in `tests/test_atp.c` as
      `atp/headline-prove-f-a-ia-equals-e-from-group-axioms`:
      under the standard group-axiom KBO config, `thvm_atp_run`
      proves `f(a, i(a)) == e` from {right-id, right-inv, assoc}
      in <= 20 saturation steps.  Stage 5 complete.

## Stage 6 -- proof trace + Waldmeister parser

- [x] 6.1a TraceEntry shape lives as TAG_CTR(label = reason,
      children = [NUM(parent_a), NUM(parent_b), lhs, rhs]).  Public
      constants `TRACE_AXIOM/ORIENT/CP`, `ATP_TRACE_NONE`,
      `ATP_MAX_TRACE = 4096` in `src/thvm.h`.  AtpState gains
      `Term trace[ATP_MAX_TRACE]` + `u32 n_trace` (zero-init via
      calloc).  Internal `atp_trace_push(s, reason, p_a, p_b, lhs,
      rhs) -> u32` (returns entry index or ATP_TRACE_NONE on
      overflow).  Tests in `tests/test_atp.c` decode an
      AXIOM-shape entry, an ORIENT-with-parent entry, and verify
      the overflow-returns-NONE behavior.
- [x] 6.1b `thvm_atp_add_equation` now records TRACE_AXIOM and
      stashes the index in `cp_trace[]`.  `thvm_atp_select_cp`
      shifts `cp_trace[]` in lockstep with `cp_lhs/rhs` and
      stashes the popped trace index in `s->last_popped_trace`.
      `thvm_atp_step` reads `last_popped_trace` after select and,
      after a successful orient_and_add, pushes one TRACE_ORIENT
      entry per added rule (handles unfailing 2-way) with
      `parent_a` = source CP's trace.  Tests verify axiom is
      recorded on add_equation and orient parent threading on a
      single step.
- [x] 6.1c `thvm_atp_generate_cps` rewritten to enumerate
      `(i, j)` pairs explicitly so each emitted CP knows its
      parent rules.  AtpState gains `u32 r_trace[ATP_MAX_RULES]`
      tracking the orient trace index per rule (init'd to
      ATP_TRACE_NONE; populated in `atp_step` after orient;
      shifted in `interreduce`).  New helper `atp_push_cps_traced`
      pushes each CP with `TRACE_CP(parent_a = r_trace[i],
      parent_b = r_trace[j])`.  Tests verify a CP entry's parents
      both point at the orient trace index of the rule that
      birthed it (self-overlap case: parent_a == parent_b == 1).
- [x] 6.1d sibling test `atp/headline-trace-shape-and-walk-to-axiom`
      runs the same group-axiom prove, asserts exactly 3
      TRACE_AXIOM entries + >= 1 TRACE_ORIENT, and walks
      `parent_a` from the latest TRACE_ORIENT back through the
      trace until it hits a TRACE_AXIOM.  Hops are capped at 100
      to defend against parent-pointer corruption.
- [x] 6.2 `thvm_atp_trace_serialize(s, buf, cap)` walks `trace[]`
      and emits PCL-shaped lines:
      `<idx> (<reason> [from <p_a>[, <p_b>]]): <lhs> = <rhs>`.
      Internal `atp_pretty_term` renders TAG_CTR as
      `C<lab>(args...)`, TAG_FVR as `x_<id>`, TAG_NUM as `#<v>`,
      TAG_ERA as `ERA`, fallback `?T<tag>`.  Truncates silently
      on buffer overflow.  Tests cover empty trace, single axiom
      (label rendering), CTR-with-args + FVR rendering, post-step
      trace with `from N` parent annotations, and small-buffer
      truncation null-termination.
- [x] 6.3a `WaldSpec` data model lands in `src/thvm.h` plus
      `src/wald/_.c` with `wald_init` / `wald_free`.  Holds the
      parsed signature (`WaldSym[64]` with monotonically-assigned
      CTR labels starting at 1), variable table (`WaldVar[32]`
      with sequential FVR ids), equations[64] parallel arrays, and
      a single `(goal_lhs, goal_rhs)` for proof-mode CONCLUSION.
      `mode_proof` defaults to 1.  Tests cover init defaults,
      NULL-safe free, and cap sanity.
- [x] 6.3b lexer: `WaldLex` cursor + `wald_lex_next(lex)`.
      Skips whitespace and `%`-to-EOL comments; recognizes idents
      (`[A-Za-z_][A-Za-z0-9_]*`, truncated to `NAME_LEN - 1`),
      `:`, `->`, `=`, `(`, `)`, `,`, `>`.  Returns `WT_END` at
      EOF, `WT_ERR` on unknown chars.  Tests cover empty / whitespace-
      only, `%` comment skipping, ident with digits + underscore,
      `->` vs bare `-`, full punctuation set, an `f(x, e) = x`
      stream, long-ident truncation, and the error path.
- [x] 6.3c1 `WaldSection` enum (WSEC_NONE / NAME / MODE / SORTS
      / SIGNATURE / VARIABLES / ORDERING / EQUATIONS /
      CONCLUSION) lands in `src/thvm.h`; `wald_section_from_ident`
      compares against the known keyword set (case-sensitive,
      unknown -> WSEC_NONE).  Lexer gains a 1-token peek
      (`have_peek` + `peeked_kind` + `peeked_text`) wired
      through `wald_lex_next` so `wald_lex_peek` -> next returns
      the same token.  Shared `wald_skip_to_section(lex)` eats
      tokens until the next section keyword (or returns
      WSEC_NONE on EOF).  Tests cover all eight keywords + the
      unknown / case-sensitivity / empty-string cases for
      section_from_ident, peek-then-next consistency, peek at
      EOF survives multiple calls, skip_to_section finds the
      keyword and leaves the lexer positioned past it, and the
      EOF / empty-source paths.
- [ ] 6.3c2 NAME / MODE / SORTS section parsers: each reads
      its single-line content (one ident for NAME, "PROOF"
      vs "COMPLETION" for MODE, ident list for SORTS) and
      returns the next section keyword via the shared
      `wald_skip_to_section` helper.  Tests drive each
      parser with a fixture string that ends in a known
      section keyword.
- [ ] 6.3c3 SIGNATURE section parser: per entry
      `name : arg_sorts -> result_sort` register the symbol
      in `spec->symbols[]` with a fresh CTR label and arity =
      number of arg sorts.  Stops at the next section keyword.
- [ ] 6.3c4 VARIABLES section parser:
      `name1, name2, ... : sort` registers each name with a
      sequential FVR id.  Stops at the next section keyword.
- [ ] 6.3c5 ORDERING section parser: detects KBO vs LPO from
      the first ident; KBO reads the comma-separated
      `name=weight` list followed by a precedence chain
      `f1 > f2 > ...`; LPO reads the precedence chain only.
      Records the precedence rank (lower index = lower in
      the order) on each symbol.  Stops at the next section
      keyword.
- [ ] 6.3d term parser: `wald_parse_term(spec, lex) -> Term`.
      Parses `ident` (lookup in var table -> FVR; else
      lookup in signature -> CTR with arity 0) and
      `ident(t1, t2, ...)` (CTR with the args; arity must
      match the signature).  Recursive-descent; no operator
      precedence (FOL terms are flat).
- [ ] 6.3e EQUATIONS / CONCLUSION parsers: each section is a
      sequence of `term = term` pairs.  Push parsed pairs into
      `spec->eqn_lhs/rhs[]` (axioms) or `spec->goal_lhs/rhs`
      (single conclusion for proof mode).  Reject multiple
      conclusions for now (8.x can revisit).
- [ ] 6.3f top-level driver `wald_parse(src, spec) -> 0/err`
      that orchestrates the sections in fixed order, returns
      a structured error code on syntax failure (not crashing).
      Error codes enum'd in `WaldErr`.
- [ ] 6.3g unit tests: parse the group example from
      `waldmeister/documents/example.pr` (or a hand-written
      copy), verify n_eqns == 3, goal lhs/rhs structure, and
      that the parsed terms can be passed straight to the
      saturation engine + KBO comparator.
- [ ] 6.4 end-to-end: parse the group-axiom `.pr` file, run
      saturation, emit a PCL trace; cross-check structurally
      against Waldmeister's own output

## Stage 7 -- Twee-class redundancy criteria (optional)

- [ ] 7.1 ground joinability test (Waldmeister's `GZ_ACVerzichtbar`
      / `Grundzusammenfuehrung.c` -- "ground union" criterion that
      drops trivially-joinable CPs)
- [ ] 7.2 connectedness redundancy (Bachmair-Dershowitz-Plaisted)
- [ ] 7.3 subsumption pruning on R and the CP queue
- [ ] 7.4 benchmark vs Twee on a handful of small TPTP-UEQ problems
      (`GRP`, `RNG` divisions); record wall-clock + saturation
      step count in `docs/bench-atp.md`

## Stage 8+ -- full-fledged ATP iteration

- [ ] 8.1 SUP-encoded CP enumeration via `TAG_PRI` unify (deferred
      4.5 -- moves CP gen from C-side to native IC reduction)
- [ ] 8.2 KBO as a pure IC program (deferred 2.4 -- compile from
      the C version once it's stable)
- [ ] 8.3 IC-native rule dispatch: closed-form rule = LAM-binder,
      APP-SUP fan-out across the rule set; uses ICC primitives
      (TAG_BRI / TAG_ANN) where dependent typing helps
- [ ] 8.4 multi-sort signatures (stages 1-4 assume one sort)
- [ ] 8.5 LPO ordering as alternative to KBO (Waldmeister has both
      -- LPO is `Lexikografische-Pfad-Ordnung`, "lexicographic
      path ordering")
- [ ] 8.6 unordered SUP/DUP for O(1) rule-set sharing (port from
      HVM4 when upstream lands them)
- [ ] 8.7 WL bridge: `TATP[axioms, conjecture] -> proof tree` so
      the prover is reachable from notebooks
- [ ] 8.8 `--mix` heuristic (size + orientability) replacing the
      pure-size `--add` in 5.3
- [ ] 8.9 narrowing for existential goals (Waldmeister's
      `NormaleZiele.c` / `Zielverwaltung.c`)
- [ ] 8.10 SupGen-style search inside saturation: superpose the
      "which CP to pick next" choice, collapse with priority,
      compare against the explicit-queue baseline
