# CheckProof -- standalone ProoVer build.  Single self-contained C
# binary; no external libraries beyond libm.  Same translation unit as
# FindProof (tools/thvm_atp_cli.c includes the engine); the binary name
# selects the proof-checking mode.
CC     ?= cc
CFLAGS ?= -std=c11 -O2 -w -D_GNU_SOURCE
DEFS   := -DATP_CP_GROUND_JOIN -DATP_FV_INDEX -DATP_MNF -DATP_ORDERED_REWRITE \
          -DATP_ORPHAN_KILL -DATP_RULE_INDEX -DATP_VAR_NORM \
          -DTHVM_ATPFT_ALLOC -DTHVM_ATPFT_CONVERT -DTHVM_ATPFT_LPO \
          -DTHVM_ATPFT_MATCH -DTHVM_ATPFT_NORM -DTHVM_ATPFT_RULES -DTHVM_ATP_AC
UNAME_S := $(shell uname -s)
LDFLAGS := $(if $(filter Darwin,$(UNAME_S)),-framework Accelerate,-lm)
DEFS    += $(if $(filter Darwin,$(UNAME_S)),-DACCELERATE_NEW_LAPACK,)

bin/CheckProof: tools/thvm_atp_cli.c tools/proof_check.c $(wildcard tools/atp_cli/*.c)
	@mkdir -p bin
	$(CC) $(CFLAGS) $(DEFS) -o bin/CheckProof tools/thvm_atp_cli.c $(LDFLAGS)

clean:
	rm -f bin/CheckProof
.PHONY: clean
