# Makefile - single-TU build. Each test is its own executable that
# #includes ../src/thvm.c. The runtime grows by adding files under src/
# and including them from src/thvm.c - never edit this Makefile to add
# a source file, only to add a test.

CC      ?= clang
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Wno-unused-function
BIN     := bin
BUILD   := build

# Milestone 8a: -DATP_CP_GRAPH switches the ATP CP set onto the
# IC-native shared SUP-graph representation (src/atp/_.c).  Off by
# default -- the milestone-7 array engine is the regression oracle
# and ships byte-for-byte.  `make ATP_CP_GRAPH=1 bin/test_atp`
# builds the graph path; both flag states must compile and, for 8a,
# produce bit-identical proofs.
ATP_CP_GRAPH ?=
ATP_DEFINES  := $(if $(filter-out 0,$(ATP_CP_GRAPH)),-DATP_CP_GRAPH,)

# Milestone 8b/8e instrumentation.  ATP_NORM_STATS reports the 8b
# normalization-memo sharing ratio; ATP_MATCH_STATS reports the 8e
# shared-traversal multi-match memo sharing ratio + sweep cost.
# Both imply -DATP_CP_GRAPH (the graph path they instrument).
ATP_NORM_STATS  ?=
ATP_MATCH_STATS ?=
ATP_DEFINES     += $(if $(filter-out 0,$(ATP_NORM_STATS)),-DATP_CP_GRAPH -DATP_NORM_STATS,)
ATP_DEFINES     += $(if $(filter-out 0,$(ATP_MATCH_STATS)),-DATP_CP_GRAPH -DATP_MATCH_STATS,)

# Milestone 7d: -DATP_FV_INDEX adds a feature-vector subsumption index
# over the CP queue (and rule set) -- a sound over-approximation that
# turns the O(n_cps) thvm_match scan in atp_cp_queue_subsumed into an
# O(retrieval) candidate lookup + match on the survivors.  ON by
# default: it is part of the canonical engine and ships byte-for-byte
# (same verdict as the array scan, just faster).  Independent of
# -DATP_CP_GRAPH.  `make ATP_FV_INDEX=0 bin/test_atp` builds the
# milestone-7 array scan -- the regression oracle.
ATP_FV_INDEX ?= 1
ATP_DEFINES  += $(if $(filter-out 0,$(ATP_FV_INDEX)),-DATP_FV_INDEX,)

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
# waldmeister/sources/CLAS/Clas_CP_Goal.c).  A CP whose subterms
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

# Waldmeister-style critical-pair classification: -DATP_CP_CLASSIFY
# ports Waldmeister's `NewClassification` ("new classification") and
# `ClasFunctions` ("classification functions") killer predicates
# (KillerR / KillerE / KillerRE / EChild).  Each CP is classified at
# insertion time; a killer CP that is also rule-subsumed is dropped,
# an EChild CP is deprioritized.  The drop is a sound subset of the
# trivially-joinable filter, so saturation status is identical with
# the flag on or off.  OFF by default until benchmarked: build with
# `make ATP_CP_CLASSIFY=1` to opt in.
ATP_CP_CLASSIFY ?=
ATP_DEFINES  += $(if $(filter-out 0,$(ATP_CP_CLASSIFY)),-DATP_CP_CLASSIFY,)

# ATP auto-precedence (Waldmeister PhilMarlow / Praezedenzgenerator
# port): -DATP_AUTO_PREC makes the ATP bench harness + WL glue
# replace the syntactic `precedence[i]=i+1` default with a
# precedence derived from per-operator algebraic-property analysis
# (commutativity / associativity / idempotence / units / inverses /
# distributivity).  OFF by default until benchmarked: build with
# `make ATP_AUTO_PREC=1` to opt in.  See src/atp/precedence.c.
ATP_AUTO_PREC ?=
ATP_DEFINES  += $(if $(filter-out 0,$(ATP_AUTO_PREC)),-DATP_AUTO_PREC,)

TESTS := \
  $(BIN)/test_term \
  $(BIN)/test_heap \
  $(BIN)/test_app_lam \
  $(BIN)/test_era \
  $(BIN)/test_dup_sup \
  $(BIN)/test_dup_lam \
  $(BIN)/test_lam_era \
  $(BIN)/test_tensor \
  $(BIN)/test_uop \
  $(BIN)/test_uop_index \
  $(BIN)/test_uop_index_simplify \
  $(BIN)/test_uop_range_axis_type \
  $(BIN)/test_uop_movement_index \
  $(BIN)/test_uop_graph_rewrite \
  $(BIN)/test_uop_upat \
  $(BIN)/test_uop_range_axis_type \
  $(BIN)/test_grad \
  $(BIN)/test_ref \
  $(BIN)/test_mat_op2 \
  $(BIN)/test_wnf_n \
  $(BIN)/test_redex \
  $(BIN)/test_step_incremental \
  $(BIN)/test_metal_stub \
  $(BIN)/test_expand_axis \
  $(BIN)/test_view_strided \
  $(BIN)/test_pool_im2col_chain \
  $(BIN)/test_buf_pool \
  $(BIN)/test_consumer_count \
  $(BIN)/test_kernel_fire_gen \
  $(BIN)/test_view_shrink \
  $(BIN)/test_view_permute \
  $(BIN)/test_view_pad \
  $(BIN)/test_view_flip \
  $(BIN)/test_cpu_free_list \
  $(BIN)/test_cpu_jit_via_uop \
  $(BIN)/test_slot_reuse \
  $(BIN)/test_heap_rooted_preserve \
  $(BIN)/test_gc_roots \
  $(BIN)/test_gc_mark_term \
  $(BIN)/test_extern_pin \
  $(BIN)/test_bufferize_classify \
  $(BIN)/test_rangeify_unified \
  $(BIN)/test_bufferize \
  $(BIN)/test_uop_buffer \
  $(BIN)/test_uop_store_after \
  $(BIN)/test_uop_opt \
  $(BIN)/test_uop_recognise_tc \
  $(BIN)/test_apply_opt_dag \
  $(BIN)/test_uop_recognise_conv \
  $(BIN)/test_render_uop \
  $(BIN)/test_render_uop_metal \
  $(BIN)/test_render_uop_cuda \
  $(BIN)/test_materialize_v2 \
  $(BIN)/test_collapse \
  $(BIN)/test_cnf \
  $(BIN)/test_auto_dup \
  $(BIN)/test_eql \
  $(BIN)/test_dup_num \
  $(BIN)/test_and_or \
  $(BIN)/test_any \
  $(BIN)/test_inc \
  $(BIN)/test_ctr \
  $(BIN)/test_when \
  $(BIN)/test_kbo \
  $(BIN)/test_rewrite \
  $(BIN)/test_unify \
  $(BIN)/test_cp \
  $(BIN)/test_icc \
  $(BIN)/test_wald \
  $(BIN)/test_atp \
  $(BIN)/test_atp_analysis \
  $(BIN)/test_atp_enigma \
  $(BIN)/test_bench_atp \
  $(BIN)/test_pri \
  $(BIN)/test_app_sup \
  $(BIN)/test_sup_cps \
  $(BIN)/test_kbo_pri \
  $(BIN)/test_rewrite_pri \
  $(BIN)/test_sup_rewrite \
  $(BIN)/test_lpo \
  $(BIN)/test_dyn_lab \
  $(BIN)/test_wsq \
  $(BIN)/test_wspq \
  $(BIN)/test_wnf_pool \
  $(BIN)/test_nf_pool \
  $(BIN)/test_heap_atomic \
  $(BIN)/test_heap_atomic_mt \
  $(BIN)/test_wnf_pool_mt \
  $(BIN)/test_context_wnf_state \
  $(BIN)/test_pool_profile \
  $(BIN)/test_step_saturation \
  $(BIN)/test_bench_tree \
  $(BIN)/test_bend_tree_sum \
  $(BIN)/test_aot_emit \
  $(BIN)/test_aot_e2e \
  $(BIN)/test_aot_e2e_bench \
  $(BIN)/test_aot_build \
  $(BIN)/test_multi_trace \
  $(BIN)/test_multi_trace_on

# === Metal backend (Darwin only) =====================================
# src/backend/metal/_.m compiles separately into build/backend_metal.o.
# Tests / dylib that #define THVM_HAS_METAL link this object + the
# Metal frameworks; src/thvm.c then skips the C-side stub.
ifeq ($(shell uname -s),Darwin)
  METAL_OBJ      := $(BUILD)/backend_metal.o
  METAL_LDFLAGS  := -framework Metal -framework Foundation
  METAL_LIBPATH  := $(BUILD)/default.metallib
  METAL_SHADERS  := $(wildcard src/backend/metal/shaders/*.metal)
  METAL_AIRS     := $(METAL_SHADERS:src/backend/metal/shaders/%.metal=$(BUILD)/%.air)
  METAL_DEFINES  := -DTHVM_METAL_METALLIB='"$(METAL_LIBPATH)"'
  TESTS          += $(BIN)/test_metal_real
  TESTS          += $(BIN)/test_aot_metal
  TESTS          += $(BIN)/test_aot_metal_run
else
  METAL_OBJ      :=
  METAL_LDFLAGS  :=
  METAL_LIBPATH  :=
  METAL_AIRS     :=
  METAL_DEFINES  :=
endif

# === CUDA backend (Linux + CUDA present) =============================
# Mirror of the Darwin Metal block above.  Unlike Metal (a separate
# Objective-C .o), the CUDA backend is plain C99 -- the driver API and
# nvrtc are C -- so it is #included straight into src/thvm.c, gated by
# -DTHVM_HAS_CUDA.  This block enables it ONLY on Linux with the CUDA
# headers found; the macOS build never sees -DTHVM_HAS_CUDA and is left
# completely untouched.
#
# CUDA_HOME is auto-detected (override on the make line if elsewhere).
# The CUDA driver lib (libcuda) ships with the GPU driver and lives in
# the system lib dir; libnvrtc lives in the toolkit.  Both -L paths are
# passed so a driver-only image (libcuda in /usr/lib, nvrtc symlinked
# there per docs/plans/cuda_backend.md) and a full-toolkit image both
# link.
# NB: uname is queried inline via $(shell ...) -- the same idiom the
# Metal block uses -- because the shared UNAME_S variable is not
# assigned until further down the file.
CUDA_HOME ?= $(firstword $(wildcard /usr/local/cuda /usr/local/cuda-12.4 /usr/local/cuda-12) /usr/local/cuda)
ifeq ($(shell uname -s),Linux)
  ifneq ($(wildcard $(CUDA_HOME)/include/cuda.h),)
    HAVE_CUDA      := 1
    CUDA_DEFINES   := -DTHVM_HAS_CUDA -I$(CUDA_HOME)/include
    CUDA_LDFLAGS   := -L$(CUDA_HOME)/lib64 -L$(CUDA_HOME)/lib64/stubs \
                      -L/usr/lib/x86_64-linux-gnu -lcuda -lnvrtc
    TESTS          += $(BIN)/test_cuda_backend
  endif
endif

# Every C and header file under src/, plus the test harness header.
# Used as a prerequisite by both the C tests and the WL bridge so any
# runtime change retriggers a rebuild.
SRC := $(shell find src -name '*.c' -o -name '*.h') tests/test.h

.PHONY: all test clean wl wl-test wl-examples bench-twee
all: $(TESTS)

# === Wolfram Language paclet (LibraryLink bridge) ===
# Builds wl/THVMLink/LibraryResources/$(WL_PLATFORM)/THVMLink.dylib (or .so).
#
# Override with WOLFRAM_APP=... if Wolfram is installed elsewhere.

UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)
ifeq ($(UNAME_S),Darwin)
  ifeq ($(UNAME_M),arm64)
    WL_PLATFORM ?= MacOSX-ARM64
  else
    WL_PLATFORM ?= MacOSX-x86-64
  endif
  WL_DYLIB_EXT := .dylib
  WL_DYLIB_FLAGS := -dynamiclib
else
  WL_PLATFORM ?= Linux-$(UNAME_M)
  WL_DYLIB_EXT := .so
  WL_DYLIB_FLAGS := -shared
endif

# Pick the newest /Applications/Wolfram*.app that ships the LibraryLink
# C headers, unless WOLFRAM_APP is set.  Filtering on IncludeFiles/C
# skips co-installed non-kernel apps (Wolfram VPN.app, WolframScript.app)
# that would otherwise win `sort -V | tail -1` and break `make wl`.
WOLFRAM_APP ?= $(shell ls -d "/Applications/Wolfram"*.app 2>/dev/null | sort -V | while IFS= read -r d; do [ -d "$$d/Contents/SystemFiles/IncludeFiles/C" ] && printf '%s\n' "$$d"; done | tail -1)
WL_INCLUDE   := $(WOLFRAM_APP)/Contents/SystemFiles/IncludeFiles/C

WL_PACLET   := wl/THVMLink
WL_LIB_DIR  := $(WL_PACLET)/LibraryResources/$(WL_PLATFORM)
WL_LIB      := $(WL_LIB_DIR)/THVMLink$(WL_DYLIB_EXT)
WL_SRC      := $(WL_PACLET)/CSource/thvmlink.c
WL_SRC_ATP  := $(WL_PACLET)/CSource/thvmlink_atp.c

$(WL_LIB_DIR):
	@mkdir -p $@

# WL dylib defaults to the trace-enabled variant so TMulticompTrace
# works out of the box (see wl/THVMLink/Kernel/Multicomputation.wl).
# Override with `WL_TRACE=0 make wl` to get the byte-identical
# pre-trace dylib (e.g. for benching) -- the runtime flag is still
# the dominant gate, so trace-on costs one well-predicted branch
# per interaction even in a trace-enabled dylib.
# -DTHVM_TRACE grows TContext (new fields at the END of the struct,
# so backend_metal.o built without it is fine -- it only touches
# fields before them).  AOT modules dlopen'd by this dylib pick up
# -DTHVM_TRACE automatically (src/aot/build.c reads `#ifdef
# THVM_TRACE`).  C test binaries never get -DTHVM_TRACE;
# bin/test_multi_trace_on adds it explicitly, on its own.
WL_TRACE      ?= 1
WL_TRACE_DEF  := $(if $(filter-out 0,$(WL_TRACE)),-DTHVM_TRACE,)

$(WL_LIB): $(WL_SRC) $(WL_SRC_ATP) $(SRC) $(METAL_OBJ) $(METAL_LIBPATH) build/thvm_runtime_blob.c | $(WL_LIB_DIR)
	@if [ -z "$(WOLFRAM_APP)" ] || [ ! -d "$(WL_INCLUDE)" ]; then \
	  echo "ERROR: Wolfram install not found.  Set WOLFRAM_APP=/Applications/Wolfram*.app"; \
	  exit 1; \
	fi
	$(CC) $(CFLAGS) -fPIC $(WL_DYLIB_FLAGS) \
	  -DACCELERATE_NEW_LAPACK \
	  $(WL_TRACE_DEF) \
	  $(ATP_DEFINES) \
	  $(WL_ATP_DEFINES) \
	  -I"$(WL_INCLUDE)" \
	  $(if $(METAL_OBJ),-DTHVM_HAS_METAL,) \
	  -o $@ $(WL_SRC) build/thvm_runtime_blob.c $(METAL_OBJ) $(METAL_LDFLAGS) \
	  $(if $(filter Darwin,$(UNAME_S)),-framework Accelerate,)
ifeq ($(UNAME_S),Darwin)
	codesign --force --sign - $@
endif

wl: $(WL_LIB)

wl-test: $(WL_LIB)
	wolframscript -f $(WL_PACLET)/Tests/run.wls

# Run + render every wl/Examples/<id>/term.wl, writing term.png next
# to each.  Append EXAMPLE=<id> to render just one.
wl-examples: $(WL_LIB)
	wolframscript -f wl/Examples/run.wls $(EXAMPLE)

# For each wl/Examples/<id>/ that ships both term.wl and
# term-reduced.wl, verify TTermTree[TWnf[term.wl]] === TTermTree[term-reduced.wl].
wl-examples-test: $(WL_LIB)
	wolframscript -f wl/Examples/test_reductions.wls

# === Cross-compiled paclet libraries (all five Wolfram platforms) ====
# `make wl-cross` populates wl/THVMLink/LibraryResources/<SystemID>/ for
# every typical Wolfram $SystemID, so PacletPack bundles one paclet that
# installs everywhere.  Then publish + round-trip:
#     wolframscript -f tools/publish_paclet.wls    # -> public CloudObject
#     wolframscript -f tools/verify_install.wls    # PacletInstall + Needs
#
#   MacOSX-ARM64 + MacOSX-x86-64   native clang, full Metal GPU
#                                  (one universal2 dylib in both dirs)
#   Linux-x86-64 + Linux-ARM64     zig cc, CPU-only .so
#   Windows-x86-64                 zig cc, CPU-only .dll
#
# Off-Apple builds are CPU-only: Metal is Apple Objective-C and CUDA is
# Linux+toolkit-gated, neither cross-compiles from a Mac.  The WL
# LibraryLink C headers are platform-independent, so $(WL_INCLUDE) is
# reused for every target.  -D_GNU_SOURCE is passed on the command line
# (not left to thvm.h) because WolframLibrary.h is included before
# thvm.h sets its POSIX feature-test macro, so the macro would land too
# late for the Linux glibc headers.
ZIG          ?= zig
WL_RES       := $(WL_PACLET)/LibraryResources
WL_XCC_FLAGS := -std=c11 -O2 -w -D_GNU_SOURCE -I"$(WL_INCLUDE)"

# Per-format runtime blobs: the embedded thvm source is .incbin'd into
# every per-platform library, and the host object must match the
# linker's format (Mach-O native, ELF Linux, COFF Windows).
build/thvm_runtime_blob_elf.c: build/thvm_inline.c tools/embed_blob.py
	python3 tools/embed_blob.py build/thvm_inline.c thvm_runtime_src elf > $@
build/thvm_runtime_blob_coff.c: build/thvm_inline.c tools/embed_blob.py
	python3 tools/embed_blob.py build/thvm_inline.c thvm_runtime_src coff > $@

.PHONY: wl-cross wl-mac wl-linux-x64 wl-linux-arm64 wl-win-x64

wl-cross: wl-mac wl-linux-x64 wl-linux-arm64 wl-win-x64
	@echo "[wl-cross] all five platform libraries are under $(WL_RES)/"

# macOS: one universal2 (arm64 + x86_64) dylib with Metal, placed in
# both MacOSX SystemID dirs.  Must run on a Mac (xcrun metal toolchain).
# Mirrors the $(WL_LIB) recipe but dual-arch; ships default.metallib
# alongside so the GPU path (DEV=metal) works post-install -- metal_init
# resolves it relative to the loaded dylib (see backend/metal/_.m).
wl-mac: $(WL_SRC) $(SRC) $(METAL_LIBPATH) build/thvm_runtime_blob.c
	@if [ -z "$(WOLFRAM_APP)" ] || [ ! -d "$(WL_INCLUDE)" ]; then \
	  echo "ERROR: Wolfram install not found.  Set WOLFRAM_APP=/Applications/Wolfram*.app"; exit 1; fi
	@mkdir -p $(WL_RES)/MacOSX-ARM64 $(WL_RES)/MacOSX-x86-64
	clang -fobjc-arc -O2 -arch arm64 -arch x86_64 $(METAL_DEFINES) \
	  -c -o build/backend_metal_univ.o src/backend/metal/_.m
	$(CC) -std=c11 -O2 -w -fPIC -dynamiclib -arch arm64 -arch x86_64 \
	  -DACCELERATE_NEW_LAPACK $(WL_TRACE_DEF) -DTHVM_HAS_METAL \
	  -I"$(WL_INCLUDE)" \
	  -o $(WL_RES)/MacOSX-ARM64/THVMLink.dylib \
	  $(WL_SRC) build/thvm_runtime_blob.c build/backend_metal_univ.o \
	  -framework Metal -framework Foundation -framework Accelerate
	codesign --force --sign - $(WL_RES)/MacOSX-ARM64/THVMLink.dylib
	cp $(WL_RES)/MacOSX-ARM64/THVMLink.dylib $(WL_RES)/MacOSX-x86-64/THVMLink.dylib
	cp $(METAL_LIBPATH) $(WL_RES)/MacOSX-ARM64/default.metallib
	cp $(METAL_LIBPATH) $(WL_RES)/MacOSX-x86-64/default.metallib

wl-linux-x64: build/thvm_runtime_blob_elf.c $(WL_SRC) $(SRC)
	@mkdir -p $(WL_RES)/Linux-x86-64
	$(ZIG) cc -target x86_64-linux-gnu $(WL_XCC_FLAGS) -fPIC -shared \
	  -o $(WL_RES)/Linux-x86-64/THVMLink.so \
	  $(WL_SRC) build/thvm_runtime_blob_elf.c -lm

wl-linux-arm64: build/thvm_runtime_blob_elf.c $(WL_SRC) $(SRC)
	@mkdir -p $(WL_RES)/Linux-ARM64
	$(ZIG) cc -target aarch64-linux-gnu $(WL_XCC_FLAGS) -fPIC -shared \
	  -o $(WL_RES)/Linux-ARM64/THVMLink.so \
	  $(WL_SRC) build/thvm_runtime_blob_elf.c -lm

wl-win-x64: build/thvm_runtime_blob_coff.c $(WL_SRC) $(SRC)
	@mkdir -p $(WL_RES)/Windows-x86-64
	$(ZIG) cc -target x86_64-windows-gnu $(WL_XCC_FLAGS) -shared \
	  -o $(WL_RES)/Windows-x86-64/THVMLink.dll \
	  $(WL_SRC) build/thvm_runtime_blob_coff.c


$(BIN):
	@mkdir -p $@

$(BUILD):
	@mkdir -p $@

# Metal backend object: compiled from .m with ARC, links Metal +
# Foundation frameworks at the per-binary link step.
$(BUILD)/backend_metal.o: src/backend/metal/_.m src/thvm.h | $(BUILD)
	clang -fobjc-arc -O2 -c $(METAL_DEFINES) -o $@ $<

# Per-shader compile: .metal -> .air.
$(BUILD)/%.air: src/backend/metal/shaders/%.metal | $(BUILD)
	xcrun -sdk macosx metal -c $< -o $@

# Link all .air files into a single default.metallib.
$(METAL_LIBPATH): $(METAL_AIRS) | $(BUILD)
	xcrun -sdk macosx metallib $(METAL_AIRS) -o $@

# test_metal_real opts into the dual-TU build: -DTHVM_HAS_METAL
# tells src/thvm.c to skip the C stub and instead link the .o.
# Depends on the metallib so metal_init can find it at runtime.
# Darwin folds libm into libSystem; Linux needs an explicit -lm (the
# UOp walker / renderer call exp2/log2/sqrt).
TEST_LDFLAGS := $(if $(filter Darwin,$(UNAME_S)),-framework Accelerate,-lm)
TEST_DEFINES := $(if $(filter Darwin,$(UNAME_S)),-DACCELERATE_NEW_LAPACK,)

# === Embedded thvm runtime source ==================================
# At build time, flatten src/thvm.c (resolve all `#include "..."`,
# leave system `<...>` headers alone) and embed the resulting source
# via the assembler's .incbin directive in build/thvm_runtime_blob.c.
# AOT compile reads this blob at runtime, prepends it to the per-
# program emit, and clang-compiles -- so the AOT pipeline doesn't
# depend on src/ being on disk (paclet ships standalone).
#
# Per-call compile is ~3-4 sec the first time per def-shape;
# cache-by-content (build.c FNV hash on the wrapped source) catches
# repeat calls so subsequent TAOTCompile is instant.
build/thvm_inline.c: $(SRC) tools/inline_includes.py | $(BUILD)
	python3 tools/inline_includes.py src/thvm.c > $@

build/thvm_runtime_blob.c: build/thvm_inline.c tools/embed_blob.py
	python3 tools/embed_blob.py build/thvm_inline.c thvm_runtime_src macho > $@

# AOT-using test binaries link build/thvm_runtime_blob.c so the
# extern thvm_runtime_src symbol resolves.
AOT_TESTS := $(BIN)/test_aot_emit $(BIN)/test_aot_e2e \
             $(BIN)/test_aot_e2e_bench $(BIN)/test_aot_build

$(AOT_TESTS): $(BIN)/test_%: tests/test_%.c $(SRC) build/thvm_runtime_blob.c | $(BIN)
	$(CC) $(CFLAGS) $(TEST_DEFINES) -o $@ $< build/thvm_runtime_blob.c $(TEST_LDFLAGS)

$(BIN)/test_metal_real: tests/test_metal_real.c $(SRC) $(METAL_OBJ) $(METAL_LIBPATH) | $(BIN)
	$(CC) $(CFLAGS) $(TEST_DEFINES) -DTHVM_HAS_METAL -o $@ $< $(METAL_OBJ) $(METAL_LDFLAGS) $(TEST_LDFLAGS)

$(BIN)/test_aot_metal: tests/test_aot_metal.c $(SRC) $(METAL_OBJ) $(METAL_LIBPATH) | $(BIN)
	$(CC) $(CFLAGS) $(TEST_DEFINES) -DTHVM_HAS_METAL -o $@ $< $(METAL_OBJ) $(METAL_LDFLAGS) $(TEST_LDFLAGS)

$(BIN)/test_aot_metal_run: tests/test_aot_metal_run.c $(SRC) $(METAL_OBJ) $(METAL_LIBPATH) | $(BIN)
	$(CC) $(CFLAGS) $(TEST_DEFINES) -DTHVM_HAS_METAL -o $@ $< $(METAL_OBJ) $(METAL_LDFLAGS) $(TEST_LDFLAGS)

# CUDA end-to-end test: -DTHVM_HAS_CUDA pulls backend/cuda/ into the
# single-TU build; -lcuda -lnvrtc link the driver + nvrtc.  Only
# reachable when the CUDA block above added it to TESTS (Linux+CUDA).
$(BIN)/test_cuda_backend: tests/test_cuda_backend.c $(SRC) | $(BIN)
	$(CC) $(CFLAGS) $(TEST_DEFINES) $(CUDA_DEFINES) -o $@ $< $(CUDA_LDFLAGS) $(TEST_LDFLAGS)

# Cross-backend dispatch microbench.  One binary; the backend is chosen
# at runtime via DEV={cpu,metal,cuda}.  On macOS it links the Metal
# backend (so DEV=cpu and DEV=metal both work); on Linux+CUDA it links
# the CUDA backend (DEV=cpu and DEV=cuda).
ifeq ($(UNAME_S),Darwin)
$(BIN)/xbackend_bench: tools/xbackend_bench.c $(SRC) $(METAL_OBJ) $(METAL_LIBPATH) | $(BIN)
	$(CC) $(CFLAGS) $(TEST_DEFINES) $(if $(METAL_OBJ),-DTHVM_HAS_METAL,) -o $@ $< $(METAL_OBJ) $(METAL_LDFLAGS) $(TEST_LDFLAGS)
else
$(BIN)/xbackend_bench: tools/xbackend_bench.c $(SRC) | $(BIN)
	$(CC) $(CFLAGS) $(TEST_DEFINES) $(CUDA_DEFINES) -o $@ $< $(CUDA_LDFLAGS) $(TEST_LDFLAGS)
endif

# Multicomputation trace -- built TWICE from the same source so we
# can verify both halves of the gating discipline.  test_multi_trace
# uses default CFLAGS (no -DTHVM_TRACE), so the multi_emit() call sites
# in src/interact/*.c compile to ((void)0) and src/instrument/multi.c
# is empty.  test_multi_trace_on adds -DTHVM_TRACE, so the runtime
# flag and all multi_trace_* API are present.  Both binaries must
# pass; see docs/plans/multicomputation_trace.md.
$(BIN)/test_multi_trace_on: tests/test_multi_trace.c $(SRC) | $(BIN)
	$(CC) $(CFLAGS) $(TEST_DEFINES) -DTHVM_TRACE -o $@ $< $(TEST_LDFLAGS)

$(BIN)/test_%: tests/test_%.c $(SRC) | $(BIN)
	$(CC) $(CFLAGS) $(TEST_DEFINES) $(ATP_DEFINES) -o $@ $< $(TEST_LDFLAGS)

# === py/ ctypes bindings (libthvm_py.{dylib,so}) =====================
# Single-TU build of src/thvm.c + extern-C wrapper that re-exports the
# static-inline UOp constructors, so ctypes can drive thvm from Python.
# Two platform variants, both producing one `py` target:
#
#   macOS:  libthvm_py.dylib  =  thvm_py.c (.o) + thvm_py_metal.m (.o)
#                                + Accelerate/Metal/Foundation frameworks
#   Linux:  libthvm_py.so     =  thvm_py.c (.o) + thvm_py_cuda.c (.o)
#                                + -lcuda -lnvrtc   (when CUDA present)
#
# The macOS block is guarded by `uname -s == Darwin` and is untouched
# by the Linux addition below; the Linux block only emits a CUDA bridge
# when the CUDA headers were found (HAVE_CUDA from the block above).
ifeq ($(shell uname -s),Darwin)
PY_DYLIB        := py/thvm/libthvm_py.dylib
PY_THVM_OBJ     := $(BUILD)/py_thvm.o
PY_METAL_OBJ    := $(BUILD)/py_thvm_metal.o
$(PY_THVM_OBJ): py/csource/thvm_py.c $(SRC) | $(BUILD)
	clang -fPIC -O2 -DACCELERATE_NEW_LAPACK \
	    -Wno-unused-function -Wno-unused-variable -Wno-int-conversion \
	    -c -o $@ $<
$(PY_METAL_OBJ): py/csource/thvm_py_metal.m | $(BUILD)
	clang -fPIC -fobjc-arc -O2 -c -o $@ $<
$(PY_DYLIB): $(PY_THVM_OBJ) $(PY_METAL_OBJ)
	clang -shared -framework Accelerate -framework Metal -framework Foundation \
	    -o $@ $^
.PHONY: py
py: $(PY_DYLIB)
endif

# Linux py build: libthvm_py.so.  thvm_py.c carries the UOp builder +
# both render entry points (the CUDA renderer is plain C, always
# compiled in); thvm_py_cuda.c is the in-process nvrtc + driver bridge,
# linked only when HAVE_CUDA.  Without CUDA the .so still builds (UOp
# construction + render-to-string), the `Cuda` class just stays
# unavailable -- mirroring how the macOS .dylib needs no GPU to render.
ifeq ($(shell uname -s),Linux)
PY_SO           := py/thvm/libthvm_py.so
PY_THVM_OBJ     := $(BUILD)/py_thvm.o
$(PY_THVM_OBJ): py/csource/thvm_py.c $(SRC) | $(BUILD)
	$(CC) -fPIC -O2 $(CUDA_DEFINES) \
	    -Wno-unused-function -Wno-unused-variable -Wno-int-conversion \
	    -c -o $@ $<
ifdef HAVE_CUDA
PY_CUDA_OBJ     := $(BUILD)/py_thvm_cuda.o
$(PY_CUDA_OBJ): py/csource/thvm_py_cuda.c | $(BUILD)
	$(CC) -fPIC -O2 $(CUDA_DEFINES) -c -o $@ $<
$(PY_SO): $(PY_THVM_OBJ) $(PY_CUDA_OBJ)
	$(CC) -shared -o $@ $^ $(CUDA_LDFLAGS) -lm
else
$(PY_SO): $(PY_THVM_OBJ)
	$(CC) -shared -o $@ $^ -lm
endif
.PHONY: py
py: $(PY_SO)
endif

test: $(TESTS)
	@fail=0; \
	for t in $(TESTS); do \
	  printf '%s\n' "> $$t"; \
	  $$t || fail=1; \
	done; \
	exit $$fail

clean:
	rm -rf $(BIN)

# === Twee comparison bench (stage 7.4d) ============================
# Requires `twee` on PATH or at ~/.cabal/bin/twee (install via
# `cabal install twee`).  Walks tests/data/atp/*.pr, converts each
# to TPTP-CNF, runs Twee with the same step budget our ATP uses
# (256), writes build/bench-twee.csv.  Not part of `make test`
# because Twee is an external dependency.
$(BUILD)/bench_twee: tools/bench_twee.c $(SRC) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $<

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
$(BIN)/bench_flatcore: tools/bench_flatcore.c $(SRC) | $(BIN)
	$(CC) $(CFLAGS) $(ATP_DEFINES) -o $@ $< $(TEST_LDFLAGS)

# WM-FPA microbench: the faithful Waldmeister flatterm + discrimination-
# tree (DSBaum) + NormalformInnermost substrate (src/wmfpa/wmfpa.h) A/B'd
# against thvm's IC normalize on the REAL harvested rule set, at several
# |R| sizes.  Asserts identical normal form on every subject. Additive.
$(BIN)/bench_wmfpa: tools/bench_wmfpa.c src/wmfpa/wmfpa.h $(SRC) | $(BIN)
	$(CC) $(CFLAGS) $(ATP_DEFINES) -o $@ $< $(TEST_LDFLAGS)
