#!/bin/bash
# ENIGMA gates G2.2 and G2.4 on FindProof (docs/ATP/enigma_portfolio.md,
# M2; G2.2 reads exact identity under the steps clock, amendment A8).
# Runs on a pod, never on the Mac.
#
# G2.2 (zero correction is exact): for every (family, lane, problem) the
# base lane run
#   THVM_ATP_WORK_CLOCK=steps FindProof --lane=L --proof --szs --wall=W p
# and its zero twin (the same with THVM_ATP_GUIDE=F THVM_ATP_GUIDE_ZERO=1,
# the variables --guide=F --guide-zero set) must agree on
#   - stdout (status and proof), after masking timings;
#   - the engine stats on stderr (LRS prune and recompute counters
#     included), after masking timings, the goal path and the twin's
#     "% guide:" line;
#   - every selection (THVM_ATP_CP_PICK_TRACE: pick, seq, pri);
#   - the clock-gated decisions (THVM_ATP_CLOCK_TRACE), clocks masked.
# Under the steps clock an observer cannot advance the clock the
# disciplines read, so every row must agree on all four: a clock-gated
# decision taken elsewhere is a FAIL (A8 withdrew A2's drift reading).
# The base run's environment carries THVM_ATP_GUIDP* variables of the
# same lengths as the twin's THVM_ATP_GUIDE*, because an uncached getenv
# costs instructions in proportion to the environment's size (the
# front-end budgets keep the instruction clock).  The twin must report a
# "% guide:" line with picks > 0 and a frac_wall field: a layer that never
# picked (phi(p) missing for a head that reads it, the KD-heap or the
# top-K re-ranker armed) fails.
#
# AA=1 runs the base twice: when the two base runs differ the row is
# NONDET (it cannot test the twin) and does not count.  DUMP_EVERY=N
# gives the twin the loop-0 harvest's sampled dump (THVM_ATP_GUIDE_DUMP,
# THVM_ATP_GUIDE_DUMP_EVERY=N, DUMP_MAX picks), which must not change the
# search either.  Per (family, lane) the rows count in list order: the
# lane passes when no row FAILs and MIN_PASS (default 8, the plan's
# count) rows PASS; fewer leave it INCONCLUSIVE.
#
# G2.4 (C and Python agree), when HEAD names the head's safetensors (the
# binary must have it compiled in; MODEL gives its id): the non-zero twin
# runs with THVM_ATP_GUIDE_DUMP and tools/atp_enigma/weights_to_c.py
# parity rescores the dump.  FEATPARITY=1 also exports the run's history
# (rate 1, push features), converts it with np0_extract.py and runs
# weights_to_c.py featparity.  A run that proved but whose proof FindProof
# withheld has no proof block, so its export converts in np0_extract's
# no-proof mode, as the guided harvest converts it: featparity reads the
# push, selection and deletion records, which both modes keep, and no
# label (amendment A31).
#
# Usage: tests/atp_guide_gates.sh [bin/FindProof] [spec]
#   spec: lines "family lane problem..." (the plan wants 8 problems per
#   family); default: the twee family on two repo problems.
#   WALL (30) is each run's --wall; OUT keeps the runs (default: a
#   temporary directory, removed on exit); DUMP_MAX (100000) caps the
#   dumped picks; MODEL picks the compiled-in head (default: the first).
#   TPTP must name the root the problems' include() directives resolve
#   against.  Writes OUT/g22.tsv (one row per run problem) and
#   OUT/g22.summary (one line per family and lane).
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
BIN=${1:-$ROOT/bin/FindProof}
SPEC=${2:-}
WALL=${WALL:-30}
MODEL=${MODEL:-}
HEAD=${HEAD:-}
DUMP_MAX=${DUMP_MAX:-100000}
DUMP_EVERY=${DUMP_EVERY:-}
AA=${AA:-0}
MIN_PASS=${MIN_PASS:-8}
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
n_nondet=0
: > "$OUT/g22.tsv"

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

# run <dir> <env...> -- <flags...>: one FindProof lane run, reduced to
# its four compared parts.
run() {
  local dir=$1
  shift
  local envs=()
  while [ "$1" != "--" ]; do
    envs+=("$1")
    shift
  done
  shift
  rm -rf "$dir"
  mkdir -p "$dir/clock"
  env ${envs[@]+"${envs[@]}"} THVM_ATP_WORK_CLOCK=steps THVM_ATP_CP_PICK_TRACE=1 THVM_ATP_CLOCK_TRACE="$dir/clock" \
    "$BIN" "$@" --proof --szs --wall="$WALL" \
    > "$dir/out" 2> "$dir/err" < /dev/null
  sels "$dir/err" > "$dir.sel"
  decisions "$dir/clock" > "$dir.dec"
  mask "$dir/out" > "$dir.out.m"
  stats "$dir/err" > "$dir.stats"
}

# same <a> <b>: the two runs agree on all four parts; else names the
# first that differs.
same() {
  if ! cmp -s "$1.sel" "$2.sel"; then
    local first
    first=$(cmp "$1.sel" "$2.sel" 2>&1 | sed -n -E 's/.*line ([0-9]+).*/\1/p')
    echo "selection ${first:-?} of $(wc -l < "$1.sel" | tr -d ' ') differs"
  elif ! cmp -s "$1.dec" "$2.dec"; then
    local ln
    ln=$(cmp "$1.dec" "$2.dec" 2>&1 | sed -n -E 's/.*line ([0-9]+).*/\1/p')
    echo "clock-gated decision ${ln:-?} differs: $(sed -n "${ln:-1}p" "$1.dec" | cut -c1-80) | $(sed -n "${ln:-1}p" "$2.dec" | cut -c1-80)"
  elif ! cmp -s "$1.out.m" "$2.out.m"; then
    echo "stdout (status or proof) differs"
  elif ! cmp -s "$1.stats" "$2.stats"; then
    echo "engine stats differ: $(diff "$1.stats" "$2.stats" | head -3 | tr '\n' ' ' | cut -c1-200)"
  fi
}

verdict() {
  case $1 in
    PASS) n_pass=$((n_pass + 1)) ;;
    NONDET) n_nondet=$((n_nondet + 1)) ;;
    *) n_fail=$((n_fail + 1)); fail=1 ;;
  esac
  echo "$1 $2"
}

idx=0
while read -r fam lane probs; do
  case "$fam" in
    ''|'#'*) continue ;;
  esac
  for p in $probs; do
    idx=$((idx + 1))
    name=$(basename "$p")
    d=$OUT/$fam/$lane/$name
    mkdir -p "$d"
    # The twin arms through the variables --guide, --guide-zero and
    # --guide-model set; the base carries the same names with P for E, in
    # the same places, so both environments have one layout.
    tenvs=(THVM_ATP_GUIDE="$fam" THVM_ATP_GUIDE_ZERO=1)
    pads=(THVM_ATP_GUIDP="$fam" THVM_ATP_GUIDP_ZERO=1)
    if [ -n "$MODEL" ]; then
      tenvs+=(THVM_ATP_GUIDE_MODEL="$MODEL")
      pads+=(THVM_ATP_GUIDP_MODEL="$MODEL")
    fi
    if [ -n "$DUMP_EVERY" ]; then
      tenvs+=(THVM_ATP_GUIDE_DUMP="$d/twin.dump" THVM_ATP_GUIDE_DUMP_EVERY="$DUMP_EVERY"
              THVM_ATP_GUIDE_DUMP_MAX="$DUMP_MAX")
      pads+=(THVM_ATP_GUIDP_DUMP="$d/base.dump" THVM_ATP_GUIDP_DUMP_EVERY="$DUMP_EVERY"
             THVM_ATP_GUIDP_DUMP_MAX="$DUMP_MAX")
      rm -f "$d/twin.dump"
    fi
    run "$d/base" "${pads[@]}" -- --lane="$lane" "$p"
    why=""
    v=FAIL
    if [ "$AA" = 1 ]; then
      run "$d/base2" "${pads[@]}" -- --lane="$lane" "$p"
      aa=$(same "$d/base" "$d/base2")
      if [ -n "$aa" ]; then
        v=NONDET
        why="the base does not repeat: $aa"
      fi
    fi
    nsel=$(wc -l < "$d/base.sel" | tr -d ' ')
    ndec=$(wc -l < "$d/base.dec" | tr -d ' ')
    if [ "$v" != NONDET ]; then
      run "$d/twin" "${tenvs[@]}" -- --lane="$lane" "$p"
      gl=$(grep -m1 '^% guide: ' "$d/twin/err")
      picks=$(echo "$gl" | sed -n -E 's/.* picks=([0-9]+) .*/\1/p')
      if [ -z "$gl" ] || [ "${picks:-0}" -eq 0 ]; then
        why="the twin reports no guided pick ('${gl:-no % guide: line}'; $(grep -m1 '^guide: ' "$d/twin/err" | cut -c1-120))"
      elif ! echo "$gl" | grep -q ' frac_wall='; then
        why="the twin's % guide: line has no frac_wall"
      elif [ -n "$DUMP_EVERY" ] && ! grep -q '^# guide end ' "$d/twin.dump" 2>/dev/null; then
        why="the twin's dump has no footer"
      else
        why=$(same "$d/base" "$d/twin")
      fi
      if [ -z "$why" ]; then
        v=PASS
        why="$nsel selections, $ndec clock-gated decisions, identical"
      fi
    fi
    verdict "$v" "G2.2 $fam $lane $name: $why"
    printf '%s\t%s\t%s\t%s\t%s\t%s\t%d\n' "$fam" "$lane" "$name" "$v" "$nsel" "$why" "$idx" >> "$OUT/g22.tsv"

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
        rm -f "$d/run.npz" "$d/featparity.txt"
        python3 "$NP0" "$d/hx.bin" "$d/guided/out" "$d/run.npz" 1000000000 > "$d/np0.txt" 2>&1
        np0=$?
        if [ "$np0" != 0 ] && grep -q '^no proof block' "$d/np0.txt" &&
           grep -q '^FindProof: proof withheld' "$d/guided/err"; then
          python3 "$NP0" "$d/hx.bin" - "$d/run.npz" 1000000000 >> "$d/np0.txt" 2>&1
          np0=$?
        fi
        if [ "$np0" = 0 ] &&
           python3 "$W2C" featparity "$d/run.npz" "$d/dump.tsv" "$d/featparity.txt" > /dev/null; then
          verdict PASS "featparity $fam $lane $name: $(head -1 "$d/featparity.txt")"
        elif [ "$np0" != 0 ]; then
          verdict FAIL "featparity $fam $lane $name: np0_extract: $(tail -2 "$d/np0.txt" | tr '\n' ' ')"
        else
          verdict FAIL "featparity $fam $lane $name: $(tail -4 "$d/featparity.txt" | tr '\n' ' ')"
        fi
      fi
    fi
  done
done < "$SPEC"

# Per family and lane, in list order: any FAIL fails; MIN_PASS PASS rows
# pass; NONDET rows do not count.
sort -t$'\t' -k7,7n "$OUT/g22.tsv" | awk -F'\t' -v need="$MIN_PASS" '
  { k = $1 "\t" $2; if (!(k in n)) order[++m] = k; n[k]++; v[k, $4]++ }
  END {
    printf "family\tlane\trows\tpass\tfail\tnondet\tG2.2\n"
    for (i = 1; i <= m; i++) {
      k = order[i]
      g = (v[k, "FAIL"] > 0) ? "FAIL" : (v[k, "PASS"] >= need) ? "PASS" : "INCONCLUSIVE"
      printf "%s\t%d\t%d\t%d\t%d\t%s\n", k, n[k], v[k, "PASS"] + 0, v[k, "FAIL"] + 0, v[k, "NONDET"] + 0, g
    }
  }' > "$OUT/g22.summary"
cat "$OUT/g22.summary"
echo "GUIDE GATES: $n_pass pass, $n_fail fail, $n_nondet nondet (runs in $OUT)"
if [ "$fail" != 0 ]; then
  exit 1
fi
