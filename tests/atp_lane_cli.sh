#!/bin/bash
# FindProof --list-lanes / --lane=NAME.  --list-lanes prints the SCHEDULE
# table in order (name TAB flags); an unknown schedule mode
# (--schedule=NAME) and an unknown or missing lane name are input errors
# (exit 2, nothing on stdout); and a lane run is the schedule's slice
# child for that lane: its status line, its engine stats and the exit code
# it reports match the child command built by hand from the table
# (FINDPROOF_SLICE, the slice RSS cap, the lane's flags, the slice wall the
# schedule hands it).  A child that prints no status line must come back
# as GaveUp from the lane run, or as Error when it exited 2 (an input
# error, which the schedule reports once every slice hit it).
#
# Usage: tests/atp_lane_cli.sh [bin/FindProof [problem ...]]
#   LANES (default "wm flatten bare") picks the compared lanes, WALL (20)
#   the lane run's --wall.  Run from a built tree; the problems default to
#   three fast UEQ problems in the repo and one that every lane gives up
#   on at once (RNG042-2 has no negated conjecture).
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
BIN=${1:-$ROOT/bin/FindProof}
if [ $# -gt 0 ]; then
  shift
fi
PROBS=("$@")
if [ ${#PROBS[@]} -eq 0 ]; then
  PROBS=("$ROOT/wl/Examples/proover/problems/BOO001-1.p"
         "$ROOT/tools/baselines/wm_pr/GroupAxioms__InverseOfComposite.pr"
         "$ROOT/tools/baselines/wm_pr/BooleanAxioms__AndDistributivity.pr"
         "$ROOT/tests/soundness/RNG042-2.p")
fi
LANES=${LANES:-wm flatten bare}
WALL=${WALL:-20}
SRC=$ROOT/tools/atp_cli/main.c
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
fail=0

# The table as the source spells it: name TAB flags, one lane per line.
sed -n 's/^  {"\([^"]*\)", *"\([^"]*\)", *[0-9.]*},.*$/\1\t\2/p' "$SRC" > "$TMP/table"
rss=$(sed -n 's/^#define SCHED_SLICE_RSS_MB \([0-9]*\).*/\1/p' "$SRC")

"$BIN" --schedule --list-lanes > "$TMP/listed"
rc=$?
if [ $rc -ne 0 ] || [ ! -s "$TMP/table" ] || ! cmp -s "$TMP/table" "$TMP/listed"; then
  echo "  FAIL: --list-lanes (exit $rc) differs from the SCHEDULE table (name TAB flags)"
  fail=1
else
  echo "  ok: --list-lanes prints the $(wc -l < "$TMP/listed" | tr -d ' ') schedule lanes in order"
fi

for bad in --schedule=p1 --schedule=; do
  for tail in --list-lanes "${PROBS[0]}"; do
    "$BIN" "$bad" --szs "$tail" > "$TMP/bad.out" 2> "$TMP/bad.err"
    rc=$?
    if [ $rc -ne 2 ] || [ -s "$TMP/bad.out" ] || ! grep -q "schedule mode" "$TMP/bad.err"; then
      echo "  FAIL: $bad $tail exits $rc (want 2, a message, no stdout)"
      fail=1
    else
      echo "  ok: $bad $tail -> exit 2: $(head -1 "$TMP/bad.err")"
    fi
  done
done

for bad in --lane=no-such-lane --lane= --lane; do
  "$BIN" "$bad" --szs "${PROBS[0]}" > "$TMP/bad.out" 2> "$TMP/bad.err"
  rc=$?
  if [ $rc -ne 2 ] || [ -s "$TMP/bad.out" ] || ! grep -q "lane" "$TMP/bad.err"; then
    echo "  FAIL: $bad exits $rc (want 2, a message, no stdout)"
    fail=1
  else
    echo "  ok: $bad -> exit 2: $(head -1 "$TMP/bad.err")"
  fi
done

# The final engine stats without the run-dependent fields: the goal is the
# temporary .pr path (it carries the pid) and the trailing time is CPU.
stats() {
  grep -m1 '^=> ' "$1"
  grep -m1 '^   goal=' "$1" | sed -E 's/goal=[^ ]* +//; s/ +[0-9.]+s$//'
}

# run_schedule hands each slice the wall less 3 s (budget) above 6 s.
slice_wall=$WALL
if [ "$WALL" -gt 6 ]; then
  slice_wall=$((WALL - 3))
fi
for p in "${PROBS[@]}"; do
  for lane in $LANES; do
    flags=$(awk -F'\t' -v l="$lane" '$1 == l { print $2 }' "$TMP/table")
    env -u THVM_ATP_RSS_ABORT_MB "$BIN" --lane="$lane" --szs --wall="$WALL" "$p" \
      > "$TMP/l.out" 2> "$TMP/l.err"
    lrc=$?
    # The schedule's own command line: sh -c with the flags unquoted.
    env THVM_ATP_RSS_ABORT_MB="$rss" FINDPROOF_SLICE="$lane" \
      sh -c "'$BIN' $flags --wall=$slice_wall --szs '$p'" \
      > "$TMP/c.out" 2> "$TMP/c.err"
    crc=$?
    lst=$(grep -m1 '^% SZS status' "$TMP/l.out")
    cst=$(grep -m1 '^% SZS status' "$TMP/c.out")
    if [ -z "$cst" ] && [ "$crc" -eq 2 ]; then
      cst="% SZS status Error for $p"
    elif [ -z "$cst" ]; then
      cst="% SZS status GaveUp for $p"
    fi
    lstats=$(stats "$TMP/l.err")
    cstats=$(stats "$TMP/c.err")
    lend=$(grep -m1 "^% FindProof lane $lane: " "$TMP/l.err")
    name=$(basename "$p")
    if [ "$lst" != "$cst" ]; then
      echo "  FAIL: $lane $name: status '$lst' vs child '$cst'"
      fail=1
    elif [ "$lstats" != "$cstats" ]; then
      echo "  FAIL: $lane $name: stats differ"
      echo "    lane:  $lstats" | tr '\n' ' '
      echo
      echo "    child: $cstats" | tr '\n' ' '
      echo
      fail=1
    elif [ "$lend" != "% FindProof lane $lane: exit $crc" ]; then
      echo "  FAIL: $lane $name: lane reports '$lend', child exit $crc"
      fail=1
    elif [ "$crc" -eq 0 ] && [ "$lrc" -ne 0 ]; then
      echo "  FAIL: $lane $name: child proved, lane exits $lrc"
      fail=1
    else
      echo "  ok: $lane $name: ${lst#% SZS status } exit $lrc;" \
           "$(echo "$lstats" | tr '\n' ' ')"
    fi
  done
done
if [ "$fail" = 0 ]; then
  echo "LANE CLI OK"
else
  echo "LANE CLI FAILED"
  exit 1
fi
