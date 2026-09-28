#!/bin/bash
# Neural-selection phase 0: observation-only check of the history export.
# Runs each problem twice with the same binary and lane flags -- export
# OFF, then ON (rate 1) -- with the per-pick selection trace armed
# (THVM_ATP_CP_PICK_TRACE: pick, seq, priority, rules, both sides), and
# compares the selection sequence, the SZS status, the emitted proof and
# the engine's final stats line.  Pod-only (compute).
#
# Usage: np0_obs.sh <FindProof> <TPTP_root> <problems.txt> <outdir> \
#                   [wall=60] [lane=wm] ["lane flags"]
set -u
BIN=$1; ROOT=$2; PROBS=$3; OUT=$4; WALL=${5:-60}; LANE=${6:-wm}; FLAGS=${7:-}
mkdir -p "$OUT"
: > "$OUT/obs.tsv"
while read -r rel; do
  [ -z "$rel" ] && continue
  nm=$(basename "$rel" .p)
  for mode in off on; do
    ex=""
    [ "$mode" = on ] && ex="THVM_ATP_HISTORY_EXPORT=$OUT/$nm.hx THVM_ATP_HISTORY_RATE=1"
    rm -f "$OUT/$nm.hx"
    # shellcheck disable=SC2086
    (cd /tmp && env FINDPROOF_SLICE="$LANE" THVM_ATP_RSS_ABORT_MB=3500 TPTP="$ROOT" \
       THVM_ATP_CP_PICK_TRACE=1 $ex \
       timeout $((WALL + 30)) "$BIN" $FLAGS --wall="$WALL" --szs --proof \
       "$ROOT/$rel" </dev/null >"$OUT/$nm.$mode.out" 2>"$OUT/$nm.$mode.err")
    grep -E '^CPSEL|^CPSKIP' "$OUT/$nm.$mode.err" > "$OUT/$nm.$mode.sel"
    grep -E '^   goal=.* steps=' "$OUT/$nm.$mode.err" \
      | sed -E 's/goal=[^ ]+//; s/ +[0-9.]+s$//' > "$OUT/$nm.$mode.stats"
    grep -E '^% SZS status' "$OUT/$nm.$mode.out" | head -1 \
      | awk '{print $4}' > "$OUT/$nm.$mode.szs"
  done
  sel_eq=$(cmp -s "$OUT/$nm.off.sel" "$OUT/$nm.on.sel" && echo same || echo DIFF)
  prf_eq=$(cmp -s "$OUT/$nm.off.out" "$OUT/$nm.on.out" && echo same || echo DIFF)
  st_eq=$(cmp -s "$OUT/$nm.off.stats" "$OUT/$nm.on.stats" && echo same || echo DIFF)
  printf '%s\t%s\t%s\t%s\tpicks=%s\tselection=%s\tproof=%s\tstats=%s\t%s\n' \
    "$nm" "$(cat "$OUT/$nm.off.szs")" "$(cat "$OUT/$nm.on.szs")" \
    "$(wc -c < "$OUT/$nm.hx" 2>/dev/null || echo 0)" \
    "$(grep -c '^CPSEL' "$OUT/$nm.off.sel")" "$sel_eq" "$prf_eq" "$st_eq" \
    "$(cat "$OUT/$nm.off.stats")" >> "$OUT/obs.tsv"
  rm -f "$OUT/$nm.off.err" "$OUT/$nm.on.err"
done < "$PROBS"
echo done > "$OUT/obs.done"
