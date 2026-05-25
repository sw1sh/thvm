#!/usr/bin/env bash
# Run WL FindEquationalProof on every NotableTheorem of every
# AxiomaticTheory, ONE WL kernel per case (so a kernel crash on one
# theorem doesn't lose the rest).  Hard per-case shell-level timeout
# 35s so no kernel can hang past TC + 5s.  Streams TSV to stdout +
# tools/baselines/wl_notable_theorems.tsv.
#
# Run once.  Future thvm sweeps compare against the cached TSV +
# per-theorem proof object files (tools/baselines/proofs/*.m).

set -u
TC=${TC:-30}
HARDLIMIT=$((TC + 5))
OUT=tools/baselines/wl_notable_theorems.tsv
THMS=${1:-/tmp/thms.tsv}

if [[ ! -f $THMS ]]; then
    echo "Theorem list not found: $THMS" >&2
    echo "Generate with: wolframscript -f /tmp/enum_thms.wls > /tmp/thms.tsv" >&2
    exit 1
fi

echo "theory	thm	status	seconds	proofLength	verifies" > $OUT

n=0
while IFS=$'\t' read -r theory thm <&3; do
    n=$((n+1))
    line=$(timeout --kill-after=5s ${HARDLIMIT}s /Applications/Wolfram.app/Contents/MacOS/wolframscript \
        -f tools/baselines/run_wl_one.wls "$theory" "$thm" "$TC" \
        </dev/null 2>&1 | grep -E '^[A-Za-z]+	' | tail -1)
    rc=$?
    if [[ $rc -eq 124 ]]; then
        # shell-killed: TC was ignored
        line="${theory}\t${thm}\tHardKilled\t${HARDLIMIT}\t-\t-"
    fi
    if [[ -z "$line" ]]; then
        line="${theory}\t${thm}\tCRASH\t-\t-\t-"
    fi
    printf '%b\n' "$line" | tee -a $OUT
done 3< $THMS

echo "---" >&2
echo "DONE.  $n cases. Baseline: $OUT  Proof objects: tools/baselines/proofs/*.m" >&2
echo "Tally:" >&2
awk -F'\t' 'NR>1 {c[$3]++} END {for (k in c) print "  ", k, c[k]}' $OUT >&2
