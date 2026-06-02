#!/usr/bin/env bash
# Aggregate tools/baselines/thvm_per_conjunct.tsv into a per-theorem
# summary.  For each (theory, thm) compute:
#   - PROVED   if every conjunct PROVED
#   - Partial(k/n) if 0 < k < n conjuncts PROVED
#   - Failed | TimedOut | CRASH  if k == 0 (most-frequent non-PROVED
#     status wins; ties break Failed -> TimedOut -> CRASH)
#   - total seconds across all conjuncts
#   - total proofLength across proven conjuncts
#
# Output to stdout (and optional argument).
set -u
SRC=${1:-tools/baselines/thvm_per_conjunct.tsv}
OUT=${2:-/dev/stdout}

awk -F'\t' '
NR == 1 { next }
{
    key = $1 "\t" $2
    if (!(key in seen)) {
        seen[key] = 1
        order[++n_keys] = key
    }
    total_secs[key] += ($5 == "-" ? 0 : $5)
    if ($4 == "PROVED") {
        proved[key]++
        total_len[key] += ($6 == "-" ? 0 : $6)
    } else {
        # tally non-PROVED status types
        nonp[key, $4]++
    }
    # total conjuncts for this theorem = max k seen in k/n field
    split($3, parts, "/")
    if (parts[2]+0 > total_conj[key]+0) total_conj[key] = parts[2]+0
}
END {
    printf "theory\tthm\tstatus\tseconds\tproofLength\tverifies\n"
    for (i = 1; i <= n_keys; i++) {
        key = order[i]
        n = total_conj[key]
        k = proved[key]+0
        if (k == n) {
            status = "PROVED"
            verif  = "Success"
        } else if (k > 0) {
            status = "Partial(" k "/" n ")"
            verif  = "PartialSuccess"
        } else {
            # most-frequent non-PROVED tag for this key
            best = "Failed"; bestn = -1
            for (tag in nonp) {
                if (substr(tag, 1, length(key)+1) == key SUBSEP) {
                    t = substr(tag, length(key)+2)
                    if (nonp[tag] > bestn) { best = t; bestn = nonp[tag] }
                }
            }
            status = best
            verif  = "-"
        }
        proofLen = (k > 0 ? total_len[key]+0 : "-")
        printf "%s\t%s\t%.2f\t%s\t%s\n", key, status, total_secs[key], proofLen, verif
    }
}' $SRC > $OUT

if [[ "$OUT" != "/dev/stdout" ]]; then
    echo "Wrote $OUT ($(wc -l < $OUT) rows)" >&2
fi
