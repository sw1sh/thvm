# atp.mk - the ATP track of the single-TU build, included by the Makefile
# after BIN, BUILD and SRC: the engine build defines (ATP_DEFINES for the C
# tests and bin/FindProof, WL_ATP_DEFINES for the paclet), the ATP test list
# (ATP_TESTS, part of TESTS), the tests under tests/atp/, the FindProof CLI,
# the diagnostic probes, the benches and the PGO build.

# Milestone 7d: -DATP_FV_INDEX adds a feature-vector subsumption index
# over the CP queue (and rule set) -- a sound over-approximation that
# turns the O(n_cps) thvm_match scan in atp_cp_queue_subsumed into an
# O(retrieval) candidate lookup + match on the survivors.  ON by
# default: it is part of the canonical engine and ships byte-for-byte
# (same verdict as the array scan, just faster).
# `make ATP_FV_INDEX=0 bin/test_atp` builds the
# milestone-7 array scan -- the regression oracle.
ATP_FV_INDEX ?= 1
ATP_DEFINES  := $(if $(filter-out 0,$(ATP_FV_INDEX)),-DATP_FV_INDEX,)

# Milestone 7e (normalization wall, lever 1): -DATP_CP_DIAG re-enables
# the two COUNTER-ONLY CP filters in `atp_push_cps_traced` --
# `atp_cp_source_disjoint_connected` (7.2b) and `atp_cp_rule_subsumed`
# (7.3a).  Their verdicts feed only `n_cps_dropped_connected` /
# `n_cps_dropped_rule_subsumed`; neither ever drops a CP, so the
# default build skips both calls (each is two full
# `atp_rewrite_normalize` passes + an O(n_rules) match scan -- ~half
# the per-CP filter cost).  Skipping them is behavior-identical: same
# CPs queued, same proof.  Set ATP_CP_DIAG=1 to recover the
# measurement counters.  The functions stay defined either way (the
# test_atp unit tests call them directly).
ATP_CP_DIAG ?=
ATP_DEFINES += $(if $(filter-out 0,$(ATP_CP_DIAG)),-DATP_CP_DIAG,)

# Milestone 7e (normalization wall, lever 2): -DATP_RULE_INDEX builds a
# discrimination tree over rule LHS terms and routes the ATP-side
# normalizer's redex search through it instead of the O(n_rules)
# linear LHS scan in `rewrite_try_top`.  ON by default: part of the
# canonical engine and behavior-identical -- among the indexed
# candidates matching a position the lowest rule index wins, exactly
# replicating the linear scan's first-match choice.  Independent of
# every other ATP flag.  `make ATP_RULE_INDEX=0` recovers the linear
# scan -- the regression oracle.
ATP_RULE_INDEX ?= 1
ATP_DEFINES    += $(if $(filter-out 0,$(ATP_RULE_INDEX)),-DATP_RULE_INDEX,)

# Milestone 7c (convergence fix): -DATP_VAR_NORM canonically renumbers
# the variables of every stored rule and queued CP to a dense [0, k)
# set shared across both sides (alpha-renaming).  This keeps every
# stored variable below the REWRITE_MAX_VAR (=64) matcher cliff -- the
# CP enumerator bakes CP_RENAME_OFFSET into stored terms, so deep
# overlaps otherwise carry ids past 64 where thvm_match goes dead and
# all redundancy (joinability / subsumption / interreduction) dies.
# Renumbering also makes alpha-equivalent rules/CPs byte-identical so
# the dedup + duplicate-rule guard fire.  ON by default: it is the
# convergence fix -- without it the engine diverges and never proves
# `thm` (with it `thm` proves in ~0.2s).  `make ATP_VAR_NORM=0`
# recovers the milestone-7 (buggy, divergent) engine for A/B.
# Independent of every other ATP flag.
ATP_VAR_NORM ?= 1
ATP_DEFINES  += $(if $(filter-out 0,$(ATP_VAR_NORM)),-DATP_VAR_NORM,)

# thm convergence (Waldmeister lever 1): -DATP_GOAL_HEURISTIC adds
# goal-directed CP selection (Waldmeister CPinGoal / GoalinCP, see
# external/waldmeister/sources/CLAS/Clas_CP_Goal.c).  A CP whose subterms
# structurally match the conjecture is preferred; one unrelated to the
# goal has its priority scaled up so it sinks in the selection heap.
# This steers saturation toward the goal instead of blindly enumerating
# -- without it cpl1 / subl2 / thm trace identical trajectories because
# the goal only gates the goal-check.  Off by default; independent of
# every other ATP flag.  CHANGES BEHAVIOR (a heuristic -- the search
# trajectory moves, completion stays sound).  No effect in completion
# mode (no goal).
ATP_GOAL_HEURISTIC ?=
ATP_DEFINES  += $(if $(filter-out 0,$(ATP_GOAL_HEURISTIC)),-DATP_GOAL_HEURISTIC,)

# thm convergence (Waldmeister lever 2): -DATP_ORPHAN_KILL adds orphan
# deletion (Waldmeister's "Waisenmord").  When interreduction drops a
# rule, the queued critical pairs descended from it are redundant --
# the re-queued reduced equation regenerates whatever they contribute
# -- so they are compacted out of the CP queue.  ON by default: it is
# a sound completion criterion (a relative of Waldmeister's
# selectNonOrphan) and a measured win -- on the deep wolfram benchmark
# it halves the per-step cost and keeps the CP queue from filling with
# CPs of churned-away rules.  `make ATP_ORPHAN_KILL=0` recovers the
# legacy path.  Independent of every other ATP flag.
ATP_ORPHAN_KILL ?= 1
ATP_DEFINES  += $(if $(filter-out 0,$(ATP_ORPHAN_KILL)),-DATP_ORPHAN_KILL,)

# thm convergence (9c foundation): -DATP_ORDERED_REWRITE replaces the
# KBO_UN both-ways hack (which stored an unorientable equation u=v as
# two looping rules u->v and v->u, a queue-blowup source) with proper
# unfailing-completion ordered rewriting: the equation is stored once,
# and the rewrite step tries every rule in BOTH directions, applying a
# direction only when it strictly decreases the redex in the reduction
# order.  An oriented rule fires forward only; an unorientable equation
# fires whichever direction decreases.  Terminating.  On by default:
# the all-oriented rule set (the common case) still takes the indexed
# normalizer -- atp_rewrite_normalize_ordered drops to the linear scan
# only while n_unorient > 0 -- so it is perf-neutral on orientable
# problems and is the only path that closes a symmetric goal (a
# commutativity equation normalizes a ground/skolemized goal to a
# canonical face).  `make ATP_ORDERED_REWRITE=0` restores the hack.
ATP_ORDERED_REWRITE ?= 1
ATP_DEFINES  += $(if $(filter-out 0,$(ATP_ORDERED_REWRITE)),-DATP_ORDERED_REWRITE,)

# -DATP_FLATTERM_DIFF compiles the test_atp flatterm differential block
# (tree mixed NF == flatterm mixed NF, AND flatterm resume-ON == resume-OFF
# NF) over 4000 random mixed-rule subjects.  Off by default (the default
# test_atp run is the 135603-assertion suite); `make ATP_FLATTERM_DIFF=1
# bin/test_atp` adds the differential.
ATP_FLATTERM_DIFF ?=
ATP_DEFINES  += $(if $(filter-out 0,$(ATP_FLATTERM_DIFF)),-DATP_FLATTERM_DIFF,)

# -DATP_FLATTERM_SELFCHECK runs the flatterm mixed normalizer ALONGSIDE the
# tree mixed normalizer on every live normalize and aborts on any NF
# mismatch (proves flatterm NF == tree NF on the saturation workload, not
# just the offline random differential).  Defeats the speedup; never in a
# release build.  `make ATP_FLATTERM_SELFCHECK=1 bin/test_atp`.
ATP_FLATTERM_SELFCHECK ?=
ATP_DEFINES  += $(if $(filter-out 0,$(ATP_FLATTERM_SELFCHECK)),-DATP_FLATTERM_SELFCHECK,)

# Milestone 10: -DATP_MNF builds the MNF goal-directed search (a port
# of Waldmeister's "MultipleNormalFormen" module).  It AUGMENTS the
# single-normal-form goal check: goal_lhs seeds a GREEN front, goal_rhs
# a RED one; each front rewrites with R (forward, and -- up to
# MNF_MAX_ANTI backward steps per lineage -- through unorientable
# equations); an opposite-colour collision is the join.  With the
# backward steps `make ATP_MNF=1` PROVES NAND commutativity from the
# single Wolfram axiom (wolfram.pr) and comm_monoid_swap -- goals the
# single-NF check structurally cannot close.  Off by default still:
# the MNF front search runs every goal_check, which is pure overhead
# on a goal the single-NF check will close on its own (it regresses
# thm ~50x).  Making MNF cheap enough to default on is open work.
ATP_MNF ?=
ATP_DEFINES  += $(if $(filter-out 0,$(ATP_MNF)),-DATP_MNF,)
# The paclet dylib (wl/THVMLink) ALWAYS compiles MNF in: the front
# search is runtime-gated by AtpState.use_mnf (set via
# thvm_atp_set_use_mnf / Method -> "GoalDirected"), so a compiled-in
# MNF stays inert -- and free except one branch test -- on
# completion-only goals.  This lets the shipped dylib prove symmetric
# goals on demand without a separate build.  C test binaries do NOT
# get this (they key off ATP_MNF above), so `make ATP_MNF=1` is still
# the way to exercise MNF from the C tests.  Override with
# `WL_ATP_MNF=0 make wl` to omit it from the dylib.
WL_ATP_MNF      ?= 1
WL_ATP_DEFINES  := $(if $(filter-out 0,$(WL_ATP_MNF)),-DATP_MNF,)
# ATP_MNF_DIAG: stderr trace of the MNF set (node count, queue sizes,
# rules fed) -- a bring-up diagnostic, implies -DATP_MNF.
ATP_MNF_DIAG ?=
ATP_DEFINES  += $(if $(filter-out 0,$(ATP_MNF_DIAG)),-DATP_MNF -DATP_MNF_DIAG,)

# Ground-joinability redundancy criterion (Martin-Nipkow / Twee CADE
# 2021 sec 3.1; AHL 2003) in src/atp/_.c.  -DATP_CP_GROUND_JOIN compiles
# in atp_cp_ground_joinable; the n_cps_ground_joinable counter always
# ticks, and DELETION of a ground-joinable CP is runtime-gated by
# AtpState.use_ground_join (set via thvm_atp_set_use_ground_join /
# Method -> {... "GroundJoin" -> True}).  ON for C tests (so test_atp
# exercises it) and ALWAYS compiled into the paclet dylib (like MNF):
# inert until the WL surface opts in, costing only one branch per CP.
ATP_CP_GROUND_JOIN ?= 1
ATP_DEFINES     += $(if $(filter-out 0,$(ATP_CP_GROUND_JOIN)),-DATP_CP_GROUND_JOIN,)
WL_ATP_DEFINES  += $(if $(filter-out 0,$(ATP_CP_GROUND_JOIN)),-DATP_CP_GROUND_JOIN,)

# AC reasoning (src/atp/ac.c) shipped in the paclet too: the C-test
# build already compiles AC in (-DTHVM_ATP_AC); the paclet was the
# odd one out.  All AC gates are runtime-gated on engine-global
# `g_atp_ac_info.ac_mask` (set via thvm_atp_set_ac_mask /
# thvm_atp_auto_ac) -- ac_mask = 0 is byte-identical to a non-AC
# build, so the default paclet behaviour is unchanged.  An opt-in
# Environment["THVM_ATP_AUTO_AC", "1"] lights up the AC analyzer +
# Bachmair-Plaisted extended rewriting / AC-overlap CPs / AC-eq
# goal-check on theories whose axiom set surfaces an AC top symbol.
WL_ATP_DEFINES  += -DTHVM_ATP_AC

# AtpFt flatterm-native infrastructure.  Stage 4 (RULES) is now baked
# in as engine-default storage: every atp_push_rule writes a parallel
# AtpFtCell mirror into s->lhs_ft[]/s->rhs_ft[] in lockstep with the
# Term-side rule slot, and every rule-array mutation path (grow,
# soft-delete, set_goal, interreduce compaction, swap, free) syncs the
# mirror via the helpers in the THVM_ATPFT_RULES blocks of src/atp/.
# All compile-time ATPFT readers (NORM verdict, RI discrim descend,
# CPQ queue mirror, match) gate on RULES; without it they #error out.
# Runtime activation of the read-side accelerators stays env-knob:
# THVM_ATPFT_NORM=1 routes joinability verdicts through ft_norm,
# THVM_ATPFT_RI=1 routes find-redex through the FT discrim descent.
# OFF the env knobs the mirror is pure storage -- the Term path is
# byte-identical to a no-FT build, modulo a ~720KB slab pool per
# AtpState.  Defaulting ATPFT_RULES on lets the WL paclet and the C
# test binaries share one configuration; the env knobs above stay the
# user-visible toggles.  Bench evidence: +55% steps/sec on
# test_atp_wolfram_bench andassoc (9ab38b58); regression clean on
# bin/test_atp_ft_norm 135624/135624 in both env modes.
ATP_DEFINES     += -DTHVM_ATPFT_ALLOC -DTHVM_ATPFT_CONVERT \
                   -DTHVM_ATPFT_LPO -DTHVM_ATPFT_MATCH \
                   -DTHVM_ATPFT_RULES -DTHVM_ATPFT_NORM
WL_ATP_DEFINES  += -DTHVM_ATPFT_ALLOC -DTHVM_ATPFT_CONVERT \
                   -DTHVM_ATPFT_LPO -DTHVM_ATPFT_MATCH \
                   -DTHVM_ATPFT_RULES -DTHVM_ATPFT_NORM

# ATP auto-precedence (Waldmeister PhilMarlow / Praezedenzgenerator
# port): -DATP_AUTO_PREC makes the ATP bench harness + WL glue
# replace the syntactic `precedence[i]=i+1` default with a
# precedence derived from per-operator algebraic-property analysis
# (commutativity / associativity / idempotence / units / inverses /
# distributivity).  OFF by default until benchmarked: build with
# `make ATP_AUTO_PREC=1` to opt in.  See src/atp/precedence.c.
ATP_AUTO_PREC ?=
ATP_DEFINES  += $(if $(filter-out 0,$(ATP_AUTO_PREC)),-DATP_AUTO_PREC,)

ATP_TESTS := \
  $(BIN)/test_kbo \
  $(BIN)/test_rewrite \
  $(BIN)/test_unify \
  $(BIN)/test_cp \
  $(BIN)/test_wald \
  $(BIN)/test_atp \
  $(BIN)/test_cc \
  $(BIN)/test_ffmep \
  $(BIN)/test_atp_analysis \
  $(BIN)/test_atp_enigma \
  $(BIN)/test_atp_multiproof \
  $(BIN)/test_atp_cp_graph \
  $(BIN)/test_atp_rerank \
  $(BIN)/test_atp_guide \
  $(BIN)/test_atp_guide_ac \
  $(BIN)/test_atp_gnn_score \
  $(BIN)/test_atp_problem_features \
  $(BIN)/test_atp_ifeq10 \
  $(BIN)/test_bench_atp \
  $(BIN)/test_sup_cps \
  $(BIN)/test_kbo_pri \
  $(BIN)/test_rewrite_pri \
  $(BIN)/test_sup_rewrite \
  $(BIN)/test_lpo \
  $(BIN)/test_rpo \
  $(BIN)/test_atp_rpo \
  $(BIN)/test_wpo \
  $(BIN)/test_atp_wpo \
  $(BIN)/test_fol \
  $(BIN)/test_fol_pelletier \
  $(BIN)/test_ft_alloc \
  $(BIN)/test_ft \
  $(BIN)/test_ft_deep \
  $(BIN)/test_atp_deep \
  $(BIN)/test_atp_cli_deep \
  $(BIN)/test_atp_deep_cp \
  $(BIN)/test_ft_order \
  $(BIN)/test_atp_ft_rules \
  $(BIN)/test_atp_gc_threshold \
  $(BIN)/test_atp_ft \
  $(BIN)/test_ft_match \
  $(BIN)/test_ft_norm \
  $(BIN)/test_atp_ft_norm \
  $(BIN)/test_ft_ri \
  $(BIN)/test_atp_ft_ri \
  $(BIN)/test_ft_cpq \
  $(BIN)/test_atp_ft_cpq \
  $(BIN)/test_lpo_cache \
  $(BIN)/test_ac \
  $(BIN)/test_atp_ac \
  $(BIN)/test_atp_ac_bench \
  $(BIN)/test_atp_session_reuse \
  $(BIN)/test_kbo_linear

# Every tests/atp/test_<name>.c without its own rule below.
$(BIN)/test_%: tests/atp/test_%.c $(SRC) | $(BIN)
	$(CC) $(CFLAGS) $(TEST_DEFINES) $(ATP_DEFINES) -o $@ $< $(TEST_LDFLAGS)

# Stage 1 AtpFt allocator -- src/atp/ft_alloc.c is gated on
# THVM_ATPFT_ALLOC; nothing in src/thvm.c includes it yet, so only the
# test binary turns the flag on.  The test file #includes ft_alloc.c
# directly after src/thvm.c, so we pass the test source like any other
# test target -- the per-rule -DTHVM_ATPFT_ALLOC define is what
# unlocks the file.
$(BIN)/test_ft_alloc: tests/atp/test_ft_alloc.c $(SRC) | $(BIN)
	$(CC) $(CFLAGS) $(TEST_DEFINES) $(ATP_DEFINES) -DTHVM_ATPFT_ALLOC -o $@ $< $(TEST_LDFLAGS)

# Stage 2 AtpFt converters -- src/atp/ft.c is gated on
# THVM_ATPFT_CONVERT (and depends on the Stage 1 ft_alloc.c gated on
# THVM_ATPFT_ALLOC).  Same convention as test_ft_alloc above: the test
# file #includes ft_alloc.c + ft.c directly after src/thvm.c, and the
# per-rule -D defines unlock both.
$(BIN)/test_ft: tests/atp/test_ft.c $(SRC) | $(BIN)
	$(CC) $(CFLAGS) $(TEST_DEFINES) $(ATP_DEFINES) -DTHVM_ATPFT_ALLOC -DTHVM_ATPFT_CONVERT -o $@ $< $(TEST_LDFLAGS)

# Stage 3 AtpFt-native LPO/KBO -- src/atp/ft_order.c is gated on
# THVM_ATPFT_CONVERT (same envelope as ft.c).  The test file
# #includes ft_alloc.c + ft.c + ft_order.c directly after src/thvm.c;
# the per-rule -D defines unlock all three layers (THVM_ATPFT_ALLOC,
# THVM_ATPFT_CONVERT, THVM_ATPFT_LPO).
$(BIN)/test_ft_order: tests/atp/test_ft_order.c $(SRC) | $(BIN)
	$(CC) $(CFLAGS) $(TEST_DEFINES) $(ATP_DEFINES) -DTHVM_ATPFT_ALLOC -DTHVM_ATPFT_CONVERT -DTHVM_ATPFT_LPO -o $@ $< $(TEST_LDFLAGS)

# Stage 4 AtpFt-backed rule storage smoke test -- src/atp/_.c pulls in
# ft.h + ft_alloc.c + ft.c under THVM_ATPFT_RULES, so all four flags
# are passed together (RULES requires ALLOC + CONVERT; LPO included so
# the ft_order.c entry points stay live for Stage 5+ readers).  The
# test pushes a small batch of Sheffer rules and asserts the AtpFt
# mirror agrees with the Term path on every slot (ft_eq + ft_hash).
$(BIN)/test_atp_ft_rules: tests/atp/test_atp_ft_rules.c $(SRC) | $(BIN)
	$(CC) $(CFLAGS) $(TEST_DEFINES) $(ATP_DEFINES) -DTHVM_ATPFT_ALLOC -DTHVM_ATPFT_CONVERT -DTHVM_ATPFT_LPO -DTHVM_ATPFT_RULES -o $@ $< $(TEST_LDFLAGS)

# Stage 4 headline acceptance -- the full 135624-assertion test_atp
# suite compiled with the AtpFt parallel writes enabled.  Off the
# flags the run is byte-identical to bin/test_atp; with them on every
# rule push / goal set / interreduce compaction also writes the AtpFt
# mirror, so a slot drift (or a Stage-4 bug) would surface as either
# a hard verify-abort (with THVM_ATPFT_VERIFY=1) or an assertion-count
# divergence vs the 135624 baseline.
$(BIN)/test_atp_ft: tests/atp/test_atp.c $(SRC) | $(BIN)
	$(CC) $(CFLAGS) $(TEST_DEFINES) $(ATP_DEFINES) -DTHVM_ATPFT_ALLOC -DTHVM_ATPFT_CONVERT -DTHVM_ATPFT_LPO -DTHVM_ATPFT_RULES -o $@ $< $(TEST_LDFLAGS)

# Stage 5 AtpFt-native match + substitution-apply -- src/atp/ft_match.c
# is gated on THVM_ATPFT_MATCH (and depends on ALLOC + CONVERT + LPO).
# Stage 5 is purely additive: ft_match.c has no live callers in
# src/atp/_.c at this stage (wiring landed later), so only this test
# binary turns the flag on.
# The test file #includes ft_alloc.c + ft.c + ft_order.c + ft_match.c
# directly after src/thvm.c; the per-rule -D defines unlock all four.
$(BIN)/test_ft_match: tests/atp/test_ft_match.c $(SRC) | $(BIN)
	$(CC) $(CFLAGS) $(TEST_DEFINES) $(ATP_DEFINES) \
	  -DTHVM_ATPFT_ALLOC -DTHVM_ATPFT_CONVERT \
	  -DTHVM_ATPFT_LPO -DTHVM_ATPFT_MATCH \
	  -o $@ $< $(TEST_LDFLAGS)

# === Stage 6: splice + normalize + push-norm wiring ==================
# test_ft_norm   - differential corpus (Term-NF vs AtpFt-NF) for
#                  random subjects + random orientable rule sets.
# test_atp_ft_norm
#                - the full 135624-assertion test_atp suite built with
#                  THVM_ATPFT_NORM=1 -- env-knob off by default so the
#                  Term path stays authoritative; env-on flips to the
#                  AtpFt verdict.
# test_atp_ft_norm_verify
#                - same binary as test_atp_ft_norm but the bench /
#                  bring-up runs it under THVM_ATPFT_NORM_VERIFY=1
#                  (mismatch -> abort).  Same .c file, same flags --
#                  the env knob does the switching.
$(BIN)/test_ft_norm: tests/atp/test_ft_norm.c $(SRC) | $(BIN)
	$(CC) $(CFLAGS) $(TEST_DEFINES) $(ATP_DEFINES) \
	  -DTHVM_ATPFT_ALLOC -DTHVM_ATPFT_CONVERT \
	  -DTHVM_ATPFT_LPO -DTHVM_ATPFT_MATCH \
	  -DTHVM_ATPFT_RULES -DTHVM_ATPFT_NORM \
	  -o $@ $< $(TEST_LDFLAGS)

$(BIN)/test_atp_ft_norm: tests/atp/test_atp.c $(SRC) | $(BIN)
	$(CC) $(CFLAGS) $(TEST_DEFINES) $(ATP_DEFINES) \
	  -DTHVM_ATPFT_ALLOC -DTHVM_ATPFT_CONVERT \
	  -DTHVM_ATPFT_LPO -DTHVM_ATPFT_MATCH \
	  -DTHVM_ATPFT_RULES -DTHVM_ATPFT_NORM \
	  -o $@ $< $(TEST_LDFLAGS)

$(BIN)/test_atp_ft_norm_verify: tests/atp/test_atp.c $(SRC) | $(BIN)
	$(CC) $(CFLAGS) $(TEST_DEFINES) $(ATP_DEFINES) \
	  -DTHVM_ATPFT_ALLOC -DTHVM_ATPFT_CONVERT \
	  -DTHVM_ATPFT_LPO -DTHVM_ATPFT_MATCH \
	  -DTHVM_ATPFT_RULES -DTHVM_ATPFT_NORM \
	  -o $@ $< $(TEST_LDFLAGS)

# Stage 6b: AtpFt-native discrim-tree descent.  Pulls in ft_ri.c on
# top of the Stage 6 envelope; the THVM_ATPFT_RI flag enables the
# descent path inside ft_norm.c (env-knob THVM_ATPFT_RI=1 to actually
# USE it; the default leaves the linear-scan fallback in place).
#
# test_ft_ri      - differential corpus: ft_norm linear-scan vs ft_norm
#                   discrim-tree path on random rules + random subjects.
# test_atp_ft_ri  - the full ATP suite under Stage 6b's envelope.
$(BIN)/test_ft_ri: tests/atp/test_ft_ri.c $(SRC) | $(BIN)
	$(CC) $(CFLAGS) $(TEST_DEFINES) $(ATP_DEFINES) \
	  -DTHVM_ATPFT_ALLOC -DTHVM_ATPFT_CONVERT \
	  -DTHVM_ATPFT_LPO -DTHVM_ATPFT_MATCH \
	  -DTHVM_ATPFT_RULES -DTHVM_ATPFT_NORM \
	  -DTHVM_ATPFT_RI \
	  -o $@ $< $(TEST_LDFLAGS)

$(BIN)/test_atp_ft_ri: tests/atp/test_atp.c $(SRC) | $(BIN)
	$(CC) $(CFLAGS) $(TEST_DEFINES) $(ATP_DEFINES) \
	  -DTHVM_ATPFT_ALLOC -DTHVM_ATPFT_CONVERT \
	  -DTHVM_ATPFT_LPO -DTHVM_ATPFT_MATCH \
	  -DTHVM_ATPFT_RULES -DTHVM_ATPFT_NORM \
	  -DTHVM_ATPFT_RI \
	  -o $@ $< $(TEST_LDFLAGS)

# Stage 7: AtpFt-native CP queue (dual-store).  Pulls in ft_cpq.c on
# top of the Stage 6 envelope; the THVM_ATPFT_CPQ flag enables the
# parallel cp_packed_ft[] queue inside _.c.  The legacy cp_packed[]
# byte queue stays populated alongside it (FV index + peek/stash
# consumers stay on the byte queue until Stages 8-9).
#
# test_ft_cpq      - push/pop/swap parity between legacy and FT views
#                    + atp_cp_trivially_joinable_ft verdict parity.
# test_atp_ft_cpq  - the full ATP suite under Stage 7's envelope.
$(BIN)/test_ft_cpq: tests/atp/test_ft_cpq.c $(SRC) | $(BIN)
	$(CC) $(CFLAGS) $(TEST_DEFINES) $(ATP_DEFINES) \
	  -DTHVM_ATPFT_ALLOC -DTHVM_ATPFT_CONVERT \
	  -DTHVM_ATPFT_LPO -DTHVM_ATPFT_MATCH \
	  -DTHVM_ATPFT_RULES -DTHVM_ATPFT_NORM \
	  -DTHVM_ATPFT_CPQ \
	  -o $@ $< $(TEST_LDFLAGS)

$(BIN)/test_atp_ft_cpq: tests/atp/test_atp.c $(SRC) | $(BIN)
	$(CC) $(CFLAGS) $(TEST_DEFINES) $(ATP_DEFINES) \
	  -DTHVM_ATPFT_ALLOC -DTHVM_ATPFT_CONVERT \
	  -DTHVM_ATPFT_LPO -DTHVM_ATPFT_MATCH \
	  -DTHVM_ATPFT_RULES -DTHVM_ATPFT_NORM \
	  -DTHVM_ATPFT_CPQ \
	  -o $@ $< $(TEST_LDFLAGS)

# CPQ wolfram bench: the WM-preset .pr arm under the Stage-7 FT queue
# (the config the plain ft_cpq suite does NOT exercise -- its blind
# spot hid the fuse-gate wrong-engine bug fixed in 1b7ac915).
$(BIN)/test_atp_wolfram_bench_cpq: tests/atp/test_atp_wolfram_bench.c $(SRC) | $(BIN)
	$(CC) $(CFLAGS) $(TEST_DEFINES) $(ATP_DEFINES) \
	  -DTHVM_ATP_AC -DATP_MNF -DTHVM_ATPFT_CPQ \
	  -o $@ $< $(TEST_LDFLAGS)

# Regression gate for the CPQ .pr arm: the WM preset on a real .pr must
# PROVE with the byte-exact trajectory (QUEUE_EMPTY here = the fused
# formation route is off or the slot mirror is fabricating content).
.PHONY: test-cpq-pr
test-cpq-pr: $(BIN)/test_atp_wolfram_bench_cpq
	@THVM_ATP_RSS_ABORT_MB=6000 THVM_ATP_WALDMEISTER=1 \
	  $(BIN)/test_atp_wolfram_bench_cpq \
	  tools/baselines/wm_pr/McCuneAxioms__Associativity.pr 200000 30 \
	  | grep -q "^=> PROVED" \
	  && echo "test-cpq-pr: ok (PROVED)" \
	  || { echo "test-cpq-pr: FAIL (expected PROVED)"; exit 1; }

# LPO/KBO orientability cache (src/atp/lpo_cache.c).  Gated by
# THVM_ATP_LPO_ORIENT_CACHE -- the default build is byte-identical,
# but the test binary turns it on so the cache APIs are live.
$(BIN)/test_lpo_cache: tests/atp/test_lpo_cache.c $(SRC) | $(BIN)
	$(CC) $(CFLAGS) $(TEST_DEFINES) $(ATP_DEFINES) \
	  -DTHVM_ATP_LPO_ORIENT_CACHE \
	  -o $@ $< $(TEST_LDFLAGS)

# ENIGMA selection layer (src/atp/guide.c).  THVM_ATP_GUIDE_CHECK aborts
# when a clean pick writes a cell-keyed memo; THVM_ATP_TEST_CLOCK lets the
# test drive the wall and CPU clocks (--lrs prunes at fixed selections).
# The _ac build adds FindProof's AC and MNF defines, so the layer's AC
# orientation branch is compiled and run.
$(BIN)/test_atp_guide: tests/atp/test_atp_guide.c $(SRC) | $(BIN)
	$(CC) $(CFLAGS) $(TEST_DEFINES) $(ATP_DEFINES) \
	  -DTHVM_ATP_GUIDE_CHECK -DTHVM_ATP_TEST_CLOCK \
	  -o $@ $< $(TEST_LDFLAGS)

$(BIN)/test_atp_guide_ac: tests/atp/test_atp_guide.c $(SRC) | $(BIN)
	$(CC) $(CFLAGS) $(TEST_DEFINES) $(ATP_DEFINES) \
	  -DTHVM_ATP_GUIDE_CHECK -DTHVM_ATP_TEST_CLOCK -DTHVM_ATP_AC -DATP_MNF \
	  -o $@ $< $(TEST_LDFLAGS)

# AC reasoning (src/atp/ac.c).  Gated by THVM_ATP_AC.  With the mask
# left at 0 (the default), the AC-eq trivial-join hook is a no-op,
# so the test suite is byte-identical to the default build.
$(BIN)/test_ac: tests/atp/test_ac.c $(SRC) | $(BIN)
	$(CC) $(CFLAGS) $(TEST_DEFINES) $(ATP_DEFINES) \
	  -DTHVM_ATP_AC \
	  -o $@ $< $(TEST_LDFLAGS)

# test_atp.c compiled with -DTHVM_ATP_AC.  The headline acceptance:
# the full 135624-assertion ATP regression suite under a build that
# has AC support compiled in (mask defaults to 0 so behaviour is
# byte-identical to the default).
$(BIN)/test_atp_ac: tests/atp/test_atp.c $(SRC) | $(BIN)
	$(CC) $(CFLAGS) $(TEST_DEFINES) $(ATP_DEFINES) \
	  -DTHVM_ATP_AC \
	  -o $@ $< $(TEST_LDFLAGS)

# AC bench: commutative-monoid theorem mirrored from
# external/waldmeister/commutative_monoid.pr.  Times thvm with + without AC
# mask + outputs side-by-side with wmcli's recorded run.
$(BIN)/test_atp_ac_bench: tests/atp/test_atp_ac_bench.c $(SRC) | $(BIN)
	$(CC) $(CFLAGS) $(TEST_DEFINES) $(ATP_DEFINES) \
	  -DTHVM_ATP_AC \
	  -o $@ $< $(TEST_LDFLAGS)

# C-level reproducer of the AC-on AbelianGroupAxioms/ImpliesAbelianMcCune
# regression surfaced by the WL paclet's THVM_ATP_AUTO_AC=1 path.
# Syntactic saturation proves; AC saturation hits QUEUE_EMPTY without
# proving.  Failing test stays in until the AC pipeline finds the
# missing inference.
$(BIN)/test_atp_ac_abelian_repro: tests/atp/test_atp_ac_abelian_repro.c $(SRC) | $(BIN)
	$(CC) $(CFLAGS) $(TEST_DEFINES) $(ATP_DEFINES) \
	  -DTHVM_ATP_AC \
	  -o $@ $< $(TEST_LDFLAGS)

# The wolfram bench drives the WM-alignment sweep (tools/wm_align_sweep).
# Its WALDMEISTER preset runs WM's default-ON dokgP/Permsub CP filter
# (GZ_ACVerzichtbar) which needs the AC machinery (src/atp/ac.c).  The
# mask is 0 unless thvm_atp_auto_ac detects an AC operator, so the
# non-AC trajectory stays byte-identical.
# -DATP_MNF: always compile the MNF goal-directed front in (like the
# paclet dylib); it stays runtime-gated on AtpState.use_mnf, which the
# WALDMEISTER preset arms for GROUND goals to match `wmcli -auto`'s
# zb=mnf goal treatment.  Completion-only goals leave it inert.
$(BIN)/test_atp_wolfram_bench: tests/atp/test_atp_wolfram_bench.c $(SRC) | $(BIN)
	$(CC) $(CFLAGS) $(TEST_DEFINES) $(ATP_DEFINES) \
	  -DTHVM_ATP_AC -DATP_MNF \
	  -o $@ $< $(TEST_LDFLAGS)

# Multi-session regression gate: several WM-preset engine sessions in
# ONE process (the WL-bridge lifecycle the one-problem-per-process CLI
# never exercises).  Guards the cross-session flatterm-arena corruption
# class: any file-scope cache keyed by arena address / span pointer /
# r_revision aliases across thvm_atp_free+init when malloc reuses the
# arena address (free_span underflow, SIGBUS).
$(BIN)/test_atp_session_reuse: tests/atp/test_atp_session_reuse.c \
    tests/atp/test_atp_wolfram_bench.c $(SRC) | $(BIN)
	$(CC) $(CFLAGS) $(TEST_DEFINES) $(ATP_DEFINES) \
	  -DTHVM_ATP_AC -DATP_MNF \
	  -o $@ $< $(TEST_LDFLAGS)

# Deep-term walks pinned to their recursive originals, in FindProof's
# configuration (AC and MNF compiled in).  test_ft_deep, test_atp_deep,
# test_atp_cli_deep and test_atp_deep_cp build many 100,000-deep terms
# and reserve a 512 MB reference thread stack (tests/atp/deep_stack.h):
# run them one at a time, never in parallel with other heavy jobs.
# tools/atp_enigma/ftdeep_pod.sh runs them on a pod and records each
# one's peak RSS; the G2.3 pod run lists them next to test_atp.
$(BIN)/test_ft_deep: tests/atp/test_ft_deep.c tests/atp/deep_stack.h $(SRC) | $(BIN)
	$(CC) $(CFLAGS) $(TEST_DEFINES) $(ATP_DEFINES) \
	  -DTHVM_ATP_AC -DATP_MNF \
	  -o $@ $< $(TEST_LDFLAGS)

$(BIN)/test_atp_deep: tests/atp/test_atp_deep.c tests/atp/deep_stack.h $(SRC) | $(BIN)
	$(CC) $(CFLAGS) $(TEST_DEFINES) $(ATP_DEFINES) \
	  -DTHVM_ATP_AC -DATP_MNF \
	  -o $@ $< $(TEST_LDFLAGS)

# The deep critical-pair push of the G0.1 crash rows, end to end.
$(BIN)/test_atp_deep_cp: tests/atp/test_atp_deep_cp.c tests/atp/deep_stack.h $(SRC) | $(BIN)
	$(CC) $(CFLAGS) $(TEST_DEFINES) $(ATP_DEFINES) \
	  -DTHVM_ATP_AC -DATP_MNF \
	  -o $@ $< $(TEST_LDFLAGS)

# FindProof's proof-output walks (tools/atp_cli/szs.c, tools/proof_check.c)
# on deep terms, over FindProof's translation unit without its main.
$(BIN)/test_atp_cli_deep: tests/atp/test_atp_cli_deep.c tests/atp/deep_stack.h tools/thvm_atp_cli.c \
    tools/proof_check.c $(wildcard tools/atp_cli/*.c) \
    tests/atp/test_atp_wolfram_bench.c $(SRC) | $(BIN)
	$(CC) $(CFLAGS) $(TEST_DEFINES) $(ATP_DEFINES) \
	  -DTHVM_ATP_AC -DATP_MNF \
	  -o $@ $< $(TEST_LDFLAGS)

# FindProof: WL-independent ATP CLI (the CASC UEQ entry) with the
# TFindProof-Method knob surface over the same engine + configuration
# path as the bench (includes its TU, so a CLI run with equal knobs is
# byte-identical to a bench run).  bin/thvm-atp is a compatibility
# alias for existing harnesses.
$(BIN)/FindProof: tools/thvm_atp_cli.c tools/proof_check.c $(wildcard tools/atp_cli/*.c) tests/atp/test_atp_wolfram_bench.c $(SRC) | $(BIN)
	$(CC) $(CFLAGS) $(TEST_DEFINES) $(ATP_DEFINES) \
	  -DTHVM_ATP_AC -DATP_MNF \
	  -o $@ $< $(TEST_LDFLAGS)

$(BIN)/thvm-atp: $(BIN)/FindProof
	cp $< $@

# Problem features phi(p) and the static gate replica: the FindProof TU
# with its main renamed (FINDPROOF_LIB), the same defines as FindProof.
$(BIN)/test_atp_problem_features: tests/atp/test_atp_problem_features.c tools/thvm_atp_cli.c tools/proof_check.c $(wildcard tools/atp_cli/*.c) tests/atp/test_atp_wolfram_bench.c $(SRC) | $(BIN)
	$(CC) $(CFLAGS) $(TEST_DEFINES) $(ATP_DEFINES) \
	  -DTHVM_ATP_AC -DATP_MNF \
	  -o $@ $< $(TEST_LDFLAGS)

# FindProof --emit-ifeq10 (tools/atp_cli/ifeq10.c, self-contained) against
# TPTP's own -10 files; the source lives outside src/, so it is named here.
$(BIN)/test_atp_ifeq10: tests/atp/test_atp_ifeq10.c tools/atp_cli/ifeq10.c | $(BIN)
	$(CC) $(CFLAGS) -o $@ $<

# The wolfram bench with THVM_ATPFT_UNIFY=1 and AC -- the
# CP-gen path routes through ft_cp.c / ft_unify.c (FT-native cp_visit
# + unifier).  Used for the perf differential vs the Term-side path.
$(BIN)/test_atp_wolfram_bench_ft_unify: tests/atp/test_atp_wolfram_bench.c $(SRC) | $(BIN)
	$(CC) $(CFLAGS) $(TEST_DEFINES) $(ATP_DEFINES) \
	  -DTHVM_ATPFT_ALLOC -DTHVM_ATPFT_CONVERT \
	  -DTHVM_ATPFT_LPO -DTHVM_ATPFT_MATCH \
	  -DTHVM_ATPFT_RULES -DTHVM_ATPFT_NORM \
	  -DTHVM_ATPFT_UNIFY -DTHVM_ATP_AC \
	  -o $@ $< $(TEST_LDFLAGS)

# FT-unifier-enabled ATP regression suite -- the full 135624-assertion
# test_atp run with THVM_ATPFT_UNIFY=1 so atp_overlap_ij routes through
# the AtpFt-native cp_visit + unifier path (ft_cp.c + ft_unify.c).
# Existence proves: same goal trace, same proof-step counts when the FT
# path is on.
$(BIN)/test_atp_ft_unify: tests/atp/test_atp.c $(SRC) | $(BIN)
	$(CC) $(CFLAGS) $(TEST_DEFINES) $(ATP_DEFINES) \
	  -DTHVM_ATPFT_ALLOC -DTHVM_ATPFT_CONVERT \
	  -DTHVM_ATPFT_LPO -DTHVM_ATPFT_MATCH \
	  -DTHVM_ATPFT_RULES -DTHVM_ATPFT_NORM \
	  -DTHVM_ATPFT_UNIFY \
	  -o $@ $< $(TEST_LDFLAGS)

# === Diagnostic probes (tests/atp/probe_*.c) =============================
# Standalone C programs that drive the engine for ad-hoc diagnosis.
# Each prints to stdout/stderr -- no CHECK/TEST_REPORT.  Built with
# the same AC support as test_atp_ac (the probes mostly target AC
# saturation flows).  See each probe's header for usage.
PROBE_SRCS := $(wildcard tests/atp/probe_*.c)
PROBE_BINS := $(patsubst tests/atp/probe_%.c,$(BIN)/probe_%,$(PROBE_SRCS))
$(BIN)/probe_%: tests/atp/probe_%.c $(SRC) | $(BIN)
	$(CC) $(CFLAGS) $(TEST_DEFINES) $(ATP_DEFINES) \
	  -DTHVM_ATP_AC \
	  -o $@ $< $(TEST_LDFLAGS)
.PHONY: probes
probes: $(PROBE_BINS)

# === Twee comparison bench (stage 7.4d) ============================
# Requires `twee` on PATH or at ~/.cabal/bin/twee (install via
# `cabal install twee`).  Walks tests/atp/data/*.pr, converts each
# to TPTP-CNF, runs Twee with the same step budget our ATP uses
# (256), writes build/bench-twee.csv.  Not part of `make test`
# because Twee is an external dependency.
$(BUILD)/bench_twee: bench/bench_twee.c $(SRC) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $<

.PHONY: bench-twee
bench-twee: $(BUILD)/bench_twee
	$(BUILD)/bench_twee
	@echo "Wrote build/bench-twee.csv"

# === Flatterm inner-loop spike + A/B microbench ====================
# Standalone measured spike (atp-wm-perstep): a purpose-built flatterm
# core (match / rewrite-normalize / KBO over contiguous pre-order node
# arrays, no IC traversal) A/B'd against thvm's IC inner loop on the REAL
# harvested Sheffer / AndAssociativity rule set + subject corpus. Asserts
# identical normal-form + KBO verdict on every input, reports IC/flat
# wall-time ratio. Additive; not part of `make test`. Built with the same
# ATP defines as the engine so the harvested rules match the live path.
$(BIN)/bench_flatcore: bench/bench_flatcore.c $(SRC) | $(BIN)
	$(CC) $(CFLAGS) $(ATP_DEFINES) -o $@ $< $(TEST_LDFLAGS)

# WM-FPA microbench: the faithful Waldmeister flatterm + discrimination-
# tree (DSBaum) + NormalformInnermost substrate (src/wmfpa/wmfpa.h) A/B'd
# against thvm's IC normalize on the REAL harvested rule set, at several
# |R| sizes.  Asserts identical normal form on every subject. Additive.
$(BIN)/bench_wmfpa: bench/bench_wmfpa.c src/wmfpa/wmfpa.h $(SRC) | $(BIN)
	$(CC) $(CFLAGS) $(ATP_DEFINES) -o $@ $< $(TEST_LDFLAGS)

# === ATP PGO build =================================================
# Profile-guided WALDMEISTER-preset bench: instrument -> train on three
# representative -auto theorems -> rebuild with the merged profile +
# thin-LTO.  Measured 2026-07-08 (Apple M3 Max): WolframAxioms
# OrAssociativity 17.4s (-O2) -> 14.1s (-19%), RULEADD stream
# byte-identical (side_check 0 div / 0 swaps on the parity battery).
# The profile is machine-local (.pgo/, untracked); re-run after any
# engine change that shifts hot paths.
ATP_PGO_FLAGS := -std=c11 -O3 -mcpu=apple-m1 -Wall -Wextra -Wpedantic \
                 -Wno-unused-function
ATP_PGO_TRAIN := WolframAxioms__OrAssociativity \
                 ShefferAxioms__OrAssociativity \
                 MeredithAxioms__AndAssociativity
.PHONY: atp-pgo
atp-pgo:
	mkdir -p .pgo
	CFLAGS="$(ATP_PGO_FLAGS) -fprofile-instr-generate" \
	  $(MAKE) -B bin/test_atp_wolfram_bench
	for T in $(ATP_PGO_TRAIN); do \
	  LLVM_PROFILE_FILE=.pgo/$$T.profraw \
	  THVM_ATP_RSS_ABORT_MB=5300 THVM_ATP_WALDMEISTER=1 \
	  ATP_BENCH_AUTO_WEIGHTS=1 \
	    timeout 400 bin/test_atp_wolfram_bench \
	      tools/baselines/wm_pr/$$T.pr 2000000 300 >/dev/null 2>&1; \
	done
	xcrun llvm-profdata merge -output=.pgo/atp.profdata .pgo/*.profraw
	CFLAGS="$(ATP_PGO_FLAGS) -flto=thin \
	        -fprofile-instr-use=$(CURDIR)/.pgo/atp.profdata \
	        -Wno-profile-instr-unprofiled -Wno-profile-instr-out-of-date" \
	  $(MAKE) -B bin/test_atp_wolfram_bench
	# The profile is engine-wide (same TU/functions), so it applies to the
	# shipped CASC entry too -- rebuild FindProof with it (measured ~15% on
	# M1: WolframAxioms OrAssociativity 15.3s -> 13.0s, RULEADD byte-identical).
	CFLAGS="$(ATP_PGO_FLAGS) -flto=thin \
	        -fprofile-instr-use=$(CURDIR)/.pgo/atp.profdata \
	        -Wno-profile-instr-unprofiled -Wno-profile-instr-out-of-date" \
	  $(MAKE) -B bin/FindProof
