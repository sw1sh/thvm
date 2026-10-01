#!/bin/bash
# M0 WDEP calibration, gate G0.3 (docs/plans/enigma_portfolio.md, sections
# 1.3 and 4).
#
# The J13 submission build (~/.cache/thvm/casc/FindProof.tgz: a static
# x86_64 FindProof and its StarExec run script) runs the 400 J13 UEQ
# problems at walls 45, 53, 60 and 70 s, one problem at a time on a 16-vCPU
# pod, with the run script's own command line, which setup checks:
#   THVM_ATP_SCHED_MAX_PAR=$(nproc) FindProof --schedule --szs --proof --wall=$LIMIT <p>
# run_arms.py runs it as the single arm SUB (flags --schedule; the runner
# adds --szs --proof --wall=W) under env -i, timeout -k 5 (W+15), stdin
# /dev/null, THVM_ATP_SCHED_MAX_PAR=16 and nothing else (that July build has
# no slice stderr directory and prints no winner line, so the rows carry
# status, proof and time).  J13 ran with the schedule's own 4500 MB slice
# cap, so setup and run refuse a pod whose effective memory, min(MemTotal,
# cgroup limit), is below 16 x 4500 MiB, or whose effective CPUs (affinity,
# cgroup quota) are not 16.  --allow-small-pod runs every job at
# THVM_ATP_RSS_ABORT_MB=3500 instead, and stats.py then labels the result
# uncalibrated.  Shards split the problems (all four walls of a problem on
# one pod, in a fixed-seed shuffled order), so 3-4 pods share the 1,600
# runs.  Each pod first passes the pod-speed calibration against one shared
# reference table, measured once with this same binary on the first pod
# (calib-ref).
#
# report compares each wall's solved set (SZS Unsatisfiable and a complete
# CNFRefutation block within the wall: CASC's "+", a solution output; empty
# blocks count and are reported apart) with
# the official J13 UEQ result (FindProof column, 269 solved), per problem:
# G0.3 passes at the wall with the highest agreement among those with
# agreement >= 95% and a total within 8 of 269 (ties: total closer to 269,
# then the lower wall).  Otherwise WDEP = 180 / c, c the geometric mean of
# official / pod time over problems solved in both with official time >=
# 1 s in the 70 s run, and every WDEP number is labelled uncalibrated.  The
# runs are J13 runs of look L0 and stay under the seal: the runner logs
# heartbeats only and packs its per-problem outputs into sealed.tgz, and
# report prints the per-wall aggregates and the G0.3 decision and writes
# the per-problem agreement table into ~/.cache/thvm/enigma/sealed/.
#
# Usage (on a pod, except report):
#   bash calibrate_wdep.sh setup [--allow-small-pod] DIR FINDPROOF_TGZ J13_UEQ_TGZ TPTP_POOL_TGZ
#        check the pod, unpack, check the run script, write DIR/problems.tsv,
#        DIR/arms.tsv and DIR/rss_cap, copy this script, run_arms.py and
#        stats.py into DIR/bin (run those)
#   bash DIR/bin/calibrate_wdep.sh calib-ref DIR CANDIDATES_TSV
#        once, on the first pod: DIR/calib_ref.tsv (copy it to the other pods)
#   bash DIR/bin/calibrate_wdep.sh run DIR K/N CALIB_REF [run_arms options]
#        shard K of N; output DIR/out_K (sealed.tgz, run.started, run.done, ...)
#   bash calibrate_wdep.sh report [stats.py wdep options] -- OUT...
#        OUT: harvested out_K directories or .tgz archives of them (look L0)
# Launch detached, with the driver's absolute deadline, e.g.
#   nohup setsid bash DIR/bin/calibrate_wdep.sh run DIR 1/4 DIR/calib_ref.tsv \
#     --deadline-utc 2026-10-04T06:00:00Z </dev/null >DIR/run_1.log 2>&1 &

main() {
  set -u
  local here cmd
  here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
  cmd=${1:-}
  if [ $# -gt 0 ]; then
    shift
  fi
  case "$cmd" in
    setup) do_setup "$here" "$@" ;;
    calib-ref) do_calib_ref "$here" "$@" ;;
    run) do_run "$here" "$@" ;;
    report) do_report "$here" "$@" ;;
    *)
      sed -n '2,/^$/p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
      return 2
      ;;
  esac
}

die() {
  echo "calibrate_wdep: $*" >&2
  exit 2
}

# The pod's effective CPUs and memory (run_arms.py hostfacts); dies unless
# they meet the protocol.  Prints the rss cap to use: default, or 3500 with
# --allow-small-pod on a pod too small for the default caps.
pod_rss_cap() {
  local here=$1 small=$2 facts cpus fit
  facts=$(python3 "$here/run_arms.py" hostfacts) || die "hostfacts failed"
  cpus=$(echo "$facts" | sed -n 's/^eff_cpus=//p')
  fit=$(echo "$facts" | sed -n 's/^default_caps_fit=//p')
  [ "$cpus" = 16 ] || die "$cpus effective CPUs; the protocol is a 16-vCPU pod ($(echo $facts))"
  if [ "$fit" = 1 ]; then
    echo default
  elif [ "$small" = 1 ]; then
    echo "calibrate_wdep: WARNING: --allow-small-pod: every job at THVM_ATP_RSS_ABORT_MB=3500;" \
         "the WDEP result will be uncalibrated" >&2
    echo 3500
  else
    die "effective memory below 16 x 4500 MiB, J13's caps ($(echo $facts)); use a larger pod" \
        "(or --allow-small-pod: uncalibrated)"
  fi
}

do_setup() {
  local here=$1 small=0 dir fp_tgz j13_tgz pool_tgz rs bin n flags exec_line par_line cap
  shift
  if [ "${1:-}" = --allow-small-pod ]; then
    small=1
    shift
  fi
  [ $# -eq 4 ] || die "setup [--allow-small-pod] DIR FINDPROOF_TGZ J13_UEQ_TGZ TPTP_POOL_TGZ"
  dir=$1
  fp_tgz=$2
  j13_tgz=$3
  pool_tgz=$4
  for f in "$fp_tgz" "$j13_tgz" "$pool_tgz"; do
    [ -f "$f" ] || die "no file $f"
  done
  cap=$(pod_rss_cap "$here" "$small") || exit 2
  mkdir -p "$dir"/bin "$dir"/sub "$dir"/j13 "$dir"/tptp_pool
  dir=$(cd "$dir" && pwd)
  tar xzf "$fp_tgz" -C "$dir/sub" || die "cannot unpack $fp_tgz"
  tar xzf "$j13_tgz" -C "$dir/j13" || die "cannot unpack $j13_tgz"
  tar xzf "$pool_tgz" -C "$dir/tptp_pool" || die "cannot unpack $pool_tgz"
  rs=$dir/sub/FindProof/bin/starexec_run_default
  bin=$dir/sub/FindProof/bin/FindProof
  [ -f "$rs" ] || die "no run script in $fp_tgz"
  [ -x "$bin" ] || die "no FindProof binary in $fp_tgz"
  # The run script must be the CASC command line the arm reproduces.
  exec_line=$(grep -E '^exec ' "$rs")
  par_line=$(grep -E '^export THVM_ATP_SCHED_MAX_PAR=' "$rs")
  [ "$exec_line" = 'exec "$HERE/bin/FindProof" --schedule --szs --proof --wall="$LIMIT" "$1"' ] \
    || die "unexpected run script exec line: $exec_line"
  [ "$par_line" = 'export THVM_ATP_SCHED_MAX_PAR="$CORES"' ] \
    || die "unexpected run script width line: $par_line"
  flags=$(echo "$exec_line" | sed 's/.*bin\/FindProof" //; s/ --wall=.*//')
  [ "$flags" = "--schedule --szs --proof" ] || die "unexpected flags: $flags"
  timeout 30 "$bin" --help </dev/null >/dev/null 2>&1 || die "$bin does not run on this pod"
  n=$(ls "$dir"/j13/UEQ/*.p | wc -l)
  [ "$n" -eq 400 ] || die "expected 400 J13 UEQ problems, found $n"
  for f in "$dir"/j13/UEQ/*.p; do
    printf '%s\tUEQ/%s\tj13\n' "$(basename "$f" .p)" "$(basename "$f")"
  done > "$dir/problems.tsv"
  printf 'SUB\t%s\t--schedule\n' "$bin" > "$dir/arms.tsv"
  echo "$cap" > "$dir/rss_cap"
  cp "$here/calibrate_wdep.sh" "$here/run_arms.py" "$here/stats.py" "$dir/bin/"
  {
    echo "setup=$(date -u +%FT%TZ) host=$(hostname)"
    python3 "$here/run_arms.py" hostfacts | tr '\n' ' '
    echo
    echo "rss_cap=$cap"
    echo "lscpu=$(lscpu | grep -m1 'Model name' | sed 's/.*: *//') tpc=$(lscpu | grep -m1 'Thread(s) per core' | sed 's/.*: *//')"
    echo "findproof_tgz=$fp_tgz sha256=$(sha256sum "$fp_tgz" | cut -c1-64)"
    echo "findproof_bin sha256=$(sha256sum "$bin" | cut -c1-64)"
    echo "run_script sha256=$(sha256sum "$rs" | cut -c1-64)"
    echo "run_script_exec=$exec_line"
    echo "run_script_width=$par_line"
    echo "arm=SUB flags=--schedule (runner adds --szs --proof --wall=W)"
    echo "j13_tgz=$j13_tgz sha256=$(sha256sum "$j13_tgz" | cut -c1-64) problems=$n"
    echo "pool_tgz=$pool_tgz sha256=$(sha256sum "$pool_tgz" | cut -c1-64)"
    for f in calibrate_wdep.sh run_arms.py stats.py; do
      echo "$f sha256=$(sha256sum "$dir/bin/$f" | cut -c1-64)"
    done
  } > "$dir/setup.txt"
  cat "$dir/setup.txt"
}

do_calib_ref() {
  local here=$1 dir cands
  [ $# -eq 3 ] || die "calib-ref DIR CANDIDATES_TSV"
  dir=$(cd "$2" && pwd) || die "no DIR $2"
  cands=$3
  [ -f "$dir/rss_cap" ] || die "run setup first"
  python3 "$dir/bin/run_arms.py" calib-ref --calib "$cands" \
    --calib-bin "$dir/sub/FindProof/bin/FindProof" --calib-root "$dir/tptp_pool" \
    --rss-cap "$(cat "$dir/rss_cap")" --ref-out "$dir/calib_ref.tsv" --out "$dir/calib_ref_out"
}

do_run() {
  local here=$1 dir shard ref k
  [ $# -ge 4 ] || die "run DIR K/N CALIB_REF [run_arms options]"
  dir=$(cd "$2" && pwd) || die "no DIR $2"
  shard=$3
  ref=$4
  shift 4
  [ -f "$ref" ] || die "no calibration reference $ref"
  [ -f "$dir/problems.tsv" ] && [ -f "$dir/arms.tsv" ] && [ -f "$dir/rss_cap" ] \
    || die "run setup first"
  k=${shard%%/*}
  # run_arms.py checks the pod again (CPUs, and memory for the default cap).
  python3 "$dir/bin/run_arms.py" run --problems "$dir/problems.tsv" --arms "$dir/arms.tsv" \
    --walls 45,53,60,70 --passes 1 --no-repeats --no-check --no-errdir --sealed on \
    --rss-cap "$(cat "$dir/rss_cap")" \
    --root "$dir/j13" --calib-ref "$ref" --calib-bin "$dir/sub/FindProof/bin/FindProof" \
    --calib-root "$dir/tptp_pool" --shard "$shard" --out "$dir/out_$k" "$@"
}

do_report() {
  local here=$1 opts=() outs=()
  shift
  while [ $# -gt 0 ] && [ "$1" != "--" ]; do
    opts+=("$1")
    shift
  done
  [ $# -gt 0 ] && shift
  outs=("$@")
  [ ${#outs[@]} -gt 0 ] || die "report [stats.py wdep options] -- OUT..."
  python3 "$here/stats.py" wdep --look L0 ${opts[@]+"${opts[@]}"} --rows "${outs[@]}"
}

# Everything above is a function and the last line holds both the call and
# the exit, so bash has read the whole file before it runs any of it.
main "$@"; exit $?
