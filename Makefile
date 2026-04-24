# Makefile — single-TU build. Each test is its own executable that
# #includes ../src/thvm.c. The runtime grows by adding files under src/
# and including them from src/thvm.c — never edit this Makefile to add
# a source file, only to add a test.

CC      ?= clang
CFLAGS  ?= -std=c11 -O2 -Wall -Wextra -Wpedantic -Wno-unused-function
BIN     := bin

TESTS := \
  $(BIN)/test_term \
  $(BIN)/test_heap \
  $(BIN)/test_app_lam \
  $(BIN)/test_era \
  $(BIN)/test_dup_sup

.PHONY: all test clean
all: $(TESTS)

$(BIN):
	@mkdir -p $@

# Each test depends on every src/*.c (single-TU build) and the harness header.
SRC := $(shell find src -name '*.c' -o -name '*.h') tests/test.h

$(BIN)/test_%: tests/test_%.c $(SRC) | $(BIN)
	$(CC) $(CFLAGS) -o $@ $<

test: $(TESTS)
	@fail=0; \
	for t in $(TESTS); do \
	  printf '%s\n' "→ $$t"; \
	  $$t || fail=1; \
	done; \
	exit $$fail

clean:
	rm -rf $(BIN)
