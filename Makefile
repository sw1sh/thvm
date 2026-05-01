# Makefile - single-TU build. Each test is its own executable that
# #includes ../src/thvm.c. The runtime grows by adding files under src/
# and including them from src/thvm.c - never edit this Makefile to add
# a source file, only to add a test.

CC      ?= clang
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Wno-unused-function
BIN     := bin
BUILD   := build

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
  $(BIN)/test_grad \
  $(BIN)/test_ref \
  $(BIN)/test_mat_op2 \
  $(BIN)/test_wnf_n \
  $(BIN)/test_redex \
  $(BIN)/test_metal_stub \
  $(BIN)/test_expand_axis \
  $(BIN)/test_view_strided \
  $(BIN)/test_buf_pool \
  $(BIN)/test_consumer_count \
  $(BIN)/test_decref_hook \
  $(BIN)/test_view_shrink \
  $(BIN)/test_view_permute \
  $(BIN)/test_view_pad \
  $(BIN)/test_view_flip \
  $(BIN)/test_cpu_free_list \
  $(BIN)/test_slot_reuse \
  $(BIN)/test_heap_rooted_preserve \
  $(BIN)/test_gc_roots \
  $(BIN)/test_gc_mark_term \
  $(BIN)/test_extern_pin \
  $(BIN)/test_realize_classify \
  $(BIN)/test_scalar_graph \
  $(BIN)/test_tile_graph \
  $(BIN)/test_materialize_v2 \
  $(BIN)/test_collapse \
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
  $(BIN)/test_lpo

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
else
  METAL_OBJ      :=
  METAL_LDFLAGS  :=
  METAL_LIBPATH  :=
  METAL_AIRS     :=
  METAL_DEFINES  :=
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

# Pick the newest /Applications/Wolfram*.app unless WOLFRAM_APP is set.
WOLFRAM_APP ?= $(shell ls -d "/Applications/Wolfram"*.app 2>/dev/null | sort -V | tail -1)
WL_INCLUDE   := $(WOLFRAM_APP)/Contents/SystemFiles/IncludeFiles/C

WL_PACLET   := wl/THVMLink
WL_LIB_DIR  := $(WL_PACLET)/LibraryResources/$(WL_PLATFORM)
WL_LIB      := $(WL_LIB_DIR)/THVMLink$(WL_DYLIB_EXT)
WL_SRC      := $(WL_PACLET)/CSource/thvmlink.c
WL_SRC_ATP  := $(WL_PACLET)/CSource/thvmlink_atp.c

$(WL_LIB_DIR):
	@mkdir -p $@

$(WL_LIB): $(WL_SRC) $(WL_SRC_ATP) $(SRC) $(METAL_OBJ) $(METAL_LIBPATH) | $(WL_LIB_DIR)
	@if [ -z "$(WOLFRAM_APP)" ] || [ ! -d "$(WL_INCLUDE)" ]; then \
	  echo "ERROR: Wolfram install not found.  Set WOLFRAM_APP=/Applications/Wolfram*.app"; \
	  exit 1; \
	fi
	$(CC) $(CFLAGS) -fPIC $(WL_DYLIB_FLAGS) \
	  -DACCELERATE_NEW_LAPACK \
	  -I"$(WL_INCLUDE)" \
	  $(if $(METAL_OBJ),-DTHVM_HAS_METAL,) \
	  -o $@ $(WL_SRC) $(METAL_OBJ) $(METAL_LDFLAGS) \
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
TEST_LDFLAGS := $(if $(filter Darwin,$(UNAME_S)),-framework Accelerate,)
TEST_DEFINES := $(if $(filter Darwin,$(UNAME_S)),-DACCELERATE_NEW_LAPACK,)

$(BIN)/test_metal_real: tests/test_metal_real.c $(SRC) $(METAL_OBJ) $(METAL_LIBPATH) | $(BIN)
	$(CC) $(CFLAGS) $(TEST_DEFINES) -DTHVM_HAS_METAL -o $@ $< $(METAL_OBJ) $(METAL_LDFLAGS) $(TEST_LDFLAGS)

$(BIN)/test_%: tests/test_%.c $(SRC) | $(BIN)
	$(CC) $(CFLAGS) $(TEST_DEFINES) -o $@ $< $(TEST_LDFLAGS)

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
