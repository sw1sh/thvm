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
build:
	@mkdir -p build

build/thvm_inline.c: $(SRC) tools/inline_includes.py | build
	python3 tools/inline_includes.py src/thvm.c > $@

build/thvm_runtime_blob.c: build/thvm_inline.c tools/embed_blob.py
	python3 tools/embed_blob.py build/thvm_inline.c thvm_runtime_src > $@

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
	$(CC) -fPIC -O2 \
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
