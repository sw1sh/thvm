#!/bin/bash
# FindProof --schedule teardown, ENIGMA gate G0.4
# (docs/ATP/enigma_portfolio.md, M0, as amendment A1 in section 5.4 fixes
# it): a schedule run stopped by a signal leaves no FindProof process
# behind, and no run ends later than its wall + 10 s.
#
#   tests/atp/atp_sched_teardown.sh FINDPROOF TPTP_ROOT [teardown]
#   tests/atp/atp_sched_teardown.sh FINDPROOF TPTP_ROOT dryrun LIST WALL
#
# teardown (the default) runs LAT071-1 (Problems/LAT/LAT071-1.p under
# TPTP_ROOT), a TRAIN problem of the pool whose TPTP status is Open, so no
# lane proves it, with every lane racing (THVM_ATP_SCHED_MAX_PAR = the
# --list-lanes count).  Each signal case starts a --wall=60 run and
# signals it 5 s in:
#   SIGTERM, SIGINT, SIGALRM  to the parent of a --schedule run (SIGALRM is
#               the signal of the parent's own wall alarm, sent early);
#   lane        SIGTERM to the parent of a --lane run of the last lane;
#   slice       SIGTERM to the forked slice process of the last lane alone,
#               in a --schedule run: 3 s later that slice's process group
#               (the slice, its shell, its engine) must be gone while the
#               parent and the other lanes go on; SIGTERM to the parent
#               then ends the run;
#   lane slice  the same in a --lane run of the last lane, whose parent
#               must then end by itself;
#   pkill       the lane matrix runner's stop for a stuck worker, pkill
#               -TERM -f "^BIN --lane=", on a --lane run of the last lane: it
#               matches the parent and its slice, which share a command line.
# A signalled parent must end through its teardown handler (exit 128 + the
# signal; pkill excepted, as its slice may end first), and 3 s after the
# signal no process of the run, and no slice output file of its parent in
# /tmp, may be left.  Each case needs the last lane (index NL - 1) alive at
# 5 s, so that a teardown registry shorter than the lane table leaves that
# lane running.  A case whose run ends before its signal, or whose last lane
# is not alive, is inconclusive.
#   wall        a --wall=10 --schedule run must end by 20 s and leave
#               nothing.  How it ended is reported; this case can only fail.
# The signal cases and the wall case get separate verdicts.
#
# dryrun runs the 20 problems of LIST once each at --wall=WALL under the
# protocol of plan section 1.3: env -i with only TPTP,
# THVM_ATP_SCHED_MAX_PAR=16 and, when MemTotal is below 72 GiB,
# THVM_ATP_RSS_ABORT_MB=3500; stdin from /dev/null; timeout -k 5 WALL+15;
# --schedule --szs --proof.  Each run must end by WALL + 10 s and leave no
# process 3 s later.  A run that ends before its wall leaves that bound
# untested, so at least half of the runs must reach the wall, or the dry
# run is inconclusive.  A run reaches the wall when its parent stops it
# there (half a second before the wall), when its wall alarm fires, or when
# it ends at WALL - 4 s or later: each lane stops itself on its CPU clock at
# the wall less 3 s, often before the parent's stop.  LIST holds exactly 20
# problems.tsv rows (name, path under TPTP_ROOT, split, ...), every one of
# split train; '#' lines are comments.  The gate's list is
# tools/atp_enigma/g04_dryrun.tsv.
#
# J13 stays sealed (plan section 1.4): a row of split j13 and a file in the
# J13 layout (UEQ/NAME.p) are refused.  Both modes print the sha256 of this
# script and of FINDPROOF, and dryrun that of LIST.  A THVM_ATP_RSS_ABORT_MB
# set by the caller is passed on; otherwise the --schedule runs of teardown
# mode cap each slice at min(4500, 0.9 * MemTotal / width) MB, so the caps
# of the racing slices fit the machine, and its --lane runs keep the
# binary's own cap.  Each run goes through its own symlink to FINDPROOF in
# this script's temporary directory: every process of a run carries that
# path on its command line (a slice is a fork of its parent and execs the
# parent's argv[0]), and only those processes are counted or killed, never
# another FindProof on the machine.  Linux only (/proc, procps ps and
# pkill): run it on a pod.
# Exit: 0 pass, 1 a check failed, 2 usage or setup error, 3 inconclusive
# (which does not pass G0.4).
set -u
export LC_ALL=C

DRY_N=20
PROBLEM=Problems/LAT/LAT071-1.p

usage() {
  echo "usage: $0 FINDPROOF TPTP_ROOT [teardown]" >&2
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
# Every "nothing left" check reads ps, so ps must list this script itself.
if ! ps -ww -eo pid=,stat=,args= 2> /dev/null |
     SELF=$0 awk -v p="$$" '$1 == p && index($0, ENVIRON["SELF"]) > 0 { f = 1 }
                             END { exit !f }'; then
  echo "$0: needs procps ps: ps -ww -eo pid=,stat=,args= does not list" \
       "this script" >&2
  exit 2
fi
if ! command -v pkill > /dev/null; then
  echo "$0: needs procps pkill" >&2
  exit 2
fi
SCRIPT=$(cd "$(dirname "$0")" && pwd)/$(basename "$0")
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
PFILE=""
WIDTH=""
CAP=""
ENVV=()
RC=0
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

# True while process $1 runs; false once it is reaped or a zombie.
alive() {
  local st
  st=$(ps -o stat= -p "$1" 2> /dev/null)
  st=${st//[[:space:]]/}
  [ -n "$st" ] && [ "${st#Z}" = "$st" ]
}

# The slice output files of the run's parent left in /tmp (the binary names
# them /tmp/thvm_sched_<parent pid>_<slice>.out and removes them when the
# run ends, by a signal too).
left_outs() {
  ls -d /tmp/thvm_sched_"$PARENT"_*.out 2> /dev/null
}

# Removes the files left_outs lists once a check has reported them.  Kept
# while a live process holds the parent's pid: after a reuse they would be
# that process's files.
drop_outs() {
  if [ -n "$PARENT" ] && ! alive "$PARENT"; then
    rm -f /tmp/thvm_sched_"$PARENT"_*.out
  fi
}

cleanup() {
  kill_run "$WORK/run"
  if [ -n "$PARENT" ]; then
    wait "$PARENT" 2> /dev/null
    drop_outs
  fi
  rm -rf "$WORK"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

# The lane a process runs (FINDPROOF_SLICE in its environment; empty for
# the parent, its forked slice processes and their shells).
lane_of() {
  { tr '\0' '\n' < "/proc/$1/environ"; } 2> /dev/null |
    sed -n 's/^FINDPROOF_SLICE=//p' | head -n 1
}

# The lanes alive in the run, "index name" per line.  The environments of
# its processes are read in one pass, so the list is close to a snapshot.
live_lanes() {
  local pid files=()
  for pid in $(run_procs | awk '{ print $1 }'); do
    files+=("/proc/$pid/environ")
  done
  if [ ${#files[@]} -eq 0 ]; then
    return 0
  fi
  cat "${files[@]}" 2> /dev/null | tr '\0' '\n' |
    sed -n 's/^FINDPROOF_SLICE=//p' |
    awk 'NR == FNR { ix[$0] = FNR - 1; next }
         ($0 in ix) { print ix[$0], $0 }' "$WORK/lanes" - | sort -n -u
}

# True when the "index name" lines $1 hold the last lane.
has_last() {
  printf '%s\n' "$1" | awk -v i="$LASTIX" '$1 == i { f = 1 } END { exit !f }'
}

# How many lanes other than the last are in both "index name" lists $1 and
# $2.
others_alive() {
  printf '%s\n' "$2" |
    BEFORE=$1 awk -v l="$LASTIX" '
      BEGIN {
        n = split(ENVIRON["BEFORE"], b, "\n")
        for (i = 1; i <= n; i++) {
          split(b[i], f, " ")
          if (f[1] != "" && f[1] != l) {
            want[f[1]] = 1
          }
        }
      }
      NF == 2 && ($1 in want) { c++ }
      END { print c + 0 }'
}

# The pid of the forked slice process that runs lane $1.  A slice leads its
# own process group, which its shell and its engine share, and only the
# engine carries FINDPROOF_SLICE; the slice is a child of the run's parent.
# False when no engine of the run runs that lane.
slice_of() {
  local pid g pp
  for pid in $(run_procs | awk '{ print $1 }'); do
    if [ "$(lane_of "$pid")" = "$1" ]; then
      g=$(ps -o pgid= -p "$pid" 2> /dev/null | tr -d ' ')
      pp=$(ps -o ppid= -p "${g:-0}" 2> /dev/null | tr -d ' ')
      if [ -n "$g" ] && [ "$pp" = "$PARENT" ]; then
        printf '%s\n' "$g"
        return 0
      fi
    fi
  done
  return 1
}

# The live processes of process group $1, "pid stat args" per line.
group_procs() {
  ps -ww -eo pid=,pgid=,stat=,args= 2> /dev/null |
    awk -v g="$1" '$2 == g && $3 !~ /^Z/ {
                     p = $1
                     s = $3
                     sub(/^ *[0-9]+ +[0-9]+ +[^ ]+ +/, "")
                     print p, s, $0
                   }'
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
  local pid st args lane ix
  printf '%s\n' "$1" | while read -r pid st args; do
    lane=$(lane_of "$pid")
    ix=""
    if [ -n "$lane" ]; then
      ix=$(awk -v l="$lane" '$0 == l { print NR - 1; exit }' "$WORK/lanes")
    fi
    printf '    left: pid %s %s lane %s %s: %.160s\n' \
      "$pid" "$st" "${ix:--}" "${lane:--}" "$args"
  done
}

count_lines() {
  printf '%s\n' "$1" | awk 'NF > 0' | wc -l | tr -d ' '
}

# The run ended within 5 s, before its signal: nothing of it may be left,
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
  drop_outs
}

# The end of a stopped run: 3 s later no process of the run may be left,
# nor a slice output file of its parent.  Sets RC to the parent's exit
# code.  False after a failure, which it reports and cleans up.
settled() {
  local left outs
  if ! left=$(wait_gone 3); then
    echo "  FAIL: $1: $(count_lines "$left") processes of the run left 3 s" \
         "later"
    report_left "$left"
    kill_run
    wait "$PARENT"
    drop_outs
    FAIL=1
    return 1
  fi
  wait "$PARENT"
  RC=$?
  outs=$(left_outs)
  if [ -n "$outs" ]; then
    echo "  FAIL: $1: the run left $(count_lines "$outs") slice output files" \
         "in /tmp"
    drop_outs
    FAIL=1
    return 1
  fi
  return 0
}

# Sets PFILE to the file of problems.tsv path $1, relative to TPTP_ROOT.
# Exits 2 when there is none, or when it is in the J13 layout.
problem_file() {
  case /$1 in
    */UEQ/*)
      echo "$0: $1 is in the J13 layout; J13 stays sealed (plan section" \
           "1.4)" >&2
      exit 2
      ;;
  esac
  PFILE=$ROOT/$1
  if [ ! -f "$PFILE" ]; then
    echo "$0: no problem file $PFILE" >&2
    exit 2
  fi
}

# The run environment with slice cap $1 (empty: the binary's default):
# env -i keeps only TPTP, the width and the cap.
run_env() {
  ENVV=(TPTP="$ROOT" THVM_ATP_SCHED_MAX_PAR="$WIDTH")
  if [ -n "$1" ]; then
    ENVV+=(THVM_ATP_RSS_ABORT_MB="$1")
  fi
}

describe() {
  echo "script $SCRIPT, sha256 $(sha256sum "$SCRIPT" | cut -d' ' -f1)"
  echo "binary $FP, sha256 $(sha256sum "$FP" | cut -d' ' -f1)"
  echo "$NL lanes, width $WIDTH, slice cap ${CAP:-(default)}${CAP:+ MB}," \
       "$(nproc) cpus, MemTotal $((MEMKB / 1024)) MB"
}

# Launch one run of PROB in the background: $1 wall, then its mode flags.
# A --lane run is one slice, so it keeps the binary's own slice cap.
start_run() {
  local w=$1
  shift
  case $1 in
    --lane=*)
      run_env "${THVM_ATP_RSS_ABORT_MB:-}"
      ;;
    *)
      run_env "$CAP"
      ;;
  esac
  new_link
  env -i "${ENVV[@]}" "$LINK" "$@" --szs --proof --wall="$w" "$PROB" \
    < /dev/null > "$WORK/last.out" 2> "$WORK/last.err" &
  PARENT=$!
  T0=$(now)
}

# One --wall=60 run ($3...: its mode flags) whose parent gets signal $2 5 s
# after launch; $1 names the case.
case_signal() {
  local label=$1 sig=$2 lanes want
  shift 2
  want=$((128 + $(kill -l "$sig")))
  start_run 60 "$@"
  sleep 5
  lanes=$(live_lanes)
  if ! alive "$PARENT"; then
    ended_early "$label"
    return
  fi
  kill -s "$sig" "$PARENT" 2> /dev/null
  if ! settled "$label"; then
    return
  fi
  if [ "$RC" -ne "$want" ]; then
    echo "  INCONCLUSIVE: $label: the run ended (exit $RC) before the signal" \
         "reached its teardown handler"
    INCONC=1
  elif ! has_last "$lanes"; then
    echo "  INCONCLUSIVE: $label: lane $LASTIX ($LASTNAME) was not alive" \
         "at 5 s"
    INCONC=1
  else
    echo "  ok: $label at 5 s: the teardown ended the run (exit $RC) and" \
         "nothing was left 3 s later; lanes alive at 5 s:" \
         "$(count_lines "$lanes"), $LASTNAME among them"
  fi
}

# One --wall=60 run ($1: --schedule or --lane=<last lane>) in which only the
# forked slice process of the last lane gets SIGTERM, 5 s after launch.
# 3 s later that slice's process group must be gone.  A --schedule run's
# parent and other lanes must go on, and SIGTERM to the parent then ends
# the run as in case_signal; a --lane run's parent must have ended by
# itself, with nothing of the run left.
case_slice() {
  local mode=$1 label before after slice grp n0 n1
  if [ "$mode" = --schedule ]; then
    label="slice $LASTNAME of --schedule, SIGTERM"
  else
    label="slice of $mode, SIGTERM"
  fi
  start_run 60 "$mode"
  sleep 5
  before=$(live_lanes)
  if ! alive "$PARENT"; then
    ended_early "$label"
    return
  fi
  if ! slice=$(slice_of "$LASTNAME"); then
    kill -TERM "$PARENT" 2> /dev/null
    if settled "$label"; then
      echo "  INCONCLUSIVE: $label: lane $LASTIX ($LASTNAME) was not alive" \
           "at 5 s"
      INCONC=1
    fi
    return
  fi
  kill -TERM "$slice" 2> /dev/null
  sleep 3
  grp=$(group_procs "$slice")
  if [ -n "$grp" ]; then
    echo "  FAIL: $label at 5 s: $(count_lines "$grp") processes of the" \
         "slice's group (pgid $slice) left 3 s later"
    report_left "$grp"
    kill_run
    wait "$PARENT"
    drop_outs
    FAIL=1
    return
  fi
  if [ "$mode" != --schedule ]; then
    if alive "$PARENT"; then
      echo "  FAIL: $label at 5 s: the parent still ran 3 s after its only" \
           "slice ended"
      kill_run
      wait "$PARENT"
      drop_outs
      FAIL=1
    elif settled "$label"; then
      echo "  ok: $label at 5 s: the slice's group was gone 3 s later, the" \
           "parent ended by itself (exit $RC), and nothing was left"
    fi
    return
  fi
  after=$(live_lanes)
  n0=$(others_alive "$before" "$before")
  n1=$(others_alive "$before" "$after")
  if ! alive "$PARENT"; then
    echo "  FAIL: $label at 5 s: the parent ended within 3 s; $n1 of the" \
         "$n0 other lanes alive at 5 s were alive 3 s later"
    kill_run
    wait "$PARENT"
    drop_outs
    FAIL=1
    return
  fi
  if [ "$n0" -gt 0 ] && [ "$n1" -eq 0 ]; then
    echo "  FAIL: $label at 5 s: none of the $n0 other lanes alive at 5 s" \
         "was alive 3 s later"
    kill_run
    wait "$PARENT"
    drop_outs
    FAIL=1
    return
  fi
  kill -TERM "$PARENT" 2> /dev/null
  if ! settled "$label"; then
    return
  fi
  if [ "$n0" -eq 0 ]; then
    echo "  INCONCLUSIVE: $label: no lane but $LASTNAME was alive at 5 s"
    INCONC=1
  elif [ "$RC" -ne 143 ]; then
    echo "  INCONCLUSIVE: $label: the run ended (exit $RC) before SIGTERM" \
         "reached its teardown handler"
    INCONC=1
  else
    echo "  ok: $label at 5 s: the slice's group was gone 3 s later, while" \
         "the parent and $n1 of the $n0 other lanes alive at 5 s went on;" \
         "SIGTERM to the parent then ended the run (exit $RC) and left" \
         "nothing"
  fi
}

# The lane matrix runner's stop for a stuck worker, pkill -TERM -f
# "^BIN --lane=", on a --wall=60 --lane run of the last lane 5 s after
# launch: the pattern matches the parent and its slice, which share the
# command line, and nothing of the run may be left 3 s later.
case_pkill() {
  local label="pkill -TERM -f \"^BIN --lane=\"" lanes re
  start_run 60 --lane="$LASTNAME"
  sleep 5
  lanes=$(live_lanes)
  if ! alive "$PARENT"; then
    ended_early "$label"
    return
  fi
  re=$(printf '%s\n' "$LINK" | sed 's/[]\\.[^$*+?(){}|]/\\&/g')
  if ! pkill -TERM -f -- "^$re --lane="; then
    echo "  FAIL: $label: the pattern matched no process of the run"
    kill_run
    wait "$PARENT"
    drop_outs
    FAIL=1
    return
  fi
  if ! settled "$label"; then
    return
  fi
  if ! has_last "$lanes"; then
    echo "  INCONCLUSIVE: $label: lane $LASTIX ($LASTNAME) was not alive" \
         "at 5 s"
    INCONC=1
  else
    echo "  ok: $label at 5 s: nothing was left 3 s later (parent exit $RC)"
  fi
}

# How a run with wall $1 that ended after $2 s with exit code $3 ended (its
# stderr in $WORK/last.err): the parent's wall stop, its wall alarm, at the
# wall (its lanes stopped on their own clocks at $1 - 4 s or later), or
# before the wall.
end_how() {
  if grep -q '^FindProof: schedule stopped at the wall' "$WORK/last.err"; then
    echo "the wall stop"
  elif [ "$3" -eq 142 ]; then
    echo "the wall alarm"
  elif at_least "$2" $(($1 - 4)); then
    echo "at the wall"
  else
    echo "before the wall"
  fi
}

# One --wall=10 run left alone: it must end by 20 s and leave nothing.
case_wall() {
  local w=10 el
  start_run "$w" --schedule
  while alive "$PARENT" && ! at_least "$(since "$T0")" $((w + 10)); do
    sleep 0.2
  done
  el=$(since "$T0")
  if alive "$PARENT"; then
    echo "  FAIL: wall $w s: the run still went on at $el s"
    kill_run
    wait "$PARENT"
    drop_outs
    FAIL=1
    return
  fi
  if settled "wall $w s"; then
    echo "  ok: wall $w s: the run ended at $el s ($(end_how "$w" "$el" "$RC")," \
         "exit $RC), by $((w + 10)) s, and nothing was left"
  fi
}

dryrun() {
  local list=$1 w=$2 line name rel split rest seen="" p rc el left how
  local timeout_bin longest=0 nwall=0 probs=()
  if ! [[ $w =~ ^[1-9][0-9]*$ ]] || [ ! -r "$list" ]; then
    usage
  fi
  if ! timeout_bin=$(command -v timeout) || [ "${timeout_bin#/}" = "$timeout_bin" ]; then
    echo "$0: dryrun needs GNU timeout" >&2
    exit 2
  fi
  while IFS= read -r line || [ -n "$line" ]; do
    case $line in
      '' | '#'*)
        continue
        ;;
    esac
    split=""
    IFS=$'\t' read -r name rel split rest <<< "$line"
    if [ "$split" = j13 ]; then
      echo "$0: $name is a j13 row; J13 stays sealed (plan section 1.4)" >&2
      exit 2
    fi
    if [ "$split" != train ]; then
      echo "$0: '$line' is not a problems.tsv row of split train" >&2
      exit 2
    fi
    problem_file "$rel"
    case $seen in
      *"|$PFILE|"*)
        continue
        ;;
    esac
    seen="$seen|$PFILE|"
    probs+=("$PFILE")
  done < "$list"
  if [ ${#probs[@]} -ne "$DRY_N" ]; then
    echo "$0: $list names ${#probs[@]} distinct problems; the dry run takes" \
         "exactly $DRY_N" >&2
    exit 2
  fi
  WIDTH=16
  if [ -n "${THVM_ATP_RSS_ABORT_MB:-}" ]; then
    CAP=$THVM_ATP_RSS_ABORT_MB
  elif [ "$MEMKB" -lt $((72 * 1024 * 1024)) ]; then
    CAP=3500
  else
    CAP=""
  fi
  run_env "$CAP"
  describe
  echo "dryrun: $list, sha256 $(sha256sum < "$list" | cut -d' ' -f1)"
  echo "dryrun: $DRY_N TRAIN problems at --wall=$w, each must end by" \
       "$((w + 10)) s; a run that ends at $((w - 4)) s or later reached the wall"
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
    how=$(end_how "$w" "$el" "$rc")
    if [ "$how" != "before the wall" ]; then
      nwall=$((nwall + 1))
    fi
    if [ "$rc" -eq 124 ] || [ "$rc" -eq 137 ]; then
      echo "  FAIL: $name: the outer timeout stopped it (exit $rc) at $el s"
      FAIL=1
    elif ! at_least $((w + 10)) "$el"; then
      echo "  FAIL: $name: ended at $el s ($how), after the wall + 10 s"
      FAIL=1
    else
      echo "  ok: $name: ended at $el s ($how)"
    fi
    if ! left=$(wait_gone 3); then
      echo "  FAIL: $name: $(count_lines "$left") processes of the run" \
           "left 3 s after it ended"
      report_left "$left"
      kill_run
      FAIL=1
    fi
  done
  echo "dryrun: $nwall of $DRY_N runs reached the wall; the longest ended" \
       "at $longest s"
  if [ "$FAIL" != 0 ]; then
    echo "SCHED DRYRUN FAILED"
    exit 1
  elif [ $((2 * nwall)) -lt "$DRY_N" ]; then
    echo "SCHED DRYRUN INCONCLUSIVE: fewer than half of the runs reached" \
         "the wall, so the W + 10 s bound went untested"
    exit 3
  fi
  echo "SCHED DRYRUN OK"
}

if ! "$FP" --list-lanes > "$WORK/lanes" 2> /dev/null || [ ! -s "$WORK/lanes" ]; then
  echo "$0: $FP --list-lanes failed" >&2
  exit 2
fi
NL=$(wc -l < "$WORK/lanes" | tr -d ' ')
LASTIX=$((NL - 1))
LASTNAME=$(tail -n 1 "$WORK/lanes")
MEMKB=$(awk '/^MemTotal:/ { print $2 }' /proc/meminfo)

case $MODE in
  teardown)
    if [ $# -gt 3 ]; then
      usage
    fi
    problem_file "$PROBLEM"
    PROB=$PFILE
    WIDTH=$NL
    if [ -n "${THVM_ATP_RSS_ABORT_MB:-}" ]; then
      CAP=$THVM_ATP_RSS_ABORT_MB
    else
      CAP=$(awk -v m="$MEMKB" -v w="$WIDTH" 'BEGIN { c = int(m / 1024 * 0.9 / w)
                                                      if (c > 4500) { c = 4500 }
                                                      print c }')
    fi
    describe
    echo "teardown: $PROB"
    case_signal SIGTERM TERM --schedule
    case_signal SIGINT INT --schedule
    case_signal SIGALRM ALRM --schedule
    case_signal "lane $LASTNAME, SIGTERM" TERM --lane="$LASTNAME"
    case_slice --schedule
    case_slice --lane="$LASTNAME"
    case_pkill
    SIG_FAIL=$FAIL
    SIG_INCONC=$INCONC
    FAIL=0
    case_wall
    WALL_FAIL=$FAIL
    if [ "$SIG_FAIL" != 0 ]; then
      SIGV=FAILED
    elif [ "$SIG_INCONC" != 0 ]; then
      SIGV=INCONCLUSIVE
    else
      SIGV=OK
    fi
    WALLV=OK
    if [ "$WALL_FAIL" != 0 ]; then
      WALLV=FAILED
    fi
    echo "signal cases: $SIGV; wall case: $WALLV"
    if [ "$SIG_FAIL" != 0 ] || [ "$WALL_FAIL" != 0 ]; then
      echo "SCHED TEARDOWN FAILED"
      exit 1
    elif [ "$SIG_INCONC" != 0 ]; then
      echo "SCHED TEARDOWN INCONCLUSIVE"
      exit 3
    fi
    echo "SCHED TEARDOWN OK"
    ;;
  dryrun)
    if [ $# -ne 5 ]; then
      usage
    fi
    dryrun "$4" "$5"
    ;;
  *)
    usage
    ;;
esac
