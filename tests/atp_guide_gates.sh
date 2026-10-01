#!/bin/bash
# ENIGMA gates G2.2 and G2.4 on FindProof (docs/plans/enigma_portfolio.md,
# M2; the G2.2 reading for clock-gated lanes is amendment A2 of section
# 5.4).  Runs on a pod, never on the Mac.
#
# G2.2 (zero correction is exact): for every (family, lane, problem) the
# base lane run
#   FindProof --lane=L --proof --szs --work-clock=instr --wall=W p
# and its zero twin (the same plus --guide=F --guide-zero) must agree on
#   - stdout (status and proof), after masking timings;
#   - the engine stats on stderr (LRS prune and recompute counters
#     included), after masking timings, the goal path and the twin's
#     "% guide:" line;
#   - every selection (THVM_ATP_CP_PICK_TRACE: pick, seq, pri);
#   - the clock-gated decisions (THVM_ATP_CLOCK_TRACE), clocks masked.
# A lane whose run logs no clock-gated decision must agree on all four.
# A clock-gated lane may drift once a decision reads a clock that the
# twin's few dozen out-of-window instructions per step moved: it passes
# when the selections agree up to the first decision whose line differs,
# and the script prints that selection index.  The base run's environment
# carries THVM_ATP_GUIDP* variables of the same lengths as the twin's
# THVM_ATP_GUIDE*, because an uncached getenv costs instructions in
# proportion to the environment's size.  The twin must report a "% guide:"
# line with picks > 0 and a frac_wall field: a layer that never picked
# (the KD-heap or the top-K re-ranker armed, for instance) fails.
#
# G2.4 (C and Python agree), when HEAD names the head's safetensors (the
# binary must have it compiled in; MODEL gives its id): the non-zero twin
# runs with THVM_ATP_GUIDE_DUMP and tools/atp_enigma/weights_to_c.py
# parity rescores the dump.  FEATPARITY=1 also exports the run's history
# (rate 1, push features), converts it with np0_extract.py and runs
# weights_to_c.py featparity.
#
# Usage: tests/atp_guide_gates.sh [bin/FindProof] [spec]
#   spec: lines "family lane problem..." (the plan wants 8 problems per
#   family); default: the twee family on two repo problems.
#   WALL (30) is each run's --wall; OUT keeps the runs (default: a
#   temporary directory, removed on exit); DUMP_MAX (100000) caps the
#   dumped picks.
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
BIN=${1:-$ROOT/bin/FindProof}
SPEC=${2:-}
WALL=${WALL:-30}
MODEL=${MODEL:-}
HEAD=${HEAD:-}
DUMP_MAX=${DUMP_MAX:-100000}
FEATPARITY=${FEATPARITY:-0}
W2C=$ROOT/tools/atp_enigma/weights_to_c.py
NP0=$ROOT/tools/atp_enigma/np0_extract.py
if [ -n "${OUT:-}" ]; then
  mkdir -p "$OUT"
else
  OUT=$(mktemp -d)
  trap 'rm -rf "$OUT"' EXIT
fi
if [ -z "$SPEC" ]; then
  SPEC=$OUT/spec
  printf 'twee twee %s %s\n' \
    "$ROOT/wl/Examples/proover/problems/BOO001-1.p" \
    "$ROOT/tools/baselines/wm_pr/GroupAxioms__InverseOfComposite.pr" > "$SPEC"
fi
fail=0
n_pass=0
n_fail=0

# Timings, the goal path (it carries a pid) and the twin's own line out.
mask() {
  sed -E -e '/^% guide: /d' -e '/steps\/sec=/d' -e 's/goal=[^ ]*/goal=<g>/g' \
         -e 's/[0-9]+(\.[0-9]+)?(s|ms|us)([^A-Za-z0-9_]|$)/<t>\3/g' "$1"
}

# Engine stderr without the per-pick trace lines.
stats() {
  grep -v -e '^CPSEL ' -e '^CPSKIP ' "$1" | mask /dev/stdin
}

# One selection per line: pick, seq, pri.
sels() {
  grep '^CPSEL ' "$1" | awk '{print $2, $4, $5}'
}

# The clock-gated decisions, clocks masked, in order.
decisions() {
  cat "$1"/clock.*.tsv 2>/dev/null | grep -v '^#' \
    | sed -E -e 's/ wall=[0-9]+ cpu=[0-9]+ work=[0-9]+$//'
}

# run <dir> <env...> -- <flags...>: one FindProof lane run.
run() {
  local dir=$1
  shift
  local envs=()
  while [ "$1" != "--" ]; do
    envs+=("$1")
    shift
  done
  shift
  mkdir -p "$dir/clock"
  env ${envs[@]+"${envs[@]}"} THVM_ATP_CP_PICK_TRACE=1 THVM_ATP_CLOCK_TRACE="$dir/clock" \
    "$BIN" "$@" --proof --szs --work-clock=instr --wall="$WALL" \
    > "$dir/out" 2> "$dir/err" < /dev/null
}

verdict() {
  if [ "$1" = PASS ]; then
    n_pass=$((n_pass + 1))
  else
    n_fail=$((n_fail + 1))
    fail=1
  fi
  echo "$1 $2"
}

while read -r fam lane probs; do
  case "$fam" in
    ''|'#'*) continue ;;
  esac
  for p in $probs; do
    name=$(basename "$p")
    d=$OUT/$fam/$lane/$name
    mkdir -p "$d"
    gflags=(--lane="$lane" --guide="$fam" --guide-zero)
    pads=(THVM_ATP_GUIDP="$fam" THVM_ATP_GUIDP_ZERO=1)
    if [ -n "$MODEL" ]; then
      gflags+=(--guide-model="$MODEL")
      pads+=(THVM_ATP_GUIDP_MODEL="$MODEL")
    fi
    run "$d/base" "${pads[@]}" -- --lane="$lane" "$p"
    run "$d/twin" -- "${gflags[@]}" "$p"
    why=""
    gl=$(grep -m1 '^% guide: ' "$d/twin/err")
    picks=$(echo "$gl" | sed -n -E 's/.* picks=([0-9]+) .*/\1/p')
    if [ -z "$gl" ] || [ "${picks:-0}" -eq 0 ]; then
      why="the twin reports no guided pick ('${gl:-no % guide: line}')"
    elif ! echo "$gl" | grep -q ' frac_wall='; then
      why="the twin's % guide: line has no frac_wall"
    fi
    sels "$d/base/err" > "$d/base.sel"
    sels "$d/twin/err" > "$d/twin.sel"
    decisions "$d/base/clock" > "$d/base.dec"
    decisions "$d/twin/clock" > "$d/twin.dec"
    mask "$d/base/out" > "$d/base.out.m"
    mask "$d/twin/out" > "$d/twin.out.m"
    stats "$d/base/err" > "$d/base.stats"
    stats "$d/twin/err" > "$d/twin.stats"
    nsel=$(wc -l < "$d/base.sel" | tr -d ' ')
    ndec=$(wc -l < "$d/base.dec" | tr -d ' ')
    if [ -z "$why" ] && [ "$ndec" -eq 0 ] && [ "$(wc -l < "$d/twin.dec" | tr -d ' ')" -eq 0 ]; then
      if ! cmp -s "$d/base.sel" "$d/twin.sel"; then
        first=$(cmp "$d/base.sel" "$d/twin.sel" 2>&1 | sed -n -E 's/.*line ([0-9]+).*/\1/p')
        why="selection ${first:-?} of $nsel differs"
      elif ! cmp -s "$d/base.out.m" "$d/twin.out.m"; then
        why="stdout (status or proof) differs"
      elif ! cmp -s "$d/base.stats" "$d/twin.stats"; then
        why="engine stats differ: $(diff "$d/base.stats" "$d/twin.stats" | head -3 | tr '\n' ' ')"
      fi
      [ -z "$why" ] && verdict PASS "G2.2 $fam $lane $name: $nsel selections, no clock-gated decision"
    elif [ -z "$why" ]; then
      dline=$(cmp "$d/base.dec" "$d/twin.dec" 2>&1 | sed -n -E 's/.*line ([0-9]+).*/\1/p')
      if [ -z "$dline" ] && cmp -s "$d/base.dec" "$d/twin.dec"; then
        cut=$nsel
        if ! cmp -s "$d/base.sel" "$d/twin.sel"; then
          why="selections differ with identical clock-gated decisions"
        elif ! cmp -s "$d/base.out.m" "$d/twin.out.m"; then
          why="stdout (status or proof) differs"
        elif ! cmp -s "$d/base.stats" "$d/twin.stats"; then
          why="engine stats differ: $(diff "$d/base.stats" "$d/twin.stats" | head -3 | tr '\n' ' ')"
        fi
        [ -z "$why" ] && verdict PASS "G2.2 $fam $lane $name: $nsel selections, $ndec decisions identical"
      else
        # The first differing decision: compare the selections before it.
        ln=${dline:-$((ndec + 1))}
        bsel=$(sed -n "${ln}p" "$d/base.dec" | sed -n -E 's/.* sel=([0-9]+).*/\1/p')
        tsel=$(sed -n "${ln}p" "$d/twin.dec" | sed -n -E 's/.* sel=([0-9]+).*/\1/p')
        cut=${bsel:-$tsel}
        if [ -n "$tsel" ] && [ -n "$bsel" ] && [ "$tsel" -lt "$bsel" ]; then
          cut=$tsel
        fi
        cut=${cut:-0}
        if ! cmp -s <(head -n "$cut" "$d/base.sel") <(head -n "$cut" "$d/twin.sel"); then
          why="a selection before the first drifted decision (selection $cut) differs"
        fi
        [ -z "$why" ] && verdict PASS "G2.2 $fam $lane $name: identical through selection $cut, where decision $ln drifted (A2); $nsel base selections"
      fi
    fi
    [ -n "$why" ] && verdict FAIL "G2.2 $fam $lane $name: $why"

    if [ -n "$HEAD" ]; then
      nz=(--lane="$lane" --guide="$fam")
      if [ -n "$MODEL" ]; then
        nz+=(--guide-model="$MODEL")
      fi
      envs=(THVM_ATP_GUIDE_DUMP="$d/dump.tsv" THVM_ATP_GUIDE_DUMP_MAX="$DUMP_MAX")
      if [ "$FEATPARITY" = 1 ]; then
        envs+=(THVM_ATP_HISTORY_EXPORT="$d/hx.bin" THVM_ATP_HISTORY_RATE=1
               THVM_ATP_HISTORY_CPFEAT=1)
      fi
      rm -f "$d/dump.tsv" "$d/hx.bin"
      run "$d/guided" "${envs[@]}" -- "${nz[@]}" "$p"
      gl=$(grep -m1 '^% guide: ' "$d/guided/err")
      scored=$(echo "$gl" | sed -n -E 's/.* scored=([0-9]+) .*/\1/p')
      if [ "${scored:-0}" -eq 0 ] || [ ! -s "$d/dump.tsv" ]; then
        verdict FAIL "G2.4 $fam $lane $name: no scored pick dumped ('${gl:-no % guide: line}')"
      elif python3 "$W2C" parity "$HEAD" "$d/dump.tsv" "$d/parity.txt" > /dev/null; then
        verdict PASS "G2.4 $fam $lane $name: $(head -1 "$d/parity.txt")"
      else
        verdict FAIL "G2.4 $fam $lane $name: $(grep -v '^PARITY' "$d/parity.txt" | tr '\n' ' ')"
      fi
      if [ "$FEATPARITY" = 1 ] && [ -s "$d/dump.tsv" ]; then
        if python3 "$NP0" "$d/hx.bin" "$d/guided/out" "$d/run.npz" 1000000000 > "$d/np0.txt" 2>&1 &&
           python3 "$W2C" featparity "$d/run.npz" "$d/dump.tsv" "$d/featparity.txt" > /dev/null; then
          verdict PASS "featparity $fam $lane $name: $(head -1 "$d/featparity.txt")"
        else
          verdict FAIL "featparity $fam $lane $name: $(tail -4 "$d/featparity.txt" "$d/np0.txt" 2>/dev/null | tr '\n' ' ')"
        fi
      fi
    fi
  done
done < "$SPEC"

echo "GUIDE GATES: $n_pass pass, $n_fail fail (runs in $OUT)"
if [ "$fail" != 0 ]; then
  exit 1
fi
