#!/bin/bash
# FindProof --problem-features: the static gate replica against the live
# lanes (docs/plans/enigma_portfolio.md, section 3.1).
#
# 1. phi(p) and the replica (--problem-features --lanes) for every
#    candidate problem; each must succeed.
# 2. N problems (default 20) are picked so that every gate bit and every
#    lane verdict occurs both ways where the set allows.
# 3. Every lane that arms an exit-fast gate, plus wm and bare, runs alone
#    on each picked problem (--lane=NAME, no --szs, so a Horn release that
#    reaches the goal does not end the run before the gates).  Where the
#    replica is strict, the lane's stderr must carry exactly the predicted
#    give-up line (none for a live lane), and a lane it calls dead must
#    end within FAST seconds.  Non-strict predictions (a Horn release can
#    change the gate's input) are reported, not judged.
# OLD_FP=<FindProof> also runs every lane on that binary and requires the
# same gate lines (the refactored gates decide as before).
#
# Usage: tests/atp_problem_features.sh [-n N] [-w WALL] [-f FAST] [-j JOBS]
#                                      [-o DIR] BIN PROBLEMS
#   BIN       bin/FindProof
#   PROBLEMS  a directory searched for *.p, or a file of problem paths
#   -w        a lane run's --wall (default 15); -f the dead-lane deadline
#             (default 8: the CSR knowledge bases take seconds to parse);
#             -j parallel runs (default nproc/2); -o keeps the
#             work files in DIR (lane stderr is never kept)
# TPTP must name the root the problems' include() directives resolve
# against (for the J13 set: the directory holding UEQ/ and Axioms/).
#
# J13 seal (plan section 1.4): only gate lines are read, and the run time
# of a lane the replica calls dead; no SZS status, winner, solve time or
# live-lane run time is printed or kept.
set -u

# The exit-fast and front-end give-up lines the replica predicts, and the
# bench's .pr reader ending the run.  The backward condensed-detachment
# lane's "no proof within budget" is a search result, not a gate.
STATIC_RE='FindProof: (fof slice: |ifeq-guard slice: |horn-(cd|back) slice: (no encoded|existential|oversized)|def-unfold slice: |tgt-ax slice: |tgt-sel slice: |equals-goal slice: |narrow-queue slice: |otter slice: |no negated conjecture|problem exceeds|functor arity exceeds|axiom selection cannot fit|canonical (clause|term) exceeds|curried goal exceeds|term exceeds side-length bound)|pr parse error \('

now() {
  date +%s.%N 2> /dev/null || date +%s
}

# The gate lines of one run's stderr, without the run-specific .pr path.
static_lines() {
  grep -E "$STATIC_RE" "$1" | head -n 3 |
    sed -e 's/^.*FindProof: //' -e 's/^pr parse error ([^)]*): /pr parse error: /' |
    tr '\n' '|'
}

run_lane() {
  env -i PATH="$PATH" HOME="${HOME:-/tmp}" TPTP="${TPTP:-}" THVM_ATP_RSS_ABORT_MB=2000 \
    timeout -k 5 $(($3 + 15)) "$1" --lane="$2" --wall="$3" "$4" \
    > /dev/null 2> "$5" < /dev/null
}

# --feat ID WORK BIN: phi(p) and the replica rows of candidate ID.
if [ "${1:-}" = "--feat" ]; then
  id=$2
  work=$3
  bin=$4
  path=$(sed -n "${id}p" "$work/cands.txt")
  env -i PATH="$PATH" HOME="${HOME:-/tmp}" TPTP="${TPTP:-}" \
    timeout 300 "$bin" --problem-features "$path" > "$work/feat/$id.phi.tmp" 2> /dev/null
  rc1=$?
  env -i PATH="$PATH" HOME="${HOME:-/tmp}" TPTP="${TPTP:-}" \
    timeout 300 "$bin" --problem-features --lanes "$path" > "$work/feat/$id.lanes.tmp" 2> /dev/null
  rc2=$?
  if [ $rc1 -ne 0 ] || [ $rc2 -ne 0 ] || [ ! -s "$work/feat/$id.phi.tmp" ]; then
    echo "$path: --problem-features exit $rc1, --lanes exit $rc2" > "$work/feat/$id.err"
  else
    awk -v p="$path" '{ print p "\t" $0 }' "$work/feat/$id.phi.tmp" > "$work/feat/$id.phi"
    awk -v p="$path" '{ print p "\t" $0 }' "$work/feat/$id.lanes.tmp" > "$work/feat/$id.lanes"
  fi
  rm -f "$work/feat/$id.phi.tmp" "$work/feat/$id.lanes.tmp"
  exit 0
fi

# --job ID WORK BIN WALL FAST: one lane run, judged against the replica.
if [ "${1:-}" = "--job" ]; then
  id=$2
  work=$3
  bin=$4
  wall=$5
  fast=$6
  IFS=$'\t' read -r path name lane verdict strict gates reason \
    < <(sed -n "${id}p" "$work/jobs.tsv")
  err="$work/err.$id"
  t0=$(now)
  run_lane "$bin" "$lane" "$wall" "$path" "$err"
  t1=$(now)
  el=$(awk -v a="$t0" -v b="$t1" 'BEGIN { printf "%.1f", b - a }')
  seen=$(static_lines "$err")
  status=PASS
  detail=""
  if [ "$reason" != "-" ]; then
    grep -qF -- "$reason" "$err"
    hit=$?
  else
    hit=1
    if [ -z "$seen" ]; then
      hit=0
    fi
  fi
  if [ "$strict" = 1 ]; then
    if [ $hit -ne 0 ]; then
      status=FAIL
      detail="gate lines: ${seen:-none}"
    fi
    if [ "$verdict" = dead ] && ! awk -v e="$el" -v f="$fast" 'BEGIN { exit !(e <= f) }'; then
      status=FAIL
      detail="${detail:+$detail; }dead lane ran ${el}s (limit ${fast}s)"
    fi
  else
    status=INFO
    if [ $hit -eq 0 ]; then
      detail="agrees"
    else
      detail="differs, gate lines: ${seen:-none}"
    fi
  fi
  if [ -n "${OLD_FP:-}" ]; then
    run_lane "$OLD_FP" "$lane" "$wall" "$path" "$err.old"
    old=$(static_lines "$err.old")
    if [ "$old" != "$seen" ]; then
      status=FAIL
      detail="${detail:+$detail; }OLD_FP gate lines: ${old:-none}, new: ${seen:-none}"
    fi
    rm -f "$err.old"
  fi
  rm -f "$err"
  # A live lane's run time can be its solve time (J13 seal).
  if [ "$verdict" != dead ]; then
    el=-
  fi
  printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' "$status" "$name" "$lane" \
    "$verdict" "$strict" "$gates" "$reason" "$el" "$detail" > "$work/res/$id"
  exit 0
fi

usage() {
  sed -n '2,/^set -u/p' "$0" | sed '$d' | sed 's/^# \{0,1\}//'
}

N=20
WALL=15
FAST=8
JOBS=$(( $(nproc 2> /dev/null || echo 2) / 2 ))
KEEP=""
while getopts "n:w:f:j:o:h" opt; do
  case $opt in
    n) N=$OPTARG ;;
    w) WALL=$OPTARG ;;
    f) FAST=$OPTARG ;;
    j) JOBS=$OPTARG ;;
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
if [ -n "$KEEP" ]; then
  WORK=$KEEP
  mkdir -p "$WORK"
else
  WORK=$(mktemp -d)
  trap 'rm -rf "$WORK"' EXIT
fi
rm -rf "$WORK/feat" "$WORK/res"
mkdir -p "$WORK/feat" "$WORK/res"

if [ -d "$2" ]; then
  find "$(cd "$2" && pwd)" -name '*.p' | sort > "$WORK/cands.txt"
else
  grep -v '^[[:space:]]*$' "$2" > "$WORK/cands.txt"
fi
n_cand=$(wc -l < "$WORK/cands.txt" | tr -d ' ')
if [ "$n_cand" -eq 0 ]; then
  echo "atp_problem_features: no problems in $2"
  exit 2
fi

# 1. Features and replica rows for every candidate.
"$BIN" --problem-features --header > "$WORK/phi_header.tsv"
seq 1 "$n_cand" | xargs -P "$JOBS" -I{} bash "$0" --feat {} "$WORK" "$BIN"
cat "$WORK"/feat/*.phi > "$WORK/phi.tsv" 2> /dev/null
cat "$WORK"/feat/*.lanes > "$WORK/lanes.tsv" 2> /dev/null
n_err=$(cat "$WORK"/feat/*.err 2> /dev/null | wc -l | tr -d ' ')
fail=0
if [ "$n_err" -gt 0 ]; then
  echo "  FAIL: --problem-features failed on $n_err of $n_cand problems:"
  cat "$WORK"/feat/*.err | head -n 20 | sed 's/^/    /'
  fail=1
else
  echo "  ok: phi(p) and the replica for all $n_cand problems"
fi

# 2. Pick N problems covering every gate bit and lane verdict both ways,
# first in cksum order of the name, then filling in that order.
while IFS= read -r p; do
  printf '%s\t%s\n' "$(printf '%s' "$(basename "$p")" | cksum | cut -d' ' -f1)" "$p"
done < <(cut -f1 "$WORK/phi.tsv") | sort -n | cut -f2 > "$WORK/order.txt"
awk -F'\t' -v N="$N" '
  FILENAME == ARGV[1] {
    for (i = 1; i <= NF; i++) {
      col[$i] = i + 1
    }
    next
  }
  FILENAME == ARGV[2] {
    split("goal_existential equals_goal ifeq_dispatcher horn_slice def_unfold " \
          "ax_ground_compound conj_cone_partial distrib_law fe_live sine", bits, " ")
    for (b in bits) {
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
awk -F'\t' '
  FILENAME == ARGV[1] {
    for (i = 1; i <= NF; i++) {
      col[$i] = i + 1
    }
    next
  }
  FILENAME == ARGV[2] {
    want[$1] = 1
    next
  }
  ($1 in want) {
    n = split("goal_existential equals_goal ifeq_dispatcher horn_slice def_unfold " \
              "ax_ground_compound conj_cone_partial distrib_law fe_live sine", bits, " ")
    for (b = 1; b <= n; b++) {
      ones[b] += ($(col[bits[b]]) + 0 != 0)
    }
    tot++
  }
  END {
    line = "  gate bits set (of " tot "):"
    for (b = 1; b <= n; b++) {
      line = line " " bits[b] "=" ones[b] + 0
    }
    print line
  }' "$WORK/phi_header.tsv" "$WORK/picked.txt" "$WORK/phi.tsv"

# 3. The lane runs.
awk -F'\t' '
  FILENAME == ARGV[1] {
    want[$1] = 1
    next
  }
  ($1 in want) && ($6 != "-" || $3 == "wm" || $3 == "bare") {
    print $1 "\t" $2 "\t" $3 "\t" $4 "\t" $5 "\t" $6 "\t" $7
  }' "$WORK/picked.txt" "$WORK/lanes.tsv" > "$WORK/jobs.tsv"
n_jobs=$(wc -l < "$WORK/jobs.tsv" | tr -d ' ')
echo "  running $n_jobs lane runs, $JOBS at a time, --wall=$WALL"
seq 1 "$n_jobs" | xargs -P "$JOBS" -I{} bash "$0" --job {} "$WORK" "$BIN" "$WALL" "$FAST"
cat "$WORK"/res/* 2> /dev/null | sort -t$'\t' -k2,2 -k3,3 > "$WORK/results.tsv"
n_res=$(wc -l < "$WORK/results.tsv" | tr -d ' ')
if [ "$n_res" -ne "$n_jobs" ]; then
  echo "  FAIL: $n_res results for $n_jobs lane runs"
  fail=1
fi

awk -F'\t' '
  function bit_of_reason(r) {
    if (r ~ /^fof slice/) return "fof_input"
    if (r ~ /^ifeq-guard/) return "ifeq_dispatcher"
    if (r ~ /^horn-(cd|back) slice/) return "horn_slice"
    if (r ~ /^def-unfold/) return "def_unfold"
    if (r ~ /^tgt-ax/) return "ax_ground_compound"
    if (r ~ /^tgt-sel/) return "conj_cone_partial"
    if (r ~ /^equals-goal/) return "equals_goal"
    if (r ~ /^narrow-queue/) return "goal_existential"
    if (r ~ /^otter/) return "distrib_law"
    return "fe_live"
  }
  function bit_of_gate(g) {
    if (g == "fof") return "fof_input"
    if (g == "ifeq") return "ifeq_dispatcher"
    if (g == "horn") return "horn_slice"
    if (g == "def") return "def_unfold"
    if (g == "tgt-ax") return "ax_ground_compound"
    if (g == "tgt-sel") return "conj_cone_partial"
    if (g == "equals") return "equals_goal"
    if (g == "narrowq") return "goal_existential"
    if (g == "otter") return "distrib_law"
    return g
  }
  function tally(b, st) {
    if (!(b in checks)) {
      names[++nb] = b
    }
    checks[b]++
    fails[b] += (st == "FAIL")
  }
  {
    st = $1
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
      if ($7 != "-") {
        tally(bit_of_reason($7), st)
      } else if ($6 == "-") {
        tally("fe_live", st)
      } else {
        m = split($6, gs, ",")
        for (k = 1; k <= m; k++) {
          tally(bit_of_gate(gs[k]), st)
        }
      }
    }
    if ($4 == "dead") {
      ndead++
    }
  }
  END {
    for (i = 1; i <= nb; i++) {
      printf "  gate %-20s %4d strict checks, %d failed\n", names[i], checks[names[i]], fails[names[i]]
    }
    printf "atp_problem_features: %d/%d strict lane checks pass (%d dead lanes), %d non-strict (%d agree)\n", \
      nstrict - nfail, nstrict, ndead, ninfo, nagree
    exit (nfail > 0)
  }' "$WORK/results.tsv"
rc=$?
if [ $rc -ne 0 ]; then
  fail=1
fi
exit $fail
