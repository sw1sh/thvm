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
- [x] 6.3c2 `wald_parse_name` / `wald_parse_mode` /
      `wald_parse_sorts` land in `src/wald/_.c`.  Each peeks for
      an immediate section keyword (empty section), otherwise
      consumes its content (one ident for NAME copied into
      `spec->name`; one ident for MODE setting `mode_proof = 0`
      iff "COMPLETION", else 1; ident list for SORTS, consumed
      and discarded).  Internal helper
      `wald_consume_if_section` factors the empty-section check.
      Tests drive each parser with a fixture, verify the right
      next-section enum is returned, and that the lexer is
      positioned past the keyword for downstream parsers.
- [x] 6.3c3 `wald_parse_signature` lands in `src/wald/_.c`.  Per
      entry parses `name : arg_sort1 ... argN -> result_sort`,
      registers `WaldSym{ name, label = next_label++, arity = N }`
      into `spec->symbols[]`.  Result sort consumed and discarded
      (homogeneous-signature assumption).  Stops at the next
      section keyword via the same peek-then-`wald_skip_to_section`
      pattern as 6.3c2.  Tests cover single zero-arity entry,
      three entries with monotonic labels (e=1, i=2, f=3), the
      empty-section path, and the truncated-mid-entry path
      (returns WSEC_NONE without committing the half-parsed entry).
- [x] 6.3c4 `wald_parse_variables` lands in `src/wald/_.c`.  Per
      var-decl `name1 [, name2 ...] : sort_ident` registers each
      name into `spec->vars[]` with a sequential FVR id; sort
      names are consumed and discarded.  Multi-decl sections
      (multiple `name : sort` groups) accumulate var ids
      monotonically.  Stops at the next section keyword via the
      same peek-then-`wald_skip_to_section` pattern.  Tests cover
      three-name single decl, empty section, multi-decl, and
      truncated-mid-list EOF (keeps the names registered up to
      the EOF, returns WSEC_NONE).
- [x] 6.3c5 `wald_parse_ordering` lands in `src/wald/_.c` plus a
      `prec_rank` field on `WaldSym`.  Reads everything as a
      token stream tracking the most recently seen ident; on
      `>` the previous ident becomes the next chain entry.
      KBO weight-lists are consumed and discarded (`=` resets
      the pending tracker); the saturation engine's KboConfig
      stays caller-supplied.  After the chain, ranks are
      assigned `pchain_n - 1 - i` so chain[0] is greatest.
      Tests cover LPO chain, KBO weights+chain (same ranks as
      LPO since weights are dropped), empty section, and the
      "lone ident no chain" path (no `>` -> no chain entry,
      ranks stay 0).
- [x] 6.3d `wald_parse_term(spec, lex) -> Term` lands in
      `src/wald/_.c`.  Recursive-descent: ident is a variable
      iff it's in `spec->vars[]` (returns
      `term_new_fvr(var_id)`); otherwise it's a signature symbol
      (zero-arity if no `(` follows; `arity` enforced against
      the signature on application).  Args parsed via comma
      separation; `REWRITE_MAX_ARITY` cap.  Returns 0 on any
      error (unknown ident, missing `)`, arity mismatch,
      constant-with-args).  Tests cover var lookup,
      zero-arity constant, two-arg application, nested
      `f(i(x), e)`, unknown ident, arity mismatch (`i(x, y)`
      with arity-1 i), and constant-with-args (`e(x)`).
- [x] 6.3e `wald_parse_equations` / `wald_parse_conclusion` land
      in `src/wald/_.c`.  Both share an internal
      `wald_parse_equation_pair` helper that reads
      `term "=" term` and returns the pair.  EQUATIONS appends
      each pair to `spec->eqn_lhs/rhs[]` (capped by
      `WALD_MAX_EQNS`).  CONCLUSION stores only the FIRST pair
      in `goal_lhs/rhs`; subsequent pairs are parsed (so the
      section terminates) but discarded.  Tests cover three-
      axiom EQUATIONS with cross-check on the first pair's
      structure, empty EQUATIONS, single CONCLUSION, and the
      reject-multiple behavior.
- [x] 6.3f `wald_parse(src, spec) -> WaldErr` lands in
      `src/wald/_.c`.  Lexes the source, finds the first section
      keyword via `wald_skip_to_section`, then dispatches each
      section to its parser; each parser returns the next
      section's enum so the loop just chains.  Sections are
      accepted in any order (Waldmeister's parser is permissive
      and our test fixtures appreciate that).  `WaldErr` enum:
      `WALD_OK`, `WALD_ERR_NULL`, `WALD_ERR_NO_SECTION`.  Tests
      cover NULL args, empty source, all-junk source, and the
      full group-axiom `.pr` file (spec identity + 4 symbols +
      precedence ranks + 3 vars + 3 axioms + goal lhs/rhs).
- [x] 6.3g unit tests: parse the group example from
      `waldmeister/documents/example.pr` (or a hand-written
      copy), verify n_eqns == 3, goal lhs/rhs structure, and
      that the parsed terms can be passed straight to the
      saturation engine + KBO comparator.
- [x] 6.4 end-to-end: parse the group-axiom `.pr` file, run
      saturation, emit a PCL trace; cross-check structurally
      against Waldmeister's own output
  - [x] 6.4a `wald_parse_file(path, spec) -> WaldErr` -- thin
        wrapper that opens the file, slurps it, calls
        `wald_parse` on the bytes.  New error code
        `WALD_ERR_FILE` for open/read failure.  Test: parses
        `waldmeister/documents/example.pr` from disk; if the
        symlink is missing the test silently passes (research
        fixture, not a regression).
  - [x] 6.4b end-to-end PCL emission test: load example.pr
        from disk, build KboConfig from parsed precedences,
        push axioms, set goal, run saturation, call
        `thvm_atp_trace_serialize`.  Validate the trace
        text-structurally: n_trace >= n_eqns; the first
        n_eqns lines all start with "<idx> (axiom):"; later
        lines contain "orient" or "cp" entries; the very
        last entry's lhs/rhs match the parsed conclusion
        (when ATP_PROVED).
  - [x] 6.4c structural cross-check vs Waldmeister's PCL:
        compare our axiom ordering, orient/cp parent-pointer
        shape, and final-step rhs against what Waldmeister
        would emit on the same input (Waldmeister's PCL has
        `id : type ( parents ) lhs = rhs` -- our format is
        the same shape modulo whitespace).  Document
        mismatches in CHANGELOG; if structural shape diverges
        materially, file a follow-up task rather than
        forcing convergence here.

## Stage 7 -- Twee-class redundancy criteria (optional)

- [x] 7.1 ground joinability test (Waldmeister's `GZ_ACVerzichtbar`
      / `Grundzusammenfuehrung.c` -- "ground union" criterion that
      drops trivially-joinable CPs)
- [x] 7.2 connectedness redundancy (Bachmair-Dershowitz-Plaisted)
  - [x] 7.2a design memo `docs/plans/connectedness_design.md`:
        survey BDP's "connected below c" criterion vs the
        trivial-joinability filter from 7.1.  Pick a concrete
        sub-criterion to implement that adds *non-overlapping*
        pruning on top of 7.1.  Candidates: (1) subsumption-
        connectedness ("one step from s lands at t under any rule
        in R"), (2) rule-pair-disjoint connectedness ("normalize
        with R \ {R_a, R_b} -- the rules that birthed the CP"),
        (3) BDP's literal "joinable using rules whose max-side is
        strictly smaller than max(s, t)".  Document why the
        chosen criterion is worth implementing given that 7.1
        already drops fully-joinable CPs.
  - [x] 7.2b implement the chosen criterion as a new helper in
        `src/atp/_.c` with stat counter + tests.  Expected
        scope: ~50 LOC implementation + ~30 LOC tests.
- [x] 7.3 subsumption pruning on R and the CP queue
  - [x] 7.3a **rule subsumption counter**: helper
        `atp_cp_subsumed_by_rules(s, lhs, rhs)` returns 1 if
        there is `(l, r) ∈ R` and substitution σ such that
        `(lhs, rhs) = (σl, σr)` (modulo symmetry).  Per the
        same domination argument as 7.2b
        (`docs/plans/connectedness_design.md`): rule-
        subsumption fires only when the rule rewrites lhs
        directly to rhs in one step, which is also caught by
        7.1's full-R normalize.  Add `n_cps_dropped_rule_subsumed`
        counter for empirical confirmation.  Expected scope:
        ~30 LOC + ~30 LOC tests.
  - [x] 7.3b **queue subsumption filter**: helper
        `atp_cp_subsumed_by_queue(s, lhs, rhs)` returns 1 if
        the candidate is an instance of an already-queued CP
        `(s', t')` -- i.e. there is σ with `(lhs, rhs) =
        (σs', σt')` (modulo symmetry).  This is genuinely
        orthogonal to 7.1 (the queue does not participate in
        normalization) so it adds real pruning.  Wire as a
        FILTER in `atp_push_cps_traced` (drops the CP) and
        bump `n_cps_dropped_queue_subsumed`.  Expected scope:
        ~40 LOC + ~50 LOC tests.
- [x] 7.4 benchmark vs Twee on a handful of small TPTP-UEQ problems
      (`GRP`, `RNG` divisions); record wall-clock + saturation
      step count in `docs/bench-atp.md`
  - [x] 7.4a `docs/bench-atp.md` skeleton: methodology section
        (which metrics, how `make` will drive the runs), the
        problem set (start with what we already have:
        `waldmeister/documents/example.pr`; list 3-5 candidate
        TPTP-UEQ files for later inclusion), and a results
        table populated with our ATP's wall-clock / step
        count / `n_cps_dropped_*` for the group example only.
        No Twee comparison yet -- 7.4d adds that.
  - [x] 7.4b small `.pr` test corpus under `tests/data/atp/`:
        hand-write `group_assoc_left_inverse.pr`,
        `monoid_left_id.pr`, `commutative_inverse.pr` (3-5
        small-but-non-trivial group-flavored conjectures).
        Each file is the `.pr` we feed our ATP; pair each with
        a `.expect` describing the expected proof outcome
        (PROVED / TIMEOUT / specific n_rules range).
  - [x] 7.4c `tests/test_bench_atp.c` (or a `make bench-atp`
        target): runs our ATP on every `tests/data/atp/*.pr`,
        records wall-clock (via `clock_gettime`) and the four
        `n_cps_dropped_*` counters into a CSV
        `build/bench-atp.csv`.  Hand-checks the output column
        types but does not enforce thresholds (this is a
        measurement, not a regression).  Append the CSV
        contents into `docs/bench-atp.md` under a "Results --
        thvm" heading.
  - [x] 7.4d Twee comparison: install Twee (Haskell, via
        `cabal install twee` or `brew install twee`), convert
        each `.pr` in `tests/data/atp/` to TPTP-UEQ format
        (small adapter -- our `.pr` is already close), run
        Twee with a matching step budget, and record its
        wall-clock + reported saturation steps in a parallel
        CSV.  Annotate `docs/bench-atp.md` with the
        comparison table.  Defer if Twee install fails on the
        local toolchain; record the failure mode for follow-up.

## Stage 8+ -- full-fledged ATP iteration

- [x] 8.1 SUP-encoded CP enumeration via `TAG_PRI` unify (deferred
      4.5 -- moves CP gen from C-side to native IC reduction)
  - [x] 8.1a design memo `docs/plans/sup_encoded_cps.md`:
        survey TinyHVM / HVM4 for `TAG_PRI` ("primitive function
        call") patterns, sketch the SUP-cross-product encoding
        for `overlap-position x rule-pair`, and analyze
        feasibility against the unordered-SUP requirement
        (8.6).  Document an explicit migration target: which
        parts of `src/cp/_.c` and `src/atp/_.c` move to IC,
        which stay in C as `TAG_PRI` callbacks.  Decide whether
        8.1 unblocks 8.10 (SupGen-style search) or vice versa.
  - [x] 8.1b add `TAG_PRI` primitive: new tag in `src/thvm.h`,
        constructor `term_new_pri`, single APP-PRI interaction
        in `src/interact/app_pri.c`.  PRI carries a function
        pointer ID (lookup table) and is "called" by APP.
        Tests: `tests/test_pri.c` -- 4-6 cases covering call
        behavior on simple data.
  - [x] 8.1c encode unification as a `TAG_PRI` callback: a
        single primitive that takes two superposed terms and
        returns either ERA (no unifier) or a substitution
        application.  Tests: hand-encode 2-3 pairs and verify
        round-trip.
  - [x] 8.1d SUP-encoded CP enumeration on a tiny example
        (single rule pair, two overlap positions): build the
        cross-product SUP, fire APP-SUP commutation, collapse
        to the surviving CPs.  Compare against the C-side
        `thvm_critical_pairs_range` output for the same
        inputs.  Tests: 3-5 cases that demonstrate parity.
    - [x] 8.1d-i APP-SUP commutation: `src/interact/app_sup.c`
          implementing `APP(&L{a, b}, x) -> &L{APP(a, x_dup_0),
          APP(b, x_dup_1)}` with the right-side DUP pattern
          for sharing the argument across the two children.
          Wire into the WNF dispatch (next to APP-LAM,
          APP-BRI).  Tests: `tests/test_app_sup.c` -- 4-6
          cases covering single-SUP, nested-SUP, ERA arg.
          (Foundational; not 8.1-specific but blocks 8.1d-ii.)
    - [x] 8.1d-ii SUP-encoded CP fan-out demo: hand-build
          `&L{(s1, t1), (s2, t2), ...}` with `prim_unify_apply`
          applied via APP-SUP fan-out + PRI saturation.
          Compare the result of `wnf` on the encoded
          expression against the manually-computed
          `thvm_unify_apply` outputs for each pair.  3-5 test
          cases that demonstrate parity at the two-position
          and one-rule-pair scope.
  - [x] 8.1e replace `thvm_atp_generate_cps` -- conditional
        on a feature flag -- with the SUP encoding.  Re-run
        the bench harness; expect comparable proof rates and
        latency within 2x of the C-side baseline.
    - [x] 8.1e-i `use_ic_cp_gen` feature flag on `AtpState`
          (u8, default 0) plus a setter.  Plumb through
          `thvm_atp_generate_cps`: when 0, current C path;
          when 1, dispatch to a stub `thvm_atp_generate_cps_ic`
          that returns the same CPs (initial impl: just calls
          the C path so the flag is observably a no-op).  Adds
          a single test verifying the flag round-trips.
          Scope: ~30 LOC.
    - [x] 8.1e-ii implement `thvm_atp_generate_cps_ic` using
          SUP+PRI for the unify step.  Keep the (i, j, position)
          enumeration in C for now -- the IC contribution is
          the per-position unify call goes through APP-PRI,
          producing the same Term results.  Tests: parity vs C
          path at the at_push_cps_traced call boundary on the
          group axioms.  Scope: ~80 LOC.
    - [x] 8.1e-iii bench analysis: re-run `make test` (which
          drives `test_bench_atp`) and `make bench-twee` with
          `use_ic_cp_gen = 1` set in the harness and compare
          latency vs C path.  Update `docs/bench-atp.md` with
          a new column showing the IC-path numbers.  Document
          whether the latency is within 2x; if not, identify
          the bottleneck and decide whether to keep IC default
          off or invest in further optimization.
- [x] 8.2 KBO as a pure IC program (deferred 2.4 -- compile from
      the C version once it's stable)
      Note: 8.2d (full pure-IC port) is `[blocked]` per design
      memo; revisit when SupGen-style search (8.10) creates a
      use case.  8.2a-c lands the actionable increment (PRI
      wrapper + pure-IC kbo_eq sliver).
  - [x] 8.2a design memo `docs/plans/kbo_ic_design.md`:
        survey the encoding choices for porting KBO to IC --
        (1) `TAG_PRI` wrapper around `thvm_kbo` (callable from
        IC code via APP-PRI; analogous to 8.1c's
        `prim_unify_apply`), (2) hybrid IC structural recursion
        + C arithmetic primitives, (3) full pure-IC port with
        Church-numeral or TAG_NUM weights.  Pick which steps
        fit in subsequent firings.  Document why "pure IC"
        matters: it lets 8.10 SupGen-style search superpose
        alternative ordering choices.  Scope: ~150-line memo.
  - [x] 8.2b register `thvm_kbo` as a TAG_PRI primitive at
        `ATP_PRIM_KBO = 2`; takes `(s, t, cfg_id)` -- where
        `cfg_id` is an index into a small process-global table
        of registered KboConfigs (since `KboConfig*` doesn't
        fit in a Term).  Returns a NUM whose value encodes
        `KBO_EQ / GT / LT / UN`.  Tests: `tests/test_kbo_pri.c`
        -- 4-6 cases covering each outcome and the
        APP-PRI-saturation roundtrip.
  - [x] 8.2c pure-IC port of `kbo_eq` (the structural-equality
        check) -- the simplest sub-routine that doesn't need
        arithmetic.  Encode as a `TAG_PRI` primitive that's
        itself implemented via IC structural recursion on the
        term pair, returning `NUM(0)` or `NUM(1)`.
  - [blocked: deferred per docs/plans/kbo_ic_design.md until SupGen-style search (8.10) creates a use case] 8.2d full pure-IC port of `thvm_kbo` (deferred until
        8.2a's design picks the encoding; likely
        research-grade and multi-firing on its own).
- [x] 8.3 IC-native rule dispatch: closed-form rule = LAM-binder,
      APP-SUP fan-out across the rule set; uses ICC primitives
      (TAG_BRI / TAG_ANN) where dependent typing helps
      Note: 8.3d (ICC integration) is `[blocked]` per design memo
      pending 8.4 sorts.  8.3a-c-e (design + PRI dispatch + SUP
      fan-out demo + flag swap + bench analysis) lands the
      actionable increment.
  - [x] 8.3a design memo `docs/plans/ic_rule_dispatch.md`:
        survey the encoding choices for "rule as LAM-binder".
        Key open question: our pattern variables are TAG_FVR
        (free-variable atoms with explicit ids), but LAM uses
        TAG_VAR (de-Bruijn-style binder slots).  Document the
        translation, the dispatch path
        (`APP(&L{r_0, r_1, ...}, t)` -> APP-SUP fan-out -> each
        branch fires APP-LAM if its pattern matches, else ERA),
        and where (if anywhere) ICC TAG_BRI / TAG_ANN actually
        help (likely: for type-directed pattern matching when
        sorts land in 8.4).  Pick the simplest viable
        increment.  Scope: ~150-line memo.
  - [x] 8.3b implement `prim_rewrite_step` (arity 3) at
        `ATP_PRIM_REWRITE_STEP = 4` per the 8.3a design memo
        Strategy B: takes `(lhs, rhs, target)`; runs
        `thvm_match(lhs, target, &subst)`; on success returns
        `thvm_subst_apply(rhs, &subst)`; on failure returns ERA.
        Registered in `atp_register_primitives`.  Tests verify
        the saturated APP-PRI chain produces the expected
        rewrite outcome on direct match, no-match, FVR-only LHS,
        and nested CTR cases.  ~50 LOC + 3-5 tests.
        (Originally specified as a LAM-binder encoding; the
        design memo reinterpreted this as "rule as a callable
        IC entity" via PRI dispatch.)
  - [x] 8.3c SUP of rules + APP-SUP fan-out demo: pre-encode
        a small rule set as `&L{rule_0_lam, rule_1_lam, ...}`,
        APP it to a target term, observe APP-SUP commutation
        producing a SUP of (rewritten or ERA) results, and
        compare against the C-side single-rule rewriting from
        `src/rewrite/_.c`.  ~50 LOC + 3-4 tests.
  - [x] 8.3d ICC TAG_BRI / TAG_ANN integration (optional;
        revisit after 8.3c lands).  If sort-checking proves
        useful at dispatch time, wrap rules in BRI/ANN and
        verify the type-flow rules let the wrong-sort branches
        collapse to ERA before APP-LAM fires.  Scope TBD; may
        roll up under 8.4 (multi-sort) instead.
  - [x] 8.3e replace `thvm_rewrite_step` under a feature flag
        (analogous to 8.1e's `use_ic_cp_gen`): when set, the
        rewrite step uses the SUP-of-LAMs dispatch.  Re-run
        bench harness; expect within 2x of the C path.
    - [x] 8.3e-i `use_ic_rewrite` feature flag on `AtpState`
          (u8, default 0) plus a setter.  Plumb through the
          AtpState-internal callers of `thvm_rewrite_normalize`
          (atp_cp_trivially_joinable, thvm_atp_step,
          thvm_atp_goal_check, thvm_atp_interreduce) via a new
          shim `atp_rewrite_normalize(s, ...)` that dispatches.
          Stub `atp_rewrite_normalize_ic` initially delegates
          to the C path so the flag is observable but inert.
          Tests verify the flag round-trips.  ~40 LOC.
    - [x] 8.3e-ii implement `thvm_rewrite_step_ic` using
          `prim_rewrite_step` via APP-PRI evaluation per
          the 8.3a memo Strategy B.  Walks rules in the same
          order as the C path; for each, builds the saturated
          PRI call and reduces via wnf.  Returns the rewritten
          term on first success, or the original term if no
          rule matches.  Tests: parity vs C path on the group
          axioms.  ~70 LOC.
    - [x] 8.3e-iii bench analysis: extend `test_bench_atp.c`
          to also toggle `use_ic_rewrite` (4 modes total: c+c,
          c+ic, ic+c, ic+ic for cp-gen and rewrite).  Or pick
          a smaller cross product (default + both-IC).  Update
          `docs/bench-atp.md` with the latency comparison;
          decide on default.
- [x] 8.4 multi-sort signatures (stages 1-4 assume one sort)
      Note: 8.4 closes; this also unblocks 8.3d (ICC TAG_BRI /
      TAG_ANN integration) per its design memo.  Future perf
      work: sort-aware KBO, early CP-pair sort precheck.
  - [x] 8.4a design memo `docs/plans/multi_sort.md`: survey
        the representation choices.  Where do sorts live --
        on `WaldSym` / `WaldVar` / a global sort table on
        `WaldSpec`?  Where does sort-checking fire -- inside
        `thvm_match` / `thvm_unify`, or as a precheck before
        them?  What's the impact on KBO (Waldmeister supports
        sort-aware KBO; can we get away without it for v0)?
        What's the impact on CP enumeration (only overlap
        pairs whose top symbols match in sort)?  Picks the
        minimum viable increment.  Scope: ~150-line memo.
  - [x] 8.4b extend `WaldSpec` with sort metadata: sort table
        (id -> name), per-symbol arg sorts + result sort,
        per-variable sort.  Update the `.pr` parser
        (`SORTS` / `SIGNATURE` / `VARIABLES` sections) to
        populate it.  Tests: parse a multi-sort `.pr` fixture
        and verify the metadata.
  - [x] 8.4c implement `wald_sort_check(spec, term)` that
        verifies a term is well-sorted against the spec.  Used
        by 8.4d / 8.4e as a precheck rather than threading
        sort logic through `thvm_match`.
  - [x] 8.4d wire sort-check into the saturation loop: reject
        equations / CPs / orient-and-add inputs that fail
        `wald_sort_check`.  Update tests to confirm the gate
        fires on a hand-constructed sort-mismatched equation.
  - [x] 8.4e add a multi-sort `.pr` fixture under
        `tests/data/atp/` (e.g. a small sorted-list fragment:
        nat, list, with cons taking a nat and a list).  Bench
        harness picks it up automatically.
- [x] 8.5 LPO ordering as alternative to KBO (Waldmeister has both
      -- LPO is `Lexikografische-Pfad-Ordnung`, "lexicographic
      path ordering")
  - [x] 8.5a design memo `docs/plans/lpo_design.md`: survey
        the LPO algorithm (Dershowitz, "Orderings for term-
        rewriting systems", 1982); pick a `LpoConfig` shape
        (likely just a precedence table -- LPO has no weights)
        and decide where it lives relative to `KboConfig`.
        Document how the existing saturation pipeline picks
        between KBO and LPO -- a config field on AtpState, an
        OrderConfig sum type, or two parallel
        thvm_atp_init variants.  Scope: ~150-line memo.
  - [x] 8.5b implement `thvm_lpo(s, t, cfg)` in `src/lpo/_.c`
        mirroring `src/kbo/_.c`'s structure: kbo_eq -> lpo_eq
        (or reuse), variable-domination, top-symbol comparison
        by precedence, lexicographic recursion.  Tests in
        `tests/test_lpo.c` cover GT / LT / EQ / UN outcomes
        on hand-constructed term pairs.  Scope: ~120 LOC + ~50
        LOC tests.
  - [x] 8.5c wire LPO into the saturation engine: extend
        AtpState with the chosen ordering selector from 8.5a;
        update `thvm_atp_orient_and_add` to dispatch between
        KBO and LPO.  Tests verify orient outcomes match
        between KBO and LPO on a fixture where both succeed,
        and differ where LPO is more discriminating.
  - [x] 8.5d add an LPO test fixture or update existing ones:
        the existing group / monoid fixtures use `ORDERING LPO`
        in their `.pr` files but the saturator currently maps
        them to KBO; switch one fixture to actually use LPO
        and verify the proof outcome (or note it as a
        regression/improvement).
- [blocked: waiting on HVM4 upstream to land unordered SUP/DUP first; tracked in docs/plans/sup_encoded_cps.md] 8.6 unordered SUP/DUP for O(1) rule-set sharing (port from
      HVM4 when upstream lands them)
- [x] 8.7 WL bridge: `TATP[axioms, conjecture] -> proof tree` so
      the prover is reachable from notebooks
  - [x] 8.7a design memo `docs/plans/waldmeister_ic_atp.md` section 7.1: pick
        the WL surface form (`TATP[{axiom_1, axiom_2, ...},
        conjecture]` returning a `ProofTree[]` association?
        a list of rewrite steps? a `Failure[...]` on TIMEOUT?),
        and the WL-expression-to-Term encoding (atoms ->
        TAG_CTR with arity 0; symbols -> TAG_CTR with their
        operands; uppercase identifiers -> TAG_FVR; `==` ->
        equation pair).  Document how to handle WL pattern
        constructs (`x_`, `Blank[]`, etc.) that don't have
        clean Term equivalents.  Scope: ~200-line memo.
  - [x] 8.7b LibraryLink helper that runs the ATP on
        pre-encoded Term inputs (skipping the WL encoder).
        New entry point `thvmlink_atp_run` in
        `wl/THVMLink/CSource/thvmlink.c`; takes axiom lhs/rhs
        Term lists + goal Terms + an LpoConfig stub; returns
        the trace serialized to a string.  Direct WL test
        with manually-encoded Terms.
  - [x] 8.7c WL-expression-to-Term encoder: the bulk of 8.7,
        walks an expression tree and produces a Term.  Handle
        atoms, symbols, FVR (probably via `Pattern[x, _]`),
        and `==` (eq-pair).  Tests with hand-built WL
        expressions of various shapes.
  - [x] 8.7d `TATP[axioms, conjecture]` WL surface form +
        return-shape pretty printer.  Wires 8.7c (encode) into
        8.7b (run) and decodes the trace back into a
        notebook-friendly `Association[]`.  Tests use the
        group axioms via WL syntax; assert the result matches
        what test_bench_atp produces on the equivalent .pr.
- [x] 8.8 `--mix` heuristic (size + orientability) replacing the
      pure-size `--add` in 5.3
- [x] 8.9 narrowing for existential goals (Waldmeister's
      `NormaleZiele.c` / `Zielverwaltung.c`)
  - [x] 8.9a design memo `docs/plans/waldmeister_ic_atp.md` section 7.2:
        survey the narrowing algorithm vs rewriting (rewrite
        applies an oriented rule via matching; narrow tries
        UNIFICATION at every position to find a witness
        substitution).  Pick an API shape: existential variables
        on `WaldSpec` (sort of, sort_witness?), or carried on
        the goal terms themselves via designated FVR ids.
        Decide where the witness output lives (new
        `AtpState.witness_subst[REWRITE_MAX_VAR]` field, exposed
        via `thvm_atp_get_witness`).  Document the saturation-
        loop divergence from rewriting (goal_check becomes
        narrow_check).  Scope: ~150-line memo.
  - [x] 8.9b implement `thvm_atp_narrow_step(s, lhs, rhs, ...)`
        helper: at every non-variable position of the goal,
        unify with each rule's LHS; on first success, return
        the witness substitution.  Companion
        `thvm_atp_get_witness(s, var_id) -> Term` API for
        retrieving bindings.  Tests: ~5 hand-built narrowing
        cases on tiny rule sets.
  - [x] 8.9c integrate narrow_step into saturation: new flag
        `s->goal_existential` + dispatch in `thvm_atp_goal_check`
        between the existing rewrite-and-compare path and the
        new narrow-and-extract path.  Witness substitution
        recorded at proof-close time.
  - [x] 8.9d existential-goal `.pr` syntax / WL surface:
        decide how users declare existential vars in input
        files (extension to `CONCLUSION` syntax? sidecar
        keyword?).  Add a fixture under `tests/data/atp/` and
        wire the bench harness.
  - [x] 8.9e WL bridge integration: `TATP[axioms, conjecture,
        Witness -> {x_, y_}]` returns `<|"Status" -> "PROVED",
        "Witness" -> <|x -> term, y -> term|>|>`.
- [x] 8.10 SupGen-style search inside saturation: superpose the
      "which CP to pick next" choice, collapse with priority,
      compare against the explicit-queue baseline
  - [x] 8.10a design memo `docs/plans/supgen_search_design.md`:
        observe that `thvm_atp_select_cp` already implements
        SupGen-style search for CP selection (each CP wrapped
        in INC^k, folded into a SUP tree, popped via
        `thvm_collapse_ordered`).  Survey further superposition
        opportunities: (1) unfailing-orient direction
        (KBO_UN -> superpose both directions and pick by
        priority), (2) ordering choice (KBO vs LPO superposed),
        (3) heuristic mode (--add vs --mix as alternative SUP
        children).  Pick ONE concrete extension for 8.10b that
        adds non-overlapping value beyond what's already in
        place.
  - [x] 8.10b implement the chosen extension + bench it
        against the explicit-queue / single-choice baseline
        from stages 5-9.  Goal: demonstrate that SupGen-style
        superposition either (a) saves work via lazy
        evaluation, (b) explores a search space that the
        explicit version can't, or (c) reveals an architectural
        cleanup opportunity.
  - [x] 8.10c IC-native ATP arc closing memo
        `docs/plans/waldmeister_ic_atp.md` section 7.3: recap what stages
        1-8.10 delivered, what remains research-grade and
        deferred (8.6 unordered SUP/DUP awaiting HVM4
        upstream; sort-aware KBO; multi-witness narrowing),
        and what the natural follow-on stages would be (TPTP
        file parsing in WL, AC matching, full pure-IC KBO
        port).  Closing the arc.

## Stage 9 -- follow-on continuation

> 8.10c marked the arc complete.  Stage 9 reopens with concrete
> items from the closing memo's "Natural follow-on stages" list,
> picked for buildability on existing infrastructure.  Each
> 9.x item gets its own design memo + decomposition on pickup
> per the closing memo's pattern of "discrete follow-on stages
> with their own design memos."

- [x] 9.1 multi-witness narrowing enumeration: stage 8.9's
      narrowing returns the FIRST witness found.  9.1 extends
      `thvm_atp_narrow_step` (or adds a sibling) that
      enumerates ALL witnesses up to a bound.  WL surface:
      `TATP[..., AllWitnesses -> True]` returns a list of
      Association entries.
  - [x] 9.1a design memo `docs/plans/waldmeister_ic_atp.md` section 7.4:
        survey the search-tree structure (DFS over (position,
        rule) choices); pick bounds (max-depth + max-witnesses
        + step-cap); decide if witness-distinctness should be
        enforced (drop alpha-equivalent duplicates) or left to
        the caller.  Document the relationship to 8.10's
        deferred trace-level SupGen (multi-witness IS a
        small-scale trace search; this could be the seed for
        the broader work).  Scope: ~150-line memo.
  - [x] 9.1b implement `thvm_atp_narrow_all` that runs a
        bounded DFS over narrow choices, collecting up to N
        witness substitutions.  Output: a stack-allocated
        `RewriteSubst` array + count.  Tests: hand-built
        problems with 0 / 1 / multiple witnesses.
  - [x] 9.1c WL surface: `TATP[..., AllWitnesses -> True]`
        returns `<|"Witnesses" -> {<|x -> term1|>,
        <|x -> term2|>, ...}|>` instead of `Witness ->
        <|...|>`.  Include the search budget options.
  - [x] 9.1d test fixture: a `.pr` file with multiple
        witnesses (e.g. `f(x, x) = a` with rules
        `f(b, b) = a` and `f(c, c) = a` -- two witnesses
        x=b and x=c).  Wire into bench harness.
- [x] 9.2 TPTP file parsing from WL: `TATP[File["foo.p"]]` --
      thin wrapper around the existing `wald_parse_file`.
      Detects file path argument, reads via the C-side
      parser, runs the saturator, decodes the result.  Most of
      the work is the file-vs-expression dispatch in TATP.
- [x] 9.3 heap-resetting mechanism between saturation steps:
      8.3e-iii's bench finding noted IC-rewrite at budget=256
      overflows HEAP_CAP.  Add a `thvm_atp_heap_checkpoint`
      that snapshots the heap pointer; saturation step pops
      back after the rewrite, reclaiming intermediate cells.
      Lets `use_ic_rewrite = 1` scale to longer runs.
- [x] 9.4 TPTP-UEQ corpus expansion: add 3-5 fixtures from
      GRP / RNG / LCL / LAT divisions to `tests/data/atp/`.
      Re-run `tools/bench_twee.c` and `test_bench_atp` on the
      enlarged corpus; document IC-vs-Twee gaps where they
      emerge.  This is where the 8.5d "KBO and LPO agree on
      our corpus" finding gets stress-tested.
  - [x] 9.4a design memo `docs/plans/waldmeister_ic_atp.md` section 7.5:
        pick 3-5 textbook UEQ candidates from
        GRP / RNG / LCL / LAT (we don't have network access in
        a cron firing, so hand-encode from textbook axioms).
        For each: signature, axioms, conjecture, predicted
        status under our 32-step budget, KBO vs LPO orientation
        notes.  Aim for at least one PROVED and at least one
        TIMEOUT to exercise both ends of the bench.  Scope:
        ~150-line memo.
  - [x] 9.4b implement the fixtures: write each `.pr` + `.expect`
        file from 9.4a, verify each parses (run through
        `test_bench_atp` and confirm status matches the
        prediction OR update the prediction if reality differs).
        Update CHANGELOG with status of each.  Scope: 3-5 small
        fixtures, ~30-50 lines each.
  - [x] 9.4c bench comparison + findings memo
        `docs/plans/waldmeister_ic_atp.md` section 7.5: re-run
        `tools/bench_twee` on the enlarged corpus; capture the
        IC-vs-Twee deltas; document KBO-vs-LPO disagreements (if
        any) and where the 32-step budget bites.  Scope: ~100-
        line memo + updated `build/bench-atp.csv` /
        `build/bench-twee.csv` snapshots.
- [x] 10 Wolfram-axiom Boolean corpus: add two `.pr` fixtures
      based on Wolfram's 2000 single-equation axiomatisation of
      Boolean algebra in NAND form, proven equivalent to
      standard Boolean algebra by McCune via EQP:
        `((a NAND b) NAND c) NAND (a NAND ((a NAND c) NAND a)) = c`
      One conservative fixture (predicted PROVED quickly under
      our 32-step budget) + one stress fixture (predicted
      TIMEOUT, useful for the next round of bench-Twee
      comparison).  Wire into the bench harness and update
      `docs/plans/waldmeister_ic_atp.md` section 7.5 with the new
      rows.  Reference: McCune et al., "Short Single Axioms for
      Boolean Algebra", J. Automated Reasoning 2002.
  - [x] 10a design memo `docs/plans/waldmeister_ic_atp.md` section 7.6:
        pick the two conjectures (one PROVED-fast, one TIMEOUT-
        stress) from the Wolfram axiom.  For each: signature
        (a single binary `nand` symbol + 1-3 constants),
        precedence chain, conjecture, predicted status, and
        rationale for the choice (why it's tractable vs. why
        it's hard).  The conservative pick should be a near-
        literal application of the axiom; the stress pick
        should be a derived identity such as Sheffer
        commutativity `nand(a, b) = nand(b, a)` or double-
        negation `nand(nand(a, a), nand(a, a)) = a`.  Scope:
        ~120-line memo.
  - [x] 10b implement the fixtures: write each `.pr` + `.expect`
        from 10a.  Verify each parses and runs through
        `test_bench_atp`; reconcile prediction-vs-observation
        in the memo (same protocol as 9.4b).  Update CHANGELOG
        under `## Unreleased` with the prediction/observation
        table.  Scope: 2 small fixtures, ~30-40 lines each.
  - [x] 10c bench comparison + findings update: re-run
        `tools/bench_twee` on the now 14-fixture corpus; append
        a new section to `docs/plans/waldmeister_ic_atp.md` section 7.5
        with the Wolfram-axiom rows + IC-vs-Twee deltas + any
        new lemma-discovery insights.  Scope: ~50-line append +
        regenerated bench CSVs.
