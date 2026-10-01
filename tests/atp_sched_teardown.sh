#!/bin/bash
# FindProof --schedule teardown, ENIGMA gate G0.4
# (docs/plans/enigma_portfolio.md, M0): a schedule run stopped from
# outside or by its wall leaves no FindProof process behind, and every
# run ends by its wall + 10 s.
#
#   tests/atp_sched_teardown.sh FINDPROOF TPTP_ROOT [teardown [PROBLEM]]
#   tests/atp_sched_teardown.sh FINDPROOF TPTP_ROOT dryrun LIST WALL
#
# teardown (the default) races every lane (THVM_ATP_SCHED_MAX_PAR = the
# --list-lanes count) on PROBLEM, LAT186-10 by default, three times:
#   SIGTERM  to the parent 5 s into a --wall=60 run: 3 s later no process
#            of the run may be left.
#   SIGALRM  the same with the signal of the parent's wall alarm (due at
#            --wall + 1 s; sent early, while the lanes still run).
#   wall     a --wall=10 run without a signal must end by 20 s, and no
#            process of it may be left 3 s after it ends.
# At 5 s each run must still be going, with a lane alive at index 32 or
# above: the lanes that a 32-slot teardown registry (the G0.4 defect)
# leaves out.  Otherwise the case is inconclusive and needs a problem
# that keeps more lanes busy.
#
# dryrun runs every problem of LIST once at --wall=WALL as the ENIGMA
# protocol does (env -i with TPTP set, stdin from /dev/null, timeout -k 5
# WALL+15, --schedule --szs --proof, THVM_ATP_SCHED_MAX_PAR from the
# environment or 16): each run must end by WALL + 10 s and leave no
# process 3 s later.  It prints run times, so LIST should hold TRAIN
# problems: J13 outcomes stay sealed (section 1.4 of the plan).
#
# PROBLEM and LIST entries are files, absolute or under TPTP_ROOT, or
# names found as UEQ/NAME.p (the J13 root) or Problems/DOM/NAME.p (a TPTP
# tree); a LIST line with tabs (a problems.tsv or shard row) names its
# file in the second column.  Unless THVM_ATP_RSS_ABORT_MB is set, each
# slice is capped at min(4500, 0.9 * MemTotal / width) MB, so the caps of
# the racing slices fit the machine.  Each run goes through its own
# symlink to FINDPROOF in this script's temporary directory: every process
# of a run carries that path on its command line (a slice re-execs its
# parent's argv[0]), and only those processes are counted or killed, never
# another FindProof on the machine.  Linux only (/proc, procps ps): run it
# on a pod.
# Exit: 0 pass, 1 a check failed, 2 usage or setup error, 3 inconclusive.
set -u
export LC_ALL=C

usage() {
  echo "usage: $0 FINDPROOF TPTP_ROOT [teardown [PROBLEM]]" >&2
  echo "       $0 FINDPROOF TPTP_ROOT dryrun LIST WALL" >&2
  exit 2
}

if [ $# -lt 2 ]; then
  usage
fi
if [ ! -x "$1" ] || [ ! -d "$2" ]; then
  echo "$0: FINDPROOF must be an executable and TPTP_ROOT a directory" >&2
  exit 2
fi
if [ ! -r /proc/self/environ ]; then
  echo "$0: needs Linux /proc; run it on a pod" >&2
  exit 2
fi
FP=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
ROOT=$(cd "$2" && pwd)
MODE=${3:-teardown}
if ! WORK=$(mktemp -d "${TMPDIR:-/tmp}/sched_teardown.XXXXXX") || [ ! -d "$WORK" ]; then
  echo "$0: cannot create a temporary directory" >&2
  exit 2
fi
LINK=""
RUN=0
PARENT=""
PARENTS=""
CAP=""
ENVV=()
FAIL=0
INCONC=0

now() {
  if [ -n "${EPOCHREALTIME:-}" ]; then
    printf '%s\n' "$EPOCHREALTIME"
  else
    date +%s
  fi
}

# Seconds since $1, one decimal.
since() {
  awk -v a="$1" -v b="$(now)" 'BEGIN { printf "%.1f\n", b - a }'
}

# True when $1 >= $2 (decimals).
at_least() {
  awk -v a="$1" -v b="$2" 'BEGIN { exit !(a >= b) }'
}

# Each run gets its own symlink to FINDPROOF, so that a process outliving
# one run is never taken for part of the next.
new_link() {
  RUN=$((RUN + 1))
  mkdir "$WORK/run$RUN"
  LINK=$WORK/run$RUN/FindProof
  ln -s "$FP" "$LINK"
}

# Every live process whose command line holds the tag $1 (default: the
# current run's link), "pid stat args" per line.  A tag outside this
# script's own directory matches nothing, so no other FindProof run on the
# machine is ever listed or killed.  The tag travels in awk's environment,
# so awk's own command line cannot match it; zombies are already dead.
run_procs() {
  local tag=${1:-$LINK}
  case $tag in
    "$WORK"/run*)
      ;;
    *)
      return 0
      ;;
  esac
  ps -ww -eo pid=,stat=,args= 2> /dev/null |
    TAG=$tag awk 'index($0, ENVIRON["TAG"]) > 0 && $2 !~ /^Z/'
}

kill_run() {
  local pids
  pids=$(run_procs "${1:-$LINK}" | awk '{ print $1 }')
  if [ -n "$pids" ]; then
    kill -KILL $pids 2> /dev/null
  fi
}

cleanup() {
  local p
  kill_run "$WORK/run"
  for p in $PARENTS; do
    rm -f /tmp/thvm_sched_"$p"_*.out
  done
  rm -rf "$WORK"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

# The lane a process runs (FINDPROOF_SLICE in its environment; empty for
# the parent, its forked slice children and their shells).
lane_of() {
  { tr '\0' '\n' < "/proc/$1/environ"; } 2> /dev/null |
    sed -n 's/^FINDPROOF_SLICE=//p' | head -n 1
}

# The lanes alive in the run, "index name" per line.
live_lanes() {
  local pid
  for pid in $(run_procs | awk '{ print $1 }'); do
    lane_of "$pid"
  done | awk 'NR == FNR { ix[$0] = FNR - 1; next }
              ($0 in ix) { print ix[$0], $0 }' "$WORK/lanes" - | sort -n -u
}

# True while process $1 runs; false once it is reaped or a zombie.
alive() {
  local st
  st=$(ps -o stat= -p "$1" 2> /dev/null)
  st=${st//[[:space:]]/}
  [ -n "$st" ] && [ "${st#Z}" = "$st" ]
}

# Poll for up to $1 s until no process of the run is left; on failure,
# print what is left.
wait_gone() {
  local t0 left
  t0=$(now)
  while :; do
    left=$(run_procs)
    if [ -z "$left" ]; then
      return 0
    fi
    if at_least "$(since "$t0")" "$1"; then
      printf '%s\n' "$left"
      return 1
    fi
    sleep 0.2
  done
}

report_left() {
  local pid st args lane
  printf '%s\n' "$1" | while read -r pid st args; do
    lane=$(lane_of "$pid")
    printf '    left: pid %s %s lane %s: %.160s\n' \
      "$pid" "$st" "${lane:--}" "$args"
  done
}

count_lines() {
  printf '%s\n' "$1" | awk 'NF > 0' | wc -l | tr -d ' '
}

# The run ended before it could be stopped: nothing of it may be left,
# and the case says nothing about the teardown.
ended_early() {
  local left
  wait "$PARENT"
  if left=$(wait_gone 3); then
    echo "  INCONCLUSIVE: $1: the run ended within 5 s"
    INCONC=1
  else
    echo "  FAIL: $1: the run ended within 5 s and left" \
         "$(count_lines "$left") processes behind"
    report_left "$left"
    kill_run
    FAIL=1
  fi
}

# A problem argument as a file: as given, under the root, or by name in
# the J13 layout or a TPTP tree.
resolve() {
  local name=${1%.p} c
  for c in "$1" "$ROOT/$1" "$ROOT/UEQ/$name.p" "$ROOT/Problems/${name:0:3}/$name.p"; do
    if [ -f "$c" ]; then
      printf '%s/%s\n' "$(cd "$(dirname "$c")" && pwd)" "$(basename "$c")"
      return 0
    fi
  done
  return 1
}

# Unless the caller set it, the slice RSS cap that fits $1 racing slices in
# 90% of the machine, at most the binary's own 4500 MB.
slice_cap() {
  if [ -n "${THVM_ATP_RSS_ABORT_MB:-}" ]; then
    printf '%s\n' "$THVM_ATP_RSS_ABORT_MB"
  else
    awk -v w="$1" '/^MemTotal:/ { c = int($2 / 1024 * 0.9 / w)
                                  if (c > 4500) { c = 4500 }
                                  print c }' /proc/meminfo
  fi
}

# The protocol's environment for a run $1 slices wide: env -i keeps only
# PATH, HOME, TPTP, the width and the slice cap.
set_env() {
  CAP=$(slice_cap "$1")
  ENVV=(PATH="$PATH" HOME="${HOME:-/}" TPTP="$ROOT" THVM_ATP_SCHED_MAX_PAR="$1")
  if [ -n "$CAP" ]; then
    ENVV+=(THVM_ATP_RSS_ABORT_MB="$CAP")
  fi
  echo "binary $FP (sha256 $(sha256sum "$FP" | cut -c1-16)), $NL lanes," \
       "width $1, slice cap ${CAP:-default} MB, $(nproc) cpus," \
       "$(awk '/^MemTotal:/ { printf "%d", $2 / 1024 }' /proc/meminfo) MB"
}

# Launch one schedule run in the background: $1 wall, $2 problem.
start_run() {
  new_link
  env -i "${ENVV[@]}" "$LINK" --schedule --szs --proof --wall="$1" "$2" \
    < /dev/null > "$WORK/last.out" 2> "$WORK/last.err" &
  PARENT=$!
  PARENTS="$PARENTS $PARENT"
  T0=$(now)
}

# The lanes at index HIGH or above among "index name" lines.
high_lanes() {
  printf '%s\n' "$1" | awk -v h="$HIGH" 'NF == 2 && $1 >= h { printf " %s", $2 }'
}

# One --wall=60 run stopped by signal $1 (TERM or ALRM) 5 s after launch.
case_signal() {
  local sig=$1 lanes high left rc nlive
  start_run 60 "$PROB"
  sleep 5
  if ! alive "$PARENT"; then
    ended_early "SIG$sig"
    return
  fi
  lanes=$(live_lanes)
  nlive=$(count_lines "$lanes")
  high=$(high_lanes "$lanes")
  kill -s "$sig" "$PARENT"
  if left=$(wait_gone 3); then
    wait "$PARENT"
    rc=$?
    if [ -z "$high" ]; then
      echo "  INCONCLUSIVE: SIG$sig: no lane at index >= $HIGH was alive" \
           "($nlive lanes were)"
      INCONC=1
    else
      echo "  ok: SIG$sig at 5 s: nothing left 3 s later; $nlive lanes" \
           "were alive, at index >= $HIGH:$high; parent exit $rc"
    fi
  else
    echo "  FAIL: SIG$sig at 5 s: $(count_lines "$left") processes of the" \
         "run left 3 s later ($nlive lanes were alive, at index >=" \
         "$HIGH:$high)"
    report_left "$left"
    kill_run
    wait "$PARENT"
    FAIL=1
  fi
}

# One --wall=10 run left to its wall.
case_wall() {
  local w=10 lanes high left rc el nlive
  start_run "$w" "$PROB"
  sleep 5
  if ! alive "$PARENT"; then
    ended_early "wall $w s"
    return
  fi
  lanes=$(live_lanes)
  nlive=$(count_lines "$lanes")
  high=$(high_lanes "$lanes")
  while alive "$PARENT" && ! at_least "$(since "$T0")" $((w + 10)); do
    sleep 0.2
  done
  el=$(since "$T0")
  if alive "$PARENT"; then
    echo "  FAIL: wall $w s: the run still went on at $el s"
    kill_run
    wait "$PARENT"
    FAIL=1
    return
  fi
  wait "$PARENT"
  rc=$?
  if ! left=$(wait_gone 3); then
    echo "  FAIL: wall $w s: the run ended at $el s and left" \
         "$(count_lines "$left") processes behind"
    report_left "$left"
    kill_run
    FAIL=1
  elif [ -z "$high" ]; then
    echo "  INCONCLUSIVE: wall $w s: no lane at index >= $HIGH was alive at 5 s"
    INCONC=1
  else
    echo "  ok: wall $w s: the run ended at $el s and left nothing; $nlive" \
         "lanes were alive at 5 s, at index >= $HIGH:$high; parent exit $rc"
  fi
}

dryrun() {
  local list=$1 w=$2 line entry path seen="" p name rc el left longest=0
  local timeout_bin
  if ! [[ $w =~ ^[1-9][0-9]*$ ]] || [ ! -r "$list" ]; then
    usage
  fi
  if ! timeout_bin=$(command -v timeout); then
    echo "$0: dryrun needs GNU timeout" >&2
    exit 2
  fi
  local probs=()
  while IFS= read -r line || [ -n "$line" ]; do
    line=${line#"${line%%[![:space:]]*}"}
    case $line in
      '' | '#'*)
        continue
        ;;
      *$'\t'*)
        entry=$(printf '%s\n' "$line" | cut -f2)
        ;;
      *)
        entry=${line%%[[:space:]]*}
        ;;
    esac
    if ! path=$(resolve "$entry"); then
      echo "$0: no problem file for '$entry' under $ROOT" >&2
      exit 2
    fi
    case $seen in
      *"|$path|"*)
        continue
        ;;
    esac
    seen="$seen|$path|"
    probs+=("$path")
  done < "$list"
  if [ ${#probs[@]} -eq 0 ]; then
    echo "$0: $list names no problem" >&2
    exit 2
  fi
  set_env "${THVM_ATP_SCHED_MAX_PAR:-16}"
  echo "dryrun: ${#probs[@]} problems at --wall=$w, each must end by $((w + 10)) s"
  for p in "${probs[@]}"; do
    name=$(basename "$p" .p)
    new_link
    T0=$(now)
    env -i "${ENVV[@]}" "$timeout_bin" -k 5 $((w + 15)) \
      "$LINK" --schedule --szs --proof --wall="$w" "$p" \
      < /dev/null > "$WORK/last.out" 2> "$WORK/last.err"
    rc=$?
    el=$(since "$T0")
    if at_least "$el" "$longest"; then
      longest=$el
    fi
    if [ "$rc" -eq 124 ] || [ "$rc" -eq 137 ]; then
      echo "  FAIL: $name: the outer timeout stopped it (exit $rc) at $el s"
      FAIL=1
    elif ! at_least $((w + 10)) "$el"; then
      echo "  FAIL: $name: ended at $el s (exit $rc), after the wall + 10 s"
      FAIL=1
    else
      echo "  ok: $name: ended at $el s"
    fi
    if ! left=$(wait_gone 3); then
      echo "  FAIL: $name: $(count_lines "$left") processes of the run" \
           "left 3 s after it ended"
      report_left "$left"
      kill_run
      FAIL=1
    fi
  done
  if [ "$FAIL" = 0 ]; then
    echo "SCHED DRYRUN OK: ${#probs[@]} runs at --wall=$w, the longest ended at $longest s"
  else
    echo "SCHED DRYRUN FAILED"
    exit 1
  fi
}

if ! "$FP" --list-lanes > "$WORK/lanes" 2> /dev/null || [ ! -s "$WORK/lanes" ]; then
  echo "$0: $FP --list-lanes failed" >&2
  exit 2
fi
NL=$(wc -l < "$WORK/lanes" | tr -d ' ')
HIGH=32
if [ "$NL" -le "$HIGH" ]; then
  HIGH=0
fi

case $MODE in
  teardown)
    if ! PROB=$(resolve "${4:-LAT186-10}"); then
      echo "$0: no problem file for '${4:-LAT186-10}' under $ROOT" >&2
      exit 2
    fi
    set_env "$NL"
    echo "teardown: $PROB"
    case_signal TERM
    case_signal ALRM
    case_wall
    if [ "$FAIL" != 0 ]; then
      echo "SCHED TEARDOWN FAILED"
      exit 1
    elif [ "$INCONC" != 0 ]; then
      echo "SCHED TEARDOWN INCONCLUSIVE"
      exit 3
    fi
    echo "SCHED TEARDOWN OK"
    ;;
  dryrun)
    if [ $# -lt 5 ]; then
      usage
    fi
    dryrun "$4" "$5"
    ;;
  *)
    usage
    ;;
esac
