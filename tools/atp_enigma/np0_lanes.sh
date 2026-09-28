#!/bin/bash
# Neural-selection phase 0: which single SCHEDULE lane solves the most
# pool problems?  Runs every listed lane on every listed problem as a
# standalone slice (exactly how run_schedule launches it: FINDPROOF_SLICE
# set, the lane's flags, --wall --szs --proof), one job per core.
#
# Usage: np0_lanes.sh <FindProof> <TPTP_root> <problems.txt> <lanes.tsv> \
#                     <out.tsv> [wall=60] [par=16] [rss_mb=3500]
#   problems.txt : one TPTP-relative path per line (Problems/XXX/Y.p)
#   lanes.tsv    : <lane name> TAB <flags>
#   out.tsv      : lane, problem, SZS status, wall seconds
# Pod-only (compute).  Writes <out.tsv>.done when every job finished.
set -u
BIN=$1; ROOT=$2; PROBS=$3; LANES=$4; OUT=$5
WALL=${6:-60}; PAR=${7:-16}; RSS=${8:-3500}
JOBS=$OUT.jobs
: > "$JOBS"
while IFS=$'\t' read -r lane flags; do
  [ -z "$lane" ] && continue
  while read -r rel; do
    [ -z "$rel" ] && continue
    printf '%s\t%s\t%s\n' "$lane" "$flags" "$rel" >> "$JOBS"
  done < "$PROBS"
done < "$LANES"
: > "$OUT"
export BIN ROOT WALL RSS OUT
run_one() {
  lane=$(printf '%s' "$1" | cut -f1)
  flags=$(printf '%s' "$1" | cut -f2)
  rel=$(printf '%s' "$1" | cut -f3)
  t0=$(date +%s.%N)
  # shellcheck disable=SC2086
  st=$(cd /tmp && FINDPROOF_SLICE="$lane" THVM_ATP_RSS_ABORT_MB=$RSS TPTP="$ROOT" \
       timeout $((WALL + 30)) "$BIN" $flags --wall="$WALL" --szs --proof \
       "$ROOT/$rel" </dev/null 2>/dev/null | grep -m1 -o 'SZS status [A-Za-z]*' \
       | awk '{print $3}')
  t1=$(date +%s.%N)
  [ -z "$st" ] && st=NoOutput
  printf '%s\t%s\t%s\t%s\n' "$lane" "$(basename "$rel" .p)" "$st" \
    "$(awk -v a="$t0" -v b="$t1" 'BEGIN{printf "%.2f", b-a}')" >> "$OUT"
}
export -f run_one
tr '\n' '\0' < "$JOBS" | xargs -0 -P "$PAR" -I{} bash -c 'run_one "$@"' _ {}
echo done > "$OUT.done"
