#!/usr/bin/env bash
# Side-by-side comparison: thvm vs cached WL baseline.
# Tallies wins/losses + lists every case where thvm and wl disagree
# on PROVED/TimedOut.  Reads:
#   tools/baselines/wl_notable_theorems.tsv    (cached, do not regen)
#   tools/baselines/thvm_notable_theorems.tsv  (current thvm run)
WL=tools/baselines/wl_notable_theorems.tsv
TH=tools/baselines/thvm_notable_theorems.tsv

if [[ ! -f $WL ]]; then echo "Missing $WL - run run_wl_baseline.sh first" >&2; exit 1; fi
if [[ ! -f $TH ]]; then echo "Missing $TH - run run_thvm_baseline.sh first" >&2; exit 1; fi

awk -F'\t' '
NR == FNR { if (NR > 1) wl[$1"::"$2] = $3 "|" $4; next }
FNR == 1 { next }
{
    k = $1"::"$2
    ws = wl[k]; split(ws, w, "|")
    wlS = w[1]; wlT = w[2]
    thS = $3; thT = $4
    # bucket
    if (wlS == "PROVED" && thS == "PROVED") {
        both++
        if (thT+0 < wlT+0) thfast++
        else if (wlT+0 < thT+0) wlfast++
    }
    else if (wlS == "PROVED" && thS != "PROVED") {
        wlonly++
        printf "  WL-only: %-50s wl=%5.2fs  thvm=%s\n", k, wlT, thS
    }
    else if (wlS != "PROVED" && thS == "PROVED") {
        thonly++
        printf "  thvm-only: %-50s thvm=%5.2fs  wl=%s\n", k, thT, wlS
    }
    else {
        neither++
        printf "  NEITHER: %-50s wl=%s  thvm=%s\n", k, wlS, thS
    }
}
END {
    print "==========================================="
    printf "both PROVED: %d  (thvm faster: %d  wl faster: %d)\n", both, thfast, wlfast
    printf "thvm-only:   %d\n", thonly
    printf "WL-only:     %d\n", wlonly
    printf "neither:     %d\n", neither
    printf "total:       %d\n", both + thonly + wlonly + neither
}
' $WL $TH
