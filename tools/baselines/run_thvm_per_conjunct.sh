#!/usr/bin/env bash
# Per-conjunct thvm baseline runner.  Each conjunct of every
# NotableTheorem runs in its own fresh wolframscript subprocess so
# a SIGSEGV on conjunct k doesn't lose conjuncts 1..k-1's results
# (the in-process Table[...] dispatcher would collapse a partial
# multi-conjunct result to $Failed; see run_thvm_one.wls header).
#
# Output: tools/baselines/thvm_per_conjunct.tsv
#   theory \t thm \t k/n \t status \t seconds \t proofLength \t verifies
#
# Companion script `roll_up_per_conjunct.sh` aggregates this into a
# per-theorem summary with Partial(k/n) where applicable.

set -u
TC=${TC:-30}
HARDLIMIT=$((TC + 15))
OUT=tools/baselines/thvm_per_conjunct.tsv
COUNTS=/tmp/conjunct_counts.tsv
THMS=${1:-/tmp/thms.tsv}

if [[ ! -f $THMS ]]; then
    echo "Theorem list not found: $THMS" >&2
    exit 1
fi

# Pre-compute conjunct counts in ONE wolframscript call (avoids ~3s
# startup per row just to query the count).
wolframscript -f tools/baselines/list_conjuncts.wls $THMS > $COUNTS
echo "Conjunct counts written to $COUNTS ($(wc -l < $COUNTS) rows)" >&2

echo "theory	thm	conjunct	status	seconds	proofLength	verifies" > $OUT

n_theorems=0
n_conjuncts=0
while IFS=$'\t' read -r theory thm n <&3; do
    n_theorems=$((n_theorems+1))
    for i in $(seq 1 "$n"); do
        n_conjuncts=$((n_conjuncts+1))
        line=$(timeout --kill-after=5s ${HARDLIMIT}s wolframscript \
            -f tools/baselines/run_thvm_one_conjunct.wls \
            "$theory" "$thm" "$i" "$TC" \
            </dev/null 2>&1 | grep -E '^[A-Za-z]+	' | tail -1)
        rc=$?
        if [[ $rc -eq 124 ]]; then
            line="${theory}\t${thm}\t${i}/${n}\tHardKilled\t${HARDLIMIT}\t-\t-"
        fi
        if [[ -z "$line" ]]; then
            line="${theory}\t${thm}\t${i}/${n}\tCRASH\t-\t-\t-"
        fi
        printf '%b\n' "$line" | tee -a $OUT
    done
done 3< $COUNTS

echo "---" >&2
echo "DONE. $n_theorems theorems, $n_conjuncts conjuncts. Output: $OUT" >&2
awk -F'\t' 'NR>1 {c[$4]++} END {for (k in c) printf "  %-12s %d\n", k, c[k]}' $OUT >&2
