#!/usr/bin/env bash
# Run thvm TFindProof on every NotableTheorem under matched conditions
# to the WL baseline.  ONE wolframscript per case (cumulative-state
# crash insurance).  Same TC + HARDLIMIT as run_wl_baseline.sh.
# Output: tools/baselines/thvm_notable_theorems.tsv (overwritten each
# run -- this is the WORKING result; commit only the WL baseline,
# regenerate thvm whenever the tuner / engine changes).

set -u
TC=${TC:-30}
# +15s gap (was +5): the C engine consults wall_seconds at coarse
# granularity, so a hard case can overrun by several seconds.  The
# WL-side TimeConstrained in run_thvm_one.wls uses TC+10 -- this
# bash hard-limit must stay above that to avoid SIGKILL-during-write
# producing empty stdout (which the wrapper classifies as CRASH).
HARDLIMIT=$((TC + 15))
OUT=tools/baselines/thvm_notable_theorems.tsv
THMS=${1:-/tmp/thms.tsv}

if [[ ! -f $THMS ]]; then
    echo "Theorem list not found: $THMS" >&2
    exit 1
fi

echo "theory	thm	status	seconds	proofLength	verifies" > $OUT

n=0
while IFS=$'\t' read -r theory thm <&3; do
    n=$((n+1))
    line=$(timeout --kill-after=5s ${HARDLIMIT}s /Applications/Wolfram.app/Contents/MacOS/wolframscript \
        -f tools/baselines/run_thvm_one.wls "$theory" "$thm" "$TC" \
        </dev/null 2>&1 | grep -E '^[A-Za-z]+	' | tail -1)
    rc=$?
    if [[ $rc -eq 124 ]]; then
        line="${theory}\t${thm}\tHardKilled\t${HARDLIMIT}\t-\t-"
    fi
    if [[ -z "$line" ]]; then
        line="${theory}\t${thm}\tCRASH\t-\t-\t-"
    fi
    printf '%b\n' "$line" | tee -a $OUT
done 3< $THMS

echo "---" >&2
echo "DONE. $n cases. Output: $OUT" >&2
awk -F'\t' 'NR>1 {c[$3]++} END {for (k in c) printf "  %-12s %d\n", k, c[k]}' $OUT >&2
