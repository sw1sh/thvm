#!/bin/bash
# Compare thvm-Waldmeister-preset vs vampire CLI baseline.  Pure file-join.
set -u

THVM=/Users/swish/src/thvm/tools/baselines/thvm_loop_probes.tsv
VAMP=/Users/swish/src/thvm/tools/baselines/vampire_notable_theorems.tsv

awk -F'\t' '
    FNR == NR && NR > 1 {
        key = $1 "__" $2
        thvm[key] = $3
        next
    }
    FNR > 1 {
        # vampire tsv: theory \t thm \t conjunct \t status \t seconds \t winningStrategy \t proofClauses
        # When conjunct > 1 the theorem name in the thvm tracker has __cN suffix
        if ($3 > 1 || ($3 == 1 && (thvm[$1 "__" $2 "__c1"] != ""))) {
            key = $1 "__" $2 "__c" $3
        } else {
            key = $1 "__" $2
        }
        t = thvm[key]
        if (t == "") next
        vstat = $4
        if (t == "FAIL" && vstat != "PROVED") tag = "BOTH_FAIL"
        else if (t == "FAIL")                  tag = "thvm_FAIL_only"
        else if (vstat != "PROVED")            tag = "vamp_FAIL_only"
        else                                    tag = "OK_OK"
        printf "%s\t%s\t%s\t%s\t%s\n", key, t, vstat, $5, tag
    }
' "$THVM" "$VAMP" | sort -t$'\t' -k5,5
