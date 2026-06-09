#!/bin/bash
# Compare thvm-Waldmeister-preset vs wmcli on the cases both datasets cover.
# Pure file-join, no spawn, no engine work, no risk.
set -u

THVM=/Users/swish/src/thvm/tools/baselines/thvm_loop_probes.tsv
WMCLI=/Users/swish/src/thvm/tools/baselines/wm_cli_sweep.tsv

awk -F'\t' '
    FNR == NR && NR > 1 {
        # thvm tsv: theory \t theorem \t chain \t ...
        key = $1 "__" $2
        thvm[key] = $3
        next
    }
    FNR > 1 {
        # wmcli tsv: theory_theorem \t rules \t equations \t cps \t time
        # name shape includes __cN for conjuncts
        gsub(/__c[0-9]+/, "&&CONJ", $1)
        key = $1
        sub(/&&CONJ/, "", key)
        rules = $2
        time  = $5
        t = thvm[$1]
        if (t == "") next       # not in thvm tracker
        # both have a result: compute parity
        if (t == "FAIL" && rules == "FAIL") tag = "BOTH_FAIL"
        else if (t == "FAIL")                tag = "thvm_FAIL_only"
        else if (rules == "FAIL")            tag = "wmcli_FAIL_only"
        else                                  tag = "OK_OK"
        printf "%s\t%s\t%s\t%s\t%s\n", $1, t, rules, time, tag
    }
' "$THVM" "$WMCLI" | sort -t$'\t' -k5,5
