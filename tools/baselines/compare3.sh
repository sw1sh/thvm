#!/usr/bin/env bash
# 3-way comparison: thvm vs WL FindEquationalProof vs Vampire 5.0.1.
# Vampire's TSV is per-conjunct; aggregate to per-theorem (theorem
# PROVED iff EVERY conjunct PROVED, theorem TimedOut iff ANY conjunct
# TimedOut, theorem Saturated otherwise).  WL + thvm are per-theorem
# already (multi-conjunct return a List of ProofObjects, treated as
# one PROVED theorem).
WL=tools/baselines/wl_notable_theorems.tsv
TH=tools/baselines/thvm_notable_theorems.tsv
VA=tools/baselines/vampire_notable_theorems.tsv

for f in $WL $TH $VA; do
    [[ ! -f $f ]] && { echo "Missing $f" >&2; exit 1; }
done

# Build per-theorem Vampire result: PROVED iff every conjunct PROVED.
VA_AGG=$(mktemp)
echo -e "theory\tthm\tstatus\tseconds" > $VA_AGG
awk -F'\t' 'NR>1 {
    k = $1"::"$2
    if (k in seen) {
        if ($4 != "PROVED") agg_status[k] = $4
        agg_sec[k] += $5
    } else {
        seen[k] = 1
        agg_status[k] = $4
        agg_sec[k] = $5
        order[++n] = k
    }
}
END {
    for (i = 1; i <= n; i++) {
        k = order[i]
        split(k, a, "::")
        printf "%s\t%s\t%s\t%.2f\n", a[1], a[2], agg_status[k], agg_sec[k]
    }
}' $VA >> $VA_AGG

# 3-way join + tally
awk -F'\t' '
NR == FNR && FNR == 1 { fn = 1; next }
NR == FNR { wl[$1"::"$2] = $3"|"$4; next }
fn == 1 && FNR == 1 { fn = 2; next }
fn == 1 { th[$1"::"$2] = $3"|"$4; next }
fn == 2 && FNR == 1 { next }
{
    k = $1"::"$2
    v_status = $3; v_sec = $4
    split(wl[k], w, "|"); w_status = w[1]; w_sec = w[2]
    split(th[k], t, "|"); t_status = t[1]; t_sec = t[2]
    pw = (w_status == "PROVED") ? 1 : 0
    pv = (v_status == "PROVED") ? 1 : 0
    pt = (t_status == "PROVED") ? 1 : 0
    sum = pw + pv + pt
    if (sum == 3) all3++
    else if (sum == 2) {
        two++
        if (!pt) miss_t++
        if (!pw) miss_w++
        if (!pv) miss_v++
        if (!pt) printf "  thvm misses: %-55s wl=%5.2fs  vampire=%5.2fs\n", k, w_sec, v_sec
    }
    else if (sum == 1) {
        one++
        if (pt) only_t++; if (pw) only_w++; if (pv) only_v++
        printf "  ONLY %s: %-55s\n", (pt ? "thvm" : (pw ? "wl" : "vampire")), k
    }
    else if (sum == 0) {
        zero++
        printf "  NONE proves: %-55s\n", k
    }
}
END {
    print "=================================================="
    printf "all 3 prove: %d\n", all3
    printf "2 of 3:      %d  (thvm missing: %d, wl missing: %d, vampire missing: %d)\n",
        two, miss_t, miss_w, miss_v
    printf "1 of 3:      %d  (thvm only: %d, wl only: %d, vampire only: %d)\n",
        one, only_t, only_w, only_v
    printf "none:        %d\n", zero
    printf "total:       %d\n", all3 + two + one + zero
}
' $WL $TH $VA_AGG

rm -f $VA_AGG
