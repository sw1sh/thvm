# FindProof -- standalone CASC UEQ build.  Single self-contained C
# binary; no external libraries beyond libm.  The whole engine compiles
# as one translation unit (tools/thvm_atp_cli.c includes the ATP core).
CC     ?= cc
CFLAGS ?= -std=c11 -O2 -w -D_GNU_SOURCE
DEFS   := -DATP_CP_GROUND_JOIN -DATP_FV_INDEX -DATP_MNF -DATP_ORDERED_REWRITE \
          -DATP_ORPHAN_KILL -DATP_RULE_INDEX -DATP_VAR_NORM \
          -DTHVM_ATPFT_ALLOC -DTHVM_ATPFT_CONVERT -DTHVM_ATPFT_LPO \
          -DTHVM_ATPFT_MATCH -DTHVM_ATPFT_NORM -DTHVM_ATPFT_RULES -DTHVM_ATP_AC
UNAME_S := $(shell uname -s)
LDFLAGS := $(if $(filter Darwin,$(UNAME_S)),-framework Accelerate,-lm)
DEFS    += $(if $(filter Darwin,$(UNAME_S)),-DACCELERATE_NEW_LAPACK,)

bin/FindProof: tools/thvm_atp_cli.c tools/proof_check.c
	@mkdir -p bin
	$(CC) $(CFLAGS) $(DEFS) -o $@ tools/thvm_atp_cli.c $(LDFLAGS)

# ENIGMA Tier-2 offline parity harness: score exported JSONL graphs with
# the exact in-engine GCN forward (thvm_atp_gnn_score_graphs) so the
# Python trainer can assert forward parity against the C engine.
bin/gnn_score_dump: tools/atp_enigma/gnn_score_dump.c
	@mkdir -p bin
	$(CC) $(CFLAGS) $(DEFS) -o $@ tools/atp_enigma/gnn_score_dump.c $(LDFLAGS)

clean:
	rm -f bin/FindProof bin/gnn_score_dump
.PHONY: clean
