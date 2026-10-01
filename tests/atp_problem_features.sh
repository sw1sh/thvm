#!/bin/bash
# FindProof --problem-features: the static gate replica and the gate bits
# of phi(p) against the live lanes (docs/plans/enigma_portfolio.md,
# section 3.1).  Runs on pods only.
#
# 1. phi(p) and the replica (--problem-features --lanes) for every
#    candidate problem; each must succeed.  The time of phi(p) is kept per
#    problem: its maximum is what a schedule parent pays before it
#    launches the lanes.
# 2. N problems (default 20) are picked so that every gate bit and every
#    lane verdict occurs both ways where the set allows.
# 3. Every lane that arms an exit-fast gate, plus wm, bare and bare-raw,
#    runs alone on each picked problem (--lane=NAME, no --szs, so a Horn
#    release that reaches the goal does not end the run before the gates),
#    on BIN and on OLD_FP.  Where the replica is strict:
#    - the lane's first gate line must be the predicted one (none for a
#      live lane), on both binaries;
#    - a lane it calls dead must print it within max(F, 2 x its problem's
#      phi(p) time);
#    - the gate bit of that lane's gate must agree with what the lane did:
#      0 when it printed its own gate's line, 1 when it got past that gate
#      (printed a later stage's line or none); rows that stopped at an
#      earlier stage say nothing about the bit.  The pairs: ifeq /
#      ifeq_dispatcher, horn-cd and horn-back / horn_slice, unfold* /
#      def_unfold, tgt-ax / ax_ground_compound, tgt-sel / conj_cone_partial,
#      equals and equals-twee / equals_goal, narrowq / goal_existential,
#      wm and bare / fe_live; otter-* one way only (distrib_law is only the
#      shape: distrib_law=0 means the otter line).  Every bit must be
#      checked both ways (otter: the one way).
#    Non-strict predictions (a Horn release can change the gate's input)
#    are reported, not judged.
#
# Usage: OLD_FP=<FindProof> tests/atp_problem_features.sh [-n N] [-w WALL]
#          [-f F] [-j JOBS] [-D DEADLINE] [-o DIR] BIN PROBLEMS
#   BIN       bin/FindProof
#   OLD_FP    FindProof built from main before the gate refactor (d6de773a),
#             required: the replica and the lanes share the refactored
#             predicates, so only the old binary checks them independently
#             here (matrix_tables.py --t7c is the other check, over the
#             pool rows of the lane matrix)
#   PROBLEMS  a directory searched for *.p, or a file of problem paths
#   -w        a lane run's --wall (default 15); -f the floor of the
#             dead-lane deadline (default 1 s); -j parallel runs (default
#             nproc/2); -D the whole run's deadline in seconds (default
#             7200: no job starts after it); -o keeps the work files and
#             the sentinels (started, done) in DIR (lane stderr is never
#             kept)
# TPTP must name the root the problems' include() directives resolve
# against (for the J13 set: the directory holding UEQ/ and Axioms/).
#
# Pod acceptance of the enigma-phi change (plan gate G2.3 and the gate
# refactor): bin/test_atp 136835/136835, the OA pin 257971/735/2461555, the
# DN pin 2848/254/768876, bin/test_atp_problem_features, this script with
# OLD_FP on the pool and J13 problem sets, and matrix_tables.py --t7c over
# --problem-features --lanes of the 835 pool problems.
#
# J13 seal (plan section 1.4): only gate lines are read.  No SZS status,
# winner or solve time is printed or kept; a run time is kept only for a
# lane that printed the gate line the replica predicted for it (it ended
# there).
set -u

# The exit-fast and front-end give-up lines, and the bench's .pr reader
# ending the run.  The backward condensed-detachment lane's "no proof
# within budget" is a search result, not a gate.
STATIC_RE='FindProof: (fof slice: |ifeq-guard slice: |horn-(cd|back) slice: (no encoded|existential|oversized)|def-unfold slice: |tgt-ax slice: |tgt-sel slice: |equals-goal slice: |narrow-queue slice: |otter slice: |no negated conjecture|problem exceeds|functor arity exceeds|axiom selection cannot fit|canonical (clause|term) exceeds|curried goal exceeds|term exceeds side-length bound|cannot make a fresh)|pr parse error \('

BITS="goal_existential equals_goal ifeq_dispatcher horn_slice def_unfold ax_ground_compound conj_cone_partial distrib_law fe_live sine"

now() {
  date +%s.%N 2> /dev/null || date +%s
}

# The gate lines of one run's stderr, one per line, without the
# run-specific .pr path.
static_lines() {
  grep -E "$STATIC_RE" "$1" | head -n 3 |
    sed -e 's/^.*FindProof: //' -e 's/^pr parse error ([^)]*): /pr parse error: /'
}

# Does a line of file $2 start with $1?
has_line() {
  awk -v r="$1" 'index($0, r) == 1 { f = 1 } END { exit !f }' "$2"
}

run_lane() {
  env -i PATH="$PATH" HOME="${HOME:-/tmp}" TPTP="${TPTP:-}" THVM_ATP_RSS_ABORT_MB=2000 \
    timeout -k 5 $(($3 + 15)) "$1" --lane="$2" --wall="$3" "$4" \
    > /dev/null 2> "$5" < /dev/null
}

past_deadline() {
  awk -v n="$(now)" -v d="$1" 'BEGIN { exit !(n > d) }'
}

# --feat ID WORK BIN DEADLINE: phi(p), its time, and the replica rows of
# candidate ID.
if [ "${1:-}" = "--feat" ]; then
  id=$2
  work=$3
  bin=$4
  deadline=$5
  path=$(sed -n "${id}p" "$work/cands.txt")
  if past_deadline "$deadline"; then
    echo "$path: not run, the deadline passed" > "$work/feat/$id.err"
    exit 0
  fi
  t0=$(now)
  env -i PATH="$PATH" HOME="${HOME:-/tmp}" TPTP="${TPTP:-}" \
    timeout 300 "$bin" --problem-features "$path" > "$work/feat/$id.phi.tmp" 2> /dev/null
  rc1=$?
  t1=$(now)
  env -i PATH="$PATH" HOME="${HOME:-/tmp}" TPTP="${TPTP:-}" \
    timeout 300 "$bin" --problem-features --lanes "$path" > "$work/feat/$id.lanes.tmp" 2> /dev/null
  rc2=$?
  if [ $rc1 -ne 0 ] || [ $rc2 -ne 0 ] || [ ! -s "$work/feat/$id.phi.tmp" ]; then
    echo "$path: --problem-features exit $rc1, --lanes exit $rc2" > "$work/feat/$id.err"
  else
    awk -v p="$path" '{ print p "\t" $0 }' "$work/feat/$id.phi.tmp" > "$work/feat/$id.phi"
    awk -v p="$path" '{ print p "\t" $0 }' "$work/feat/$id.lanes.tmp" > "$work/feat/$id.lanes"
    awk -v p="$path" -v a="$t0" -v b="$t1" 'BEGIN { printf "%s\t%.3f\n", p, b - a }' \
      > "$work/feat/$id.time"
  fi
  rm -f "$work/feat/$id.phi.tmp" "$work/feat/$id.lanes.tmp"
  exit 0
fi

# --job ID WORK BIN WALL F DEADLINE: one lane run on BIN and OLD_FP,
# judged against the replica.
if [ "${1:-}" = "--job" ]; then
  id=$2
  work=$3
  bin=$4
  wall=$5
  floor=$6
  deadline=$7
  IFS=$'\t' read -r path name lane verdict strict gates reason tphi \
    < <(sed -n "${id}p" "$work/jobs.tsv")
  if past_deadline "$deadline"; then
    printf 'SKIP\t%s\t%s\t%s\t%s\t%s\t%s\t-\tnot run, the deadline passed\t-\n' \
      "$name" "$lane" "$verdict" "$strict" "$gates" "$reason" > "$work/res/$id"
    exit 0
  fi
  err="$work/err.$id"
  t0=$(now)
  run_lane "$bin" "$lane" "$wall" "$path" "$err"
  t1=$(now)
  el=$(awk -v a="$t0" -v b="$t1" 'BEGIN { printf "%.1f", b - a }')
  limit=$(awk -v f="$floor" -v t="$tphi" 'BEGIN { l = 2 * t; if (l < f) l = f; printf "%.1f", l }')
  static_lines "$err" > "$err.lines"
  first=$(head -n 1 "$err.lines")
  seen=$(tr '\n' '|' < "$err.lines")
  if [ "$reason" != "-" ]; then
    has_line "$reason" "$err.lines"
    hit=$?
  else
    hit=1
    if [ ! -s "$err.lines" ]; then
      hit=0
    fi
  fi
  status=PASS
  detail=""
  if [ "$strict" = 1 ]; then
    if [ $hit -ne 0 ]; then
      status=FAIL
      if [ "$verdict" = dead ] && [ ! -s "$err.lines" ]; then
        detail="dead lane printed no gate line"
      else
        detail="gate lines: ${seen:-none}"
      fi
    elif [ "$verdict" = dead ] && ! awk -v e="$el" -v l="$limit" 'BEGIN { exit !(e <= l) }'; then
      status=FAIL
      detail="dead lane took ${el}s to its gate line (limit ${limit}s)"
    fi
  else
    status=INFO
    if [ $hit -eq 0 ]; then
      detail="agrees"
    else
      detail="differs, gate lines: ${seen:-none}"
    fi
  fi
  run_lane "$OLD_FP" "$lane" "$wall" "$path" "$err.old"
  old=$(static_lines "$err.old" | tr '\n' '|')
  if [ "$old" != "$seen" ]; then
    status=FAIL
    detail="${detail:+$detail; }OLD_FP gate lines: ${old:-none}, new: ${seen:-none}"
  fi
  rm -f "$err" "$err.old" "$err.lines"
  # The run time of a lane that did not end at its predicted gate line can
  # be a search's, and on a J13 problem a solve time (the seal).
  if [ "$verdict" != dead ] || [ $hit -ne 0 ]; then
    el=-
  fi
  printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' "$status" "$name" "$lane" \
    "$verdict" "$strict" "$gates" "$reason" "$el" "$detail" "${first:--}" \
    > "$work/res/$id"
  exit 0
fi

usage() {
  sed -n '2,/^set -u/p' "$0" | sed '$d' | sed 's/^# \{0,1\}//'
}

N=20
WALL=15
FLOOR=1
DEADLINE_S=7200
JOBS=$(( $(nproc 2> /dev/null || echo 2) / 2 ))
KEEP=""
while getopts "n:w:f:j:D:o:h" opt; do
  case $opt in
    n) N=$OPTARG ;;
    w) WALL=$OPTARG ;;
    f) FLOOR=$OPTARG ;;
    j) JOBS=$OPTARG ;;
    D) DEADLINE_S=$OPTARG ;;
    o) KEEP=$OPTARG ;;
    *) usage; exit 2 ;;
  esac
done
shift $((OPTIND - 1))
if [ $# -ne 2 ]; then
  usage
  exit 2
fi
if [ "$JOBS" -lt 1 ]; then
  JOBS=1
fi
BIN=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
if [ ! -x "$BIN" ]; then
  echo "atp_problem_features: no binary $1"
  exit 2
fi
if [ -z "${OLD_FP:-}" ] || [ ! -x "$OLD_FP" ]; then
  echo "atp_problem_features: OLD_FP must name the FindProof built from main before the gate refactor"
  exit 2
fi
OLD_FP=$(cd "$(dirname "$OLD_FP")" && pwd)/$(basename "$OLD_FP")
export OLD_FP
if [ -n "$KEEP" ]; then
  WORK=$KEEP
  mkdir -p "$WORK"
else
  WORK=$(mktemp -d)
  trap 'rm -rf "$WORK"' EXIT
fi
rm -rf "$WORK/feat" "$WORK/res" "$WORK/done"
mkdir -p "$WORK/feat" "$WORK/res"
DEADLINE=$(awk -v n="$(now)" -v d="$DEADLINE_S" 'BEGIN { printf "%.3f", n + d }')
printf 'started=%s bin=%s old_fp=%s deadline_s=%s\n' "$(date -u +%FT%TZ)" "$BIN" \
  "$OLD_FP" "$DEADLINE_S" > "$WORK/started"

finish() {
  printf 'status=%s finished=%s\n' "$1" "$(date -u +%FT%TZ)" > "$WORK/done"
  exit "$2"
}

if [ -d "$2" ]; then
  find "$(cd "$2" && pwd)" -name '*.p' | sort > "$WORK/cands.txt"
else
  grep -v '^[[:space:]]*$' "$2" > "$WORK/cands.txt"
fi
n_cand=$(wc -l < "$WORK/cands.txt" | tr -d ' ')
if [ "$n_cand" -eq 0 ]; then
  echo "atp_problem_features: no problems in $2"
  finish error 2
fi

# 1. Features, their time and the replica rows for every candidate.
"$BIN" --problem-features --header > "$WORK/phi_header.tsv"
for b in $BITS; do
  if ! tr '\t' '\n' < "$WORK/phi_header.tsv" | grep -qx "$b"; then
    echo "atp_problem_features: --header has no column $b"
    finish error 2
  fi
done
seq 1 "$n_cand" | xargs -P "$JOBS" -I{} bash "$0" --feat {} "$WORK" "$BIN" "$DEADLINE"
cat "$WORK"/feat/*.phi > "$WORK/phi.tsv" 2> /dev/null
cat "$WORK"/feat/*.lanes > "$WORK/lanes.tsv" 2> /dev/null
cat "$WORK"/feat/*.time > "$WORK/phi_time.tsv" 2> /dev/null
n_err=$(cat "$WORK"/feat/*.err 2> /dev/null | wc -l | tr -d ' ')
fail=0
if [ "$n_err" -gt 0 ]; then
  echo "  FAIL: --problem-features failed on $n_err of $n_cand problems:"
  cat "$WORK"/feat/*.err | head -n 20 | sed 's/^/    /'
  fail=1
else
  echo "  ok: phi(p) and the replica for all $n_cand problems"
fi
sort -t$'\t' -k2,2n "$WORK/phi_time.tsv" | awk -F'\t' '
  { t[++n] = $2 }
  END {
    if (n > 0) {
      printf "  phi(p) time over %d problems: median %.3fs, max %.3fs (what a schedule parent pays before it launches the lanes)\n", n, t[int((n + 1) / 2)], t[n]
    }
  }'

# 2. Pick N problems covering every gate bit and lane verdict both ways,
# first in cksum order of the name, then filling in that order.
while IFS= read -r p; do
  printf '%s\t%s\n' "$(printf '%s' "$(basename "$p")" | cksum | cut -d' ' -f1)" "$p"
done < <(cut -f1 "$WORK/phi.tsv") | sort -n | cut -f2 > "$WORK/order.txt"
awk -F'\t' -v N="$N" -v BITS="$BITS" '
  FILENAME == ARGV[1] {
    for (i = 1; i <= NF; i++) {
      col[$i] = i + 1
    }
    next
  }
  FILENAME == ARGV[2] {
    nb = split(BITS, bits, " ")
    for (b = 1; b <= nb; b++) {
      keys[$1] = keys[$1] "\034B:" bits[b] ":" ($(col[bits[b]]) + 0)
    }
    next
  }
  FILENAME == ARGV[3] {
    keys[$1] = keys[$1] "\034L:" $3 ":" $4
    next
  }
  {
    order[++no] = $0
  }
  END {
    for (i = 1; i <= no && picked < N; i++) {
      p = order[i]
      m = split(keys[p], ks, "\034")
      gain = 0
      for (k = 1; k <= m; k++) {
        if (ks[k] != "" && !(ks[k] in cov)) {
          gain = 1
        }
      }
      if (gain) {
        pick[p] = 1
        picked++
        for (k = 1; k <= m; k++) {
          cov[ks[k]] = 1
        }
      }
    }
    for (i = 1; i <= no && picked < N; i++) {
      if (!(order[i] in pick)) {
        pick[order[i]] = 1
        picked++
      }
    }
    for (i = 1; i <= no; i++) {
      if (order[i] in pick) {
        print order[i]
      }
    }
  }' "$WORK/phi_header.tsv" "$WORK/phi.tsv" "$WORK/lanes.tsv" "$WORK/order.txt" \
  > "$WORK/picked.txt"
echo "  picked $(wc -l < "$WORK/picked.txt" | tr -d ' ') problems:" \
  "$(xargs -n1 basename < "$WORK/picked.txt" | sed 's/\.p$//' | tr '\n' ' ')"

# 3. The lane runs.
awk -F'\t' '
  FILENAME == ARGV[1] {
    want[$1] = 1
    next
  }
  FILENAME == ARGV[2] {
    tphi[$1] = $2
    next
  }
  ($1 in want) && ($6 != "-" || $3 == "wm" || $3 == "bare" || $3 == "bare-raw") {
    print $1 "\t" $2 "\t" $3 "\t" $4 "\t" $5 "\t" $6 "\t" $7 "\t" (($1 in tphi) ? tphi[$1] : 0)
  }' "$WORK/picked.txt" "$WORK/phi_time.tsv" "$WORK/lanes.tsv" > "$WORK/jobs.tsv"
n_jobs=$(wc -l < "$WORK/jobs.tsv" | tr -d ' ')
if [ "$n_jobs" -eq 0 ]; then
  echo "  FAIL: no lane runs"
  finish fail 1
fi
echo "  running $n_jobs lane runs on BIN and OLD_FP, $JOBS at a time, --wall=$WALL"
seq 1 "$n_jobs" | xargs -P "$JOBS" -I{} bash "$0" --job {} "$WORK" "$BIN" "$WALL" "$FLOOR" "$DEADLINE"
cat "$WORK"/res/* 2> /dev/null | sort -t$'\t' -k2,2 -k3,3 > "$WORK/results.tsv"
n_res=$(wc -l < "$WORK/results.tsv" | tr -d ' ')
if [ "$n_res" -ne "$n_jobs" ]; then
  echo "  FAIL: $n_res results for $n_jobs lane runs"
  fail=1
fi
n_skip=$(awk -F'\t' '$1 == "SKIP"' "$WORK/results.tsv" | wc -l | tr -d ' ')
if [ "$n_skip" -gt 0 ]; then
  echo "  FAIL: $n_skip lane runs not started before the deadline"
  fail=1
fi

# The replica's lines, tallied by the lane's gate.
awk -F'\t' '
  {
    st = $1
    if (st == "SKIP") {
      next
    }
    if (st == "FAIL") {
      printf "  FAIL %s %s: predicted %s strict=%s \"%s\"; %s\n", $2, $3, $4, $5, $7, $9
      nfail++
    } else if (st == "INFO") {
      ninfo++
      nagree += ($9 == "agrees")
      if ($9 != "agrees") {
        printf "  info %s %s: predicted %s (non-strict) \"%s\"; %s\n", $2, $3, $4, $7, $9
      }
    }
    if ($5 == 1) {
      nstrict++
      g = ($6 == "-") ? "front end" : $6
      if (!(g in checks)) {
        names[++ng] = g
      }
      checks[g]++
      fails[g] += (st == "FAIL")
    }
    ndead += ($4 == "dead")
  }
  END {
    for (i = 1; i <= ng; i++) {
      printf "  gate %-20s %4d strict line checks, %d failed\n", names[i], checks[names[i]], fails[names[i]]
    }
    printf "atp_problem_features: %d/%d strict line checks pass (%d dead lanes), %d non-strict (%d agree)\n", \
      nstrict - nfail, nstrict, ndead, ninfo, nagree
    exit (nfail > 0)
  }' "$WORK/results.tsv"
if [ $? -ne 0 ]; then
  fail=1
fi

# The gate bits against what each strict lane did.  pos: the front-end
# stage of a gate line, in main's order (0: no line; -1: a line two stages
# can print, or none known).
awk -F'\t' -v BITS="$BITS" '
  function pos(l) {
    if (l == "" || l == "-") return 0
    if (l ~ /^fof slice: /) return 1
    if (l ~ /^cannot make a fresh name family/) return 3
    if (l ~ /^(problem exceeds (clause|symbol|variable) bound|term exceeds side-length bound)/) return 3
    if (l ~ /^no negated conjecture/) return 4
    if (l ~ /^ifeq-guard slice: /) return 5
    if (l ~ /^curried goal exceeds/) return 6
    if (l ~ /^horn-(cd|back) slice: /) return 8
    if (l ~ /^def-unfold slice: /) return 9
    if (l ~ /^tgt-ax slice: /) return 10
    if (l ~ /^axiom selection cannot fit/) return 12
    if (l ~ /^(problem exceeds (equation|symbol|variable) capacity|functor arity exceeds)/) return 13
    if (l ~ /^canonical (clause|term) exceeds/) return 14
    if (l ~ /^tgt-sel slice: /) return 15
    if (l ~ /^equals-goal slice: /) return 17
    if (l ~ /^narrow-queue slice: /) return 18
    if (l ~ /^pr parse error/) return 19
    if (l ~ /^otter slice: /) return 20
    return -1
  }
  BEGIN {
    split("ifeq:ifeq_dispatcher:5 horn-cd:horn_slice:8 horn-back:horn_slice:8 " \
          "unfold:def_unfold:9 unfold-tgt:def_unfold:9 unfold-tgt-awr:def_unfold:9 " \
          "tgt-ax:ax_ground_compound:10 tgt-sel:conj_cone_partial:15 " \
          "equals:equals_goal:17 equals-twee:equals_goal:17 narrowq:goal_existential:18 " \
          "wm:fe_live:FE bare:fe_live:FE otter-lpo:distrib_law:20 " \
          "otter-lpo-cmin:distrib_law:20 otter-ladr:distrib_law:20", pairs, " ")
    for (i in pairs) {
      split(pairs[i], f, ":")
      lbit[f[1]] = f[2]
      lpos[f[1]] = f[3]
      tested[f[2]] = 1
    }
    split("3 4 6 12 13 14 19", fe, " ")
    for (i in fe) {
      fepos[fe[i]] = 1
    }
  }
  FILENAME == ARGV[1] {
    for (i = 1; i <= NF; i++) {
      col[$i] = i + 1
    }
    next
  }
  FILENAME == ARGV[2] {
    for (b in tested) {
      if (!(b in col)) {
        if (!(b in missing)) {
          printf "  FAIL: phi(p) has no column %s\n", b
          missing[b] = 1
        }
        bad = 1
        continue
      }
      bit[$2, b] = $(col[b]) + 0
    }
    next
  }
  $1 != "SKIP" && $5 == 1 && ($3 in lbit) {
    b = lbit[$3]
    if (!(($2, b) in bit)) {
      next
    }
    v = bit[$2, b]
    p = pos($10)
    w = ""
    if (p < 0) {
      next
    }
    if ($3 ~ /^otter/) {
      if (v == 0 && (p == 0 || p == 20)) {
        checks["otter", 0]++
        if (p == 0) {
          printf "  FAIL bit %s %s: distrib_law=0 but the lane printed no otter line\n", $2, $3
          nbad++
        }
      }
      next
    }
    if (lpos[$3] == "FE") {
      if (p == 0) {
        w = 1
      } else if (p in fepos) {
        w = 0
      }
    } else if (p == 0 || p > lpos[$3]) {
      w = 1
    } else if (p == lpos[$3]) {
      w = 0
    }
    if (w == "") {
      next
    }
    checks[b, w]++
    if (v != w) {
      printf "  FAIL bit %s %s: %s=%d, but the lane %s\n", $2, $3, b, v, \
        (w == 1) ? "got past its gate" : "printed its gate line"
      nbad++
    }
  }
  END {
    split("ifeq_dispatcher horn_slice def_unfold ax_ground_compound conj_cone_partial equals_goal goal_existential fe_live", want, " ")
    for (i = 1; i <= 8; i++) {
      b = want[i]
      printf "  bit %-20s %4d checks at 0, %4d at 1\n", b, checks[b, 0] + 0, checks[b, 1] + 0
      if (checks[b, 0] + 0 == 0 || checks[b, 1] + 0 == 0) {
        printf "  FAIL UNCOVERED %s: no strict check with the bit %s\n", b, (checks[b, 0] + 0 == 0) ? "0" : "1"
        nbad++
      }
    }
    printf "  bit %-20s %4d checks at distrib_law=0 (one way)\n", "otter", checks["otter", 0] + 0
    if (checks["otter", 0] + 0 == 0) {
      printf "  FAIL UNCOVERED otter: no strict otter-* check with distrib_law=0\n"
      nbad++
    }
    printf "atp_problem_features: gate bits against the lanes: %d failures\n", nbad + bad
    exit (nbad + bad > 0)
  }' "$WORK/phi_header.tsv" "$WORK/phi.tsv" "$WORK/results.tsv"
if [ $? -ne 0 ]; then
  fail=1
fi
if [ "$fail" -ne 0 ]; then
  finish fail 1
fi
finish pass 0
