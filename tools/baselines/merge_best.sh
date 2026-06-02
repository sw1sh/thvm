#!/usr/bin/env bash
# Merge per-conjunct results from multiple config runs into a single
# best-of TSV (PROVED beats Partial beats TimedOut beats Failed beats
# CRASH).  Used to show the union of what thvm CAN crack across all
# probed configs.
set -u
BASE=${1:-tools/baselines/thvm_per_conjunct.tsv}
shift || true
OUT=tools/baselines/thvm_per_conjunct_best.tsv

# Status priority: PROVED=0 (best), Partial=1, TimedOut=2, Failed=3, CRASH=4.
awk -F'\t' -v OFS='\t' '
function prio(s) {
    if (s == "PROVED") return 0
    if (s == "TimedOut") return 2
    if (s == "CRASH") return 4
    return 3  # Failed
}
NR == 1 && FNR == 1 { print; next }
FNR == 1 { next }
{
    key = $1 OFS $2 OFS $3
    p = prio($4)
    if (!(key in best) || p < best_prio[key]) {
        best[key] = $0
        best_prio[key] = p
    }
}
END {
    for (k in best) print best[k]
}' $BASE "$@" | sort > $OUT.tmp
head -1 $OUT.tmp > $OUT
grep -v "^theory	" $OUT.tmp >> $OUT
rm $OUT.tmp
echo "Wrote $OUT ($(wc -l < $OUT) rows incl header)" >&2
