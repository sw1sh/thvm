#!/bin/bash
# Build the AxiomaticTheory dataset, one theory per `wl -t 240` (self-killing,
# resumable). Reads the proved fast-theory list from the NotableTheorems TSV.
#   build_all_safe.sh
cd "$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)" || exit 1
OUT="${ATP_DATA:-/tmp/enigma}/ds"; mkdir -p "$OUT"
THEORIES=$(awk -F'\t' 'NR>1 && $3=="PROVED" && ($4+0)<8 {gsub(/"/,"",$1);print $1}' \
   tools/baselines/thvm_notable_theorems.tsv | sort -u)
for th in $THEORIES; do
  out="$OUT/$th.wxf"
  [ -f "$out" ] && { echo "$th: cached"; continue; }
  wl -t 240 -f tools/atp_enigma/prove_theory.wls "$th" NONE 0 "$out" 2>&1 \
    | grep -iE "solved|SKIP"
done
echo "BUILD_DONE: $(ls "$OUT"/*.wxf 2>/dev/null | wc -l) theory datasets"
