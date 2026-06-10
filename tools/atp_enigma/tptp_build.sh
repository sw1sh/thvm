#!/bin/bash
# Per-problem TPTP UEQ dataset build. Each problem runs under `wl -t 18` (self-
# kills its kernel on timeout, so no orphan accumulation), isolated (no cross-
# call state), resumable (skips existing .wxf).
#   tptp_build.sh <list-of-.p-paths> [out-dir]
# Env: TPTP_ROOT (TPTP install root, for include resolution).
cd "$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)" || exit 1
export TPTP_ROOT="${TPTP_ROOT:?set TPTP_ROOT to the TPTP install root}"
LIST=${1:?usage: tptp_build.sh <list> [out-dir]}
OUT=${2:-${ATP_DATA:-/tmp/enigma}/tptp_ds}; mkdir -p "$OUT"
n=0; solved=0
while read -r p; do
  [ -z "$p" ] && continue
  n=$((n+1)); id=$(basename "$p" .p); o="$OUT/$id.wxf"
  [ -f "$o" ] && continue
  line=$(wl -t 18 -f tools/atp_enigma/prove_one_tptp.wls "$p" "$o" 2>/dev/null | tail -1)
  case "$line" in *"graphs"*) solved=$((solved+1)); echo "[$n] $line";; esac
done < "$LIST"
echo "TPTP_DONE: $n attempted, $solved with data, $(ls "$OUT"/*.wxf 2>/dev/null|wc -l) datasets"
