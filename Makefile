# Makefile - single-TU build. Each test is its own executable that
# #includes ../src/thvm.c. The runtime grows by adding files under src/
# and including them from src/thvm.c - never edit this Makefile to add
# a source file, only to add a test.

CC      ?= clang
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Wno-unused-function
BIN     := bin

TESTS := \
  $(BIN)/test_term \
  $(BIN)/test_heap \
  $(BIN)/test_app_lam \
  $(BIN)/test_era \
  $(BIN)/test_dup_sup \
  $(BIN)/test_dup_lam \
  $(BIN)/test_tensor \
  $(BIN)/test_uop

# Every C and header file under src/, plus the test harness header.
# Used as a prerequisite by both the C tests and the WL bridge so any
# runtime change retriggers a rebuild.
SRC := $(shell find src -name '*.c' -o -name '*.h') tests/test.h

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

# Pick the newest /Applications/Wolfram*.app unless WOLFRAM_APP is set.
WOLFRAM_APP ?= $(shell ls -d "/Applications/Wolfram"*.app 2>/dev/null | sort -V | tail -1)
WL_INCLUDE   := $(WOLFRAM_APP)/Contents/SystemFiles/IncludeFiles/C

WL_PACLET   := wl/THVMLink
WL_LIB_DIR  := $(WL_PACLET)/LibraryResources/$(WL_PLATFORM)
WL_LIB      := $(WL_LIB_DIR)/THVMLink$(WL_DYLIB_EXT)
WL_SRC      := $(WL_PACLET)/CSource/thvmlink.c

$(WL_LIB_DIR):
	@mkdir -p $@

$(WL_LIB): $(WL_SRC) $(SRC) | $(WL_LIB_DIR)
	@if [ -z "$(WOLFRAM_APP)" ] || [ ! -d "$(WL_INCLUDE)" ]; then \
	  echo "ERROR: Wolfram install not found.  Set WOLFRAM_APP=/Applications/Wolfram*.app"; \
	  exit 1; \
	fi
	$(CC) $(CFLAGS) -fPIC $(WL_DYLIB_FLAGS) \
	  -I"$(WL_INCLUDE)" \
	  -o $@ $(WL_SRC)
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

$(BIN)/test_%: tests/test_%.c $(SRC) | $(BIN)
	$(CC) $(CFLAGS) -o $@ $<

test: $(TESTS)
	@fail=0; \
	for t in $(TESTS); do \
	  printf '%s\n' "> $$t"; \
	  $$t || fail=1; \
	done; \
	exit $$fail

clean:
	rm -rf $(BIN)
