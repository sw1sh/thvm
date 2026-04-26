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
- [ ] 5.1 `AtpState` struct: rules `R`, CP queue, goal, KboConfig,
      step counter; init / free helpers
- [ ] 5.2 saturation step:
      - select CP from queue (priority collapse over INC-wrapped CPs)
      - normalize both sides under R (top-position + recursive descent)
      - discard if syntactically equal
      - orient by KBO; if unorientable, add as a 2-way rule pair
      - interreduce R against the new rule (scan, drop subsumed)
      - generate fresh CPs from new rule × R, push onto queue
      - goal-test: rewrite both sides of goal under R, check syntactic eq
- [ ] 5.3 priority queue construction: each CP wrapped `INC^k`
      where k = total symbol count (the `--add` heuristic; `--mix`
      lands later), enumerate via `thvm_collapse_ordered`
- [ ] 5.4 recursive-descent rewriter: extend `thvm_rewrite_step` to
      try every position in the term, not just top
- [ ] 5.5 demo: prove `f(a, i(a)) = e` from the standard group
      axioms (assoc + right-id + right-inv) via saturation; record
      step count

## Stage 6 -- proof trace + Waldmeister parser

- [ ] 6.1 per-step trace tuple `(reason_tag, parent_ids, subst)`
      packed into a TAG_CTR; threaded through `thvm_rewrite_step`
      and the saturation step
- [ ] 6.2 PCL-shaped trace serializer to text
- [ ] 6.3 parser for `waldmeister/documents/example.pr`-style spec
      files (NAME / SORTS / SIGNATURE / ORDERING / VARIABLES /
      EQUATIONS / CONCLUSION)
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
