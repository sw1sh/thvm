# Makefile - single-TU build. Each test is its own executable that
# #includes src/thvm.c. The runtime grows by adding files under src/
# and including them from src/thvm.c - never edit this Makefile to add
# a source file, only to add a test. The ATP track's tests (tests/atp/),
# CLI and benches are in atp.mk.

CC      ?= clang
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Wno-unused-function
BIN     := bin
BUILD   := build

SRC := $(shell find src -name '*.c' -o -name '*.h') tests/test.h tests/kernel_harness.h tests/tu_probes.h

# The ATP track: its build defines, tests, CLI and benches.
include atp.mk

TESTS := \
  $(BIN)/test_term \
  $(BIN)/test_heap \
  $(BIN)/test_app_lam \
  $(BIN)/test_era \
  $(BIN)/test_dup_sup \
  $(BIN)/test_dup_ctr \
  $(BIN)/test_dup_lam \
  $(BIN)/test_lam_era \
  $(BIN)/test_lam_shape \
  $(BIN)/test_tensor \
  $(BIN)/test_disk_tensor \
  $(BIN)/test_uop \
  $(BIN)/test_uop_index \
  $(BIN)/test_uop_index_simplify \
  $(BIN)/test_uop_reduce_collapse \
  $(BIN)/test_uop_graph_rewrite \
  $(BIN)/test_uop_symbolic_shape \
  $(BIN)/test_uop_const_fold_dtype \
  $(BIN)/test_uop_walk_cast \
  $(BIN)/test_uop_walk_c_types \
  $(BIN)/test_fp8_nonfinite \
  $(BIN)/test_assign_dtypes \
  $(BIN)/test_fp16_convert \
  $(BIN)/test_sym_reshape \
  $(BIN)/test_sym_expand \
  $(BIN)/test_sym_gather \
  $(BIN)/test_sym_cmask \
  $(BIN)/test_sym_attn_causal \
  $(BIN)/test_sym_attn_block \
  $(BIN)/test_sym_repeat \
  $(BIN)/test_sym_kvcache \
  $(BIN)/test_sym_kvcache_append \
  $(BIN)/test_sym_kvcache_jit \
  $(BIN)/test_sym_kvbegin_read \
  $(BIN)/test_sym_decode_step \
  $(BIN)/test_sym_pad \
  $(BIN)/test_kvar_clamp \
  $(BIN)/test_sym_m2 \
  $(BIN)/test_sym_matmul \
  $(BIN)/test_matvec_blas \
  $(BIN)/test_lnmatmul_fuse \
  $(BIN)/test_fuse_matmul_input_cpu \
  $(BIN)/test_sym_inner \
  $(BIN)/test_sym_inner_realize \
  $(BIN)/test_sym_attn \
  $(BIN)/test_sym_attn_lazy \
  $(BIN)/test_sym_split_fuse \
  $(BIN)/test_lit_split_fuse \
  $(BIN)/test_sym_pad_concat \
  $(BIN)/test_sym_scores_fuse \
  $(BIN)/test_sym_mha_bisect \
  $(BIN)/test_uop_upat \
  $(BIN)/test_grad \
  $(BIN)/test_iwhere_grad \
  $(BIN)/test_grad_gather_scatter \
  $(BIN)/test_grad_scatter_max \
  $(BIN)/test_ref \
  $(BIN)/test_mat_op2 \
  $(BIN)/test_wnf_n \
  $(BIN)/test_redex \
  $(BIN)/test_step_incremental \
  $(BIN)/test_metal_stub \
  $(BIN)/test_expand_axis \
  $(BIN)/test_view_strided \
  $(BIN)/test_view_chain_merge \
  $(BIN)/test_pool_im2col_chain \
  $(BIN)/test_buf_pool \
  $(BIN)/test_consumer_count \
  $(BIN)/test_kernel_fire_gen \
  $(BIN)/test_custom_kernel \
  $(BIN)/test_realize_many_war \
  $(BIN)/test_realize_abort \
  $(BIN)/test_autotune_correctness_gate \
  $(BIN)/test_autotune_iso_scratch \
  $(BIN)/test_autotune_iso_extent \
  $(BIN)/test_uop_content_hash \
  $(BIN)/test_view_shrink \
  $(BIN)/test_view_permute \
  $(BIN)/test_view_pad \
  $(BIN)/test_shrink_over_pad \
  $(BIN)/test_view_flip \
  $(BIN)/test_cpu_free_list \
  $(BIN)/test_cpu_jit_warmup \
  $(BIN)/test_cpu_jit_cache \
  $(BIN)/test_cpu_jit_via_uop \
  $(BIN)/test_cpu_jit_nonfinite \
  $(BIN)/test_cpu_jit_f64_reduce \
  $(BIN)/test_uop_walk_domain \
  $(BIN)/test_int_reduce_exact \
  $(BIN)/test_slot_reuse \
  $(BIN)/test_heap_rooted_preserve \
  $(BIN)/test_heap_recovery \
  $(BIN)/test_gc_mark_term \
  $(BIN)/test_gc_node_size \
  $(BIN)/test_book_def_gc \
  $(BIN)/test_term_eq \
  $(BIN)/test_alo_gc \
  $(BIN)/test_kernel_gc_unfired \
  $(BIN)/test_extern_pin \
  $(BIN)/test_bufferize_classify \
  $(BIN)/test_rangeify_unified \
  $(BIN)/test_expand_shared_view \
  $(BIN)/test_nn_conv \
  $(BIN)/test_nn_ops \
  $(BIN)/test_shared_weight_levels \
  $(BIN)/test_uop_mov_cache_growth \
  $(BIN)/test_hashcons_realize \
  $(BIN)/test_limit_bufs \
  $(BIN)/test_kernel_width \
  $(BIN)/test_uop_buffer \
  $(BIN)/test_uop_store_after \
  $(BIN)/test_uop_substitute \
  $(BIN)/test_after_root_kernel \
  $(BIN)/test_uop_gather_scatter_ir \
  $(BIN)/test_uop_gather \
  $(BIN)/test_uop_scatter \
  $(BIN)/test_uop_scatter_max \
  $(BIN)/test_uop_opt \
  $(BIN)/test_uop_recognise_tc \
  $(BIN)/test_apply_opt_dag \
  $(BIN)/test_simplify_ranges \
  $(BIN)/test_nested_reduce_unroll \
  $(BIN)/test_full_unroll_repro \
  $(BIN)/test_render_c_slot_hole \
  $(BIN)/test_render_reduce_chain \
  $(BIN)/test_uop_walk_domains \
  $(BIN)/test_bypass_gate_view_offset \
  $(BIN)/test_render_uop \
  $(BIN)/test_render_uop_metal \
  $(BIN)/test_render_uop_cuda \
  $(BIN)/test_realize_gc_roots \
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
  $(BIN)/test_icc \
  $(BIN)/test_pri \
  $(BIN)/test_app_sup \
  $(BIN)/test_dyn_lab \
  $(BIN)/test_wsq \
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
  $(BIN)/test_multi_trace_on \
  $(ATP_TESTS)

# Test binaries that link a GPU backend: the Metal ones build on Darwin,
# the CUDA ones on Linux with the toolkit headers.
METAL_TESTS := $(addprefix $(BIN)/,test_metal_real test_metal_pad_softmax \
  test_metal_mixed_input test_metal_tc_transpose test_metal_fuse_matmul_input \
  test_metal_fuse_matmul_compile test_metal_copy_retarget test_metal_group_local \
  test_aot_metal test_aot_metal_run)
CUDA_TESTS  := $(addprefix $(BIN)/,test_cuda_backend test_cuda_mixed \
  test_custom_kernel_cuda)

# === Metal backend (Darwin only) =====================================
# src/backend/metal/_.m compiles separately into build/backend_metal.o.
# Tests / dylib that #define THVM_HAS_METAL link this object + the
# Metal frameworks; src/thvm.c then skips the C-side stub.
ifeq ($(shell uname -s),Darwin)
  METAL_OBJ      := $(BUILD)/backend_metal.o
  METAL_LDFLAGS  := -framework Metal -framework Foundation -framework MetalPerformanceShaders
  METAL_LIBPATH  := $(BUILD)/default.metallib
  METAL_SHADERS  := $(wildcard src/backend/metal/shaders/*.metal)
  METAL_AIRS     := $(METAL_SHADERS:src/backend/metal/shaders/%.metal=$(BUILD)/%.air)
  METAL_DEFINES  := -DTHVM_METAL_METALLIB='"$(METAL_LIBPATH)"'
  TESTS          += $(METAL_TESTS)
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
# there) and a full-toolkit image both
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
                      -L$(CUDA_HOME)/lib -L$(CUDA_HOME)/lib/stubs \
                      -L/usr/lib/x86_64-linux-gnu -lcuda -lnvrtc
    TESTS          += $(CUDA_TESTS)
  endif
endif

# Every C and header file under src/, plus the test harness headers.
# Used as a prerequisite by both the C tests and the WL bridge so any
# runtime change retriggers a rebuild.

# Default goal: build the WL paclet dylib only.  Every C test depends on
# all of $(SRC), so a bare `make` used to recompile ~100 test binaries on
# any header edit (each is a full standalone build of the runtime) -- far
# too slow for the edit / rebuild / WL-repro loop.  The paclet dylib is
# the actual dev artifact and is itself a full compile-check of the
# runtime, so `make` now produces just that.  Use `make test` to build +
# run the C tests, `make all` to build them, `make bin/test_foo` for one.
.DEFAULT_GOAL := wl

.PHONY: all test clean wl wl-test wl-examples
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
#     wl -f ~/.claude/skills/wolfram-cloud-account/run-as.wls wolframinstitute wl/scripts/release.wls
#     wl -f wl/scripts/verify_install.wls  # PacletInstall + Needs
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
WL_XCC_FLAGS := -std=c11 -O2 -w -D_GNU_SOURCE $(WL_TRACE_DEF) $(ATP_DEFINES) $(WL_ATP_DEFINES) -I"$(WL_INCLUDE)"

# Per-format runtime blobs: the embedded thvm source is .incbin'd into
# every per-platform library, and the host object must match the
# linker's format (Mach-O native, ELF Linux, COFF Windows).
build/thvm_runtime_blob_elf.c: build/thvm_inline.c tools/build/embed_blob.py
	python3 tools/build/embed_blob.py build/thvm_inline.c thvm_runtime_src elf > $@
build/thvm_runtime_blob_coff.c: build/thvm_inline.c tools/build/embed_blob.py
	python3 tools/build/embed_blob.py build/thvm_inline.c thvm_runtime_src coff > $@

.PHONY: wl-cross wl-mac wl-linux-x64 wl-linux-arm64 wl-win-x64 wl-linux-cuda

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
	  -DACCELERATE_NEW_LAPACK $(WL_TRACE_DEF) $(ATP_DEFINES) $(WL_ATP_DEFINES) -DTHVM_HAS_METAL \
	  -I"$(WL_INCLUDE)" \
	  -o $(WL_RES)/MacOSX-ARM64/THVMLink.dylib \
	  $(WL_SRC) build/thvm_runtime_blob.c build/backend_metal_univ.o \
	  -framework Metal -framework Foundation -framework MetalPerformanceShaders -framework Accelerate
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

# Native Linux + CUDA build of the WL paclet library -- run ON a CUDA box (a GPU
# pod) where cuda.h + the Wolfram LibraryLink headers are present.  wl-linux-x64
# (zig) is CPU-only; this compiles the CUDA driver backend in (THVM_HAS_CUDA,
# links libcuda/libnvrtc) into THVMLink-cuda.so, shipped ALONGSIDE the CPU
# THVMLink.so.  The WL loader (Kernel/THVMLink.wl) prefers -cuda.so when libcuda
# loads (a GPU box) and falls back to the CPU .so otherwise, so ONE published
# paclet serves both GPU pods and CPU-only Linux.  thvmlink.c #includes thvm.c
# (which pulls in backend/cuda/ under THVM_HAS_CUDA) and thvmlink_atp.c, so the
# whole library is one TU -- no separate backend object (unlike Metal's ObjC).
#   Build:  make wl-linux-cuda WOLFRAM_APP=/usr/local/Wolfram/WolframEngine/15.0
wl-linux-cuda: build/thvm_runtime_blob_elf.c $(WL_SRC)
	@if [ -z "$(HAVE_CUDA)" ]; then echo "ERROR: CUDA toolkit not found under $(CUDA_HOME) (set CUDA_HOME)"; exit 1; fi
	@if [ ! -d "$(WL_INCLUDE)" ]; then echo "ERROR: Wolfram LibraryLink headers not found at $(WL_INCLUDE) (set WOLFRAM_APP)"; exit 1; fi
	@mkdir -p $(WL_RES)/Linux-x86-64
	$(CC) -std=c11 -O2 -w -fPIC -shared -D_GNU_SOURCE $(CUDA_DEFINES) \
	  $(ATP_DEFINES) $(WL_ATP_DEFINES) \
	  -I"$(WL_INCLUDE)" \
	  -o $(WL_RES)/Linux-x86-64/THVMLink-cuda.so \
	  $(WL_SRC) build/thvm_runtime_blob_elf.c $(CUDA_LDFLAGS) -lm -ldl
	@echo "[wl-linux-cuda] built $(WL_RES)/Linux-x86-64/THVMLink-cuda.so (CUDA-enabled)"


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
# Linux CPU BLAS: when OpenBLAS (or BLIS) is installed, native Linux builds
# route matmul / matvec / dot kernels through cblas, as macOS does through
# Accelerate (src/backend/cpu/blas.c).  Found via pkg-config, else a
# <cblas.h> + -lopenblas probe.  THVM_CBLAS=0 opts out; CBLAS_CFLAGS /
# CBLAS_LIBS override the detection.
THVM_CBLAS ?= 1
ifeq ($(UNAME_S),Linux)
ifneq ($(THVM_CBLAS),0)
CBLAS_PKG := $(shell pkg-config --exists openblas 2>/dev/null && echo openblas || (pkg-config --exists blis 2>/dev/null && echo blis))
ifneq ($(CBLAS_PKG),)
CBLAS_CFLAGS ?= -DTHVM_HAVE_CBLAS $(shell pkg-config --cflags $(CBLAS_PKG))
CBLAS_LIBS   ?= $(shell pkg-config --libs $(CBLAS_PKG))
else
CBLAS_PROBE := $(shell printf '\043include <cblas.h>\nint main(void){return (int)cblas_sdot(0,0,1,0,1);}\n' | $(CC) -x c - -o /dev/null -lopenblas 2>/dev/null && echo yes)
ifeq ($(CBLAS_PROBE),yes)
CBLAS_CFLAGS ?= -DTHVM_HAVE_CBLAS
CBLAS_LIBS   ?= -lopenblas
endif
endif
endif
endif
TEST_LDFLAGS := $(if $(filter Darwin,$(UNAME_S)),-framework Accelerate,-lm $(CBLAS_LIBS))
# Test binaries that #include src/thvm.c (which pulls in src/atp/_.c)
# need ATP_DEFINES so the ATP_RULE_INDEX-gated forward decls match
# the unconditional call sites at ~lines 5866, 8601, 9982.  Folding
# ATP_DEFINES into TEST_DEFINES keeps the per-target rules clean.
TEST_DEFINES := $(if $(filter Darwin,$(UNAME_S)),-DACCELERATE_NEW_LAPACK,) $(ATP_DEFINES) $(CBLAS_CFLAGS)

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
# src/thvm.c #includes the codegen/backend .c files (render_uop.c,
# render_metal.c, etc.) directly, so a change to any of them must re-flatten
# the amalgamation.  $(SRC) (find src) already lists them, but Make compares
# mtimes at *second* granularity and only rebuilds on a strictly-newer
# prereq -- an editor save + a fast `make` land in the same second and the
# rebuild is SILENTLY skipped, shipping a stale dylib.
#
# Fix: the flatten/embed recipes are marked FORCE so they ALWAYS run (they
# are ~1s), but each writes to a temp and only replaces (and advances the
# mtime of) $@ on a real CONTENT change.  So a same-second source edit is
# never missed, yet the expensive blob -> dylib relink only fires when the
# flattened runtime source genuinely changed.  The explicit codegen/backend
# wildcards document the include surface (and guard any $(SRC) find-ordering
# quirk).
.PHONY: FORCE
FORCE:

build/thvm_inline.c: $(SRC) $(wildcard src/codegen/*.c src/backend/*/*.c src/backend/*/*/*.c) tools/build/inline_includes.py FORCE | $(BUILD)
	@python3 tools/build/inline_includes.py src/thvm.c > $@.tmp
	@if [ -f $@ ] && cmp -s $@.tmp $@; then rm -f $@.tmp; \
	 else mv -f $@.tmp $@; echo "regenerated $@"; fi

build/thvm_runtime_blob.c: build/thvm_inline.c tools/build/embed_blob.py FORCE
	@python3 tools/build/embed_blob.py build/thvm_inline.c thvm_runtime_src macho > $@.tmp
	@if [ -f $@ ] && cmp -s $@.tmp $@; then rm -f $@.tmp; \
	 else mv -f $@.tmp $@; echo "regenerated $@"; fi

# AOT-using test binaries link the runtime blob so the extern
# thvm_runtime_src symbol resolves: the Mach-O blob on macOS, the ELF one
# elsewhere.  A loaded AOT module resolves thvm_pri_wl_invoke_returning
# against the test binary, which on Linux exports it only with -rdynamic.
AOT_TESTS   := $(BIN)/test_aot_emit $(BIN)/test_aot_e2e \
               $(BIN)/test_aot_e2e_bench $(BIN)/test_aot_build
AOT_BLOB    := $(if $(filter Darwin,$(UNAME_S)),build/thvm_runtime_blob.c,build/thvm_runtime_blob_elf.c)
AOT_LDFLAGS := $(if $(filter Darwin,$(UNAME_S)),,-rdynamic)

$(AOT_TESTS): $(BIN)/test_%: tests/test_%.c $(SRC) $(AOT_BLOB) | $(BIN)
	$(CC) $(CFLAGS) $(TEST_DEFINES) -o $@ $< $(AOT_BLOB) $(TEST_LDFLAGS) $(AOT_LDFLAGS)

$(METAL_TESTS): $(BIN)/%: tests/%.c $(SRC) $(METAL_OBJ) $(METAL_LIBPATH) | $(BIN)
	$(CC) $(CFLAGS) $(TEST_DEFINES) -DTHVM_HAS_METAL -o $@ $< $(METAL_OBJ) $(METAL_LDFLAGS) $(TEST_LDFLAGS)

# CUDA tests: -DTHVM_HAS_CUDA pulls backend/cuda/ into the single-TU build;
# -lcuda -lnvrtc link the driver + nvrtc.  Reachable only when the CUDA
# block above added them to TESTS (Linux+CUDA).
$(CUDA_TESTS): $(BIN)/%: tests/%.c $(SRC) | $(BIN)
	$(CC) $(CFLAGS) $(TEST_DEFINES) $(ATP_DEFINES) $(CUDA_DEFINES) -o $@ $< $(CUDA_LDFLAGS) $(TEST_LDFLAGS)

# DeepSeek-V2-Lite BS=1 decode-step microbenchmark (Stage 1): synthetic bf16
# weights, custom rope/MLA3/MoE/argmax kernels + native uop matmuls, captured
# once and replayed.  DEV={cpu,cuda} at runtime; on a CUDA pod build with
# CUDA_HOME pointing at the toolkit.  Not part of TESTS (it is a benchmark);
# the WL twin is wl/THVMLink/Examples/deepseek_v2/decode_bench.wls.
$(BIN)/dsv2_decode_bench: bench/dsv2_decode_bench.c $(SRC) | $(BIN)
	$(CC) $(CFLAGS) $(TEST_DEFINES) $(ATP_DEFINES) $(CUDA_DEFINES) -o $@ $< $(CUDA_LDFLAGS) $(TEST_LDFLAGS)

# Cross-backend dispatch microbench.  One binary; the backend is chosen
# at runtime via DEV={cpu,metal,cuda}.  On macOS it links the Metal
# backend (so DEV=cpu and DEV=metal both work); on Linux+CUDA it links
# the CUDA backend (DEV=cpu and DEV=cuda).
ifeq ($(UNAME_S),Darwin)
$(BIN)/xbackend_bench: bench/xbackend_bench.c $(SRC) $(METAL_OBJ) $(METAL_LIBPATH) | $(BIN)
	$(CC) $(CFLAGS) $(TEST_DEFINES) $(if $(METAL_OBJ),-DTHVM_HAS_METAL,) -o $@ $< $(METAL_OBJ) $(METAL_LDFLAGS) $(TEST_LDFLAGS)
else
$(BIN)/xbackend_bench: bench/xbackend_bench.c $(SRC) | $(BIN)
	$(CC) $(CFLAGS) $(TEST_DEFINES) $(CUDA_DEFINES) -o $@ $< $(CUDA_LDFLAGS) $(TEST_LDFLAGS)
endif

# Multicomputation trace -- built TWICE from the same source so we
# can verify both halves of the gating discipline.  test_multi_trace
# uses default CFLAGS (no -DTHVM_TRACE), so the multi_emit() call sites
# in src/interact/*.c compile to ((void)0) and src/instrument/multi.c
# is empty.  test_multi_trace_on adds -DTHVM_TRACE, so the runtime
# flag and all multi_trace_* API are present.  Both binaries must
# pass; see docs/multicomputation/multicomputation_trace.md.
$(BIN)/test_multi_trace_on: tests/test_multi_trace.c $(SRC) | $(BIN)
	$(CC) $(CFLAGS) $(TEST_DEFINES) -DTHVM_TRACE -o $@ $< $(TEST_LDFLAGS)


$(BIN)/test_%: tests/test_%.c $(SRC) | $(BIN)
	$(CC) $(CFLAGS) $(TEST_DEFINES) $(ATP_DEFINES) -o $@ $< $(TEST_LDFLAGS)


# === py/ ctypes bindings (libthvm_py.{dylib,so}) =====================
# Single-TU build of src/thvm.c + extern-C wrapper that re-exports the
# static-inline UOp constructors, so ctypes can drive thvm from Python.
# One `py` target per platform: libthvm_py.dylib on macOS (with the Metal
# backend), libthvm_py.so on Linux (with the CUDA backend when the CUDA
# headers were found, HAVE_CUDA from the block above).
ifeq ($(shell uname -s),Darwin)
PY_DYLIB        := py/thvm/libthvm_py.dylib
PY_THVM_OBJ     := $(BUILD)/py_thvm.o
$(PY_THVM_OBJ): py/csource/thvm_py.c $(SRC) | $(BUILD)
	clang -fPIC -O2 -DACCELERATE_NEW_LAPACK $(ATP_DEFINES) \
	    -DTHVM_HAS_METAL $(METAL_DEFINES) \
	    -Wno-unused-function -Wno-unused-variable -Wno-int-conversion \
	    -c -o $@ $<
# Link the real Metal backend (backend_metal.o) + its metallib: without it
# thvm.c falls back to the no-op Metal stub and every Metal buffer reads
# back zero.
$(PY_DYLIB): $(PY_THVM_OBJ) $(METAL_OBJ) $(METAL_LIBPATH)
	clang -shared -framework Accelerate -framework Metal -framework Foundation \
	    -framework MetalPerformanceShaders \
	    -o $@ $(PY_THVM_OBJ) $(METAL_OBJ)
.PHONY: py
py: $(PY_DYLIB)
endif

ifeq ($(shell uname -s),Linux)
PY_SO           := py/thvm/libthvm_py.so
PY_THVM_OBJ     := $(BUILD)/py_thvm.o
$(PY_THVM_OBJ): py/csource/thvm_py.c $(SRC) | $(BUILD)
	$(CC) -fPIC -O2 $(CUDA_DEFINES) $(ATP_DEFINES) $(CBLAS_CFLAGS) \
	    -Wno-unused-function -Wno-unused-variable -Wno-int-conversion \
	    -c -o $@ $<
$(PY_SO): $(PY_THVM_OBJ)
	$(CC) -shared -o $@ $^ $(CUDA_LDFLAGS) -lm $(CBLAS_LIBS)
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
