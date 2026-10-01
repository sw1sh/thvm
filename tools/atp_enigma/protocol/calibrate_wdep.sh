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
# status, proof and time), plus THVM_ATP_RSS_ABORT_MB=3500 on a pod below
# 72 GiB (J13 ran with 128 GiB and the schedule's 4500 MB cap: use
# a pod with at least 72 GiB to keep that cap).  Shards split the problems
# (all four walls of a problem on one pod, in a fixed-seed shuffled order),
# so 3-4 pods share the 1,600 runs.  Each pod first passes the pod-speed
# calibration against one shared reference table, measured once with this
# same binary on the first pod (calib-ref).
#
# report compares each wall's solved set (SZS Unsatisfiable and a complete
# CNFRefutation block within the wall: CASC's "+", a solution output) with
# the official J13 UEQ result (FindProof column, 269 solved), per problem:
# G0.3 passes at the wall with the highest agreement among those with
# agreement >= 95% and a total within 8 of 269 (ties: total closer to 269,
# then the lower wall).  Otherwise WDEP = 180 / c, c the geometric mean of
# official / pod time over problems solved in both with official time >=
# 1 s in the 70 s run, and every WDEP number is labelled uncalibrated.  The
# official table is public data the plan designates for this calibration;
# the seal does not cover these runs.
#
# Usage (on a pod, except report):
#   bash calibrate_wdep.sh setup DIR FINDPROOF_TGZ J13_UEQ_TGZ TPTP_POOL_TGZ
#        unpack, check the run script, write DIR/problems.tsv and DIR/arms.tsv,
#        copy this script, run_arms.py and stats.py into DIR/bin (run those)
#   bash DIR/bin/calibrate_wdep.sh calib-ref DIR CANDIDATES_TSV
#        once, on the first pod: DIR/calib_ref.tsv (copy it to the other pods)
#   bash DIR/bin/calibrate_wdep.sh run DIR K/N CALIB_REF [run_arms options]
#        shard K of N; output DIR/out_K (rows.tsv, run.started, run.done, ...)
#   bash calibrate_wdep.sh report [stats.py wdep options] -- OUT...
#        OUT: harvested out_K directories or .tgz archives of them
# Launch detached, e.g.
#   nohup setsid bash DIR/bin/calibrate_wdep.sh run DIR 1/4 DIR/calib_ref.tsv \
#     </dev/null >DIR/run_1.log 2>&1 &

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

do_setup() {
  local here=$1 dir fp_tgz j13_tgz pool_tgz rs bin n flags exec_line par_line
  [ $# -eq 5 ] || die "setup DIR FINDPROOF_TGZ J13_UEQ_TGZ TPTP_POOL_TGZ"
  dir=$2
  fp_tgz=$3
  j13_tgz=$4
  pool_tgz=$5
  for f in "$fp_tgz" "$j13_tgz" "$pool_tgz"; do
    [ -f "$f" ] || die "no file $f"
  done
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
  "$bin" --help > /dev/null 2>&1 || die "$bin does not run on this pod"
  n=$(ls "$dir"/j13/UEQ/*.p | wc -l)
  [ "$n" -eq 400 ] || die "expected 400 J13 UEQ problems, found $n"
  for f in "$dir"/j13/UEQ/*.p; do
    printf '%s\tUEQ/%s\tj13\n' "$(basename "$f" .p)" "$(basename "$f")"
  done > "$dir/problems.tsv"
  printf 'SUB\t%s\t--schedule\n' "$bin" > "$dir/arms.tsv"
  cp "$here/calibrate_wdep.sh" "$here/run_arms.py" "$here/stats.py" "$dir/bin/"
  {
    echo "setup=$(date -u +%FT%TZ) host=$(hostname)"
    echo "nproc=$(nproc) mem_kb=$(awk '/^MemTotal:/ { print $2 }' /proc/meminfo)"
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
  if [ "$(nproc)" -ne 16 ]; then
    echo "calibrate_wdep: WARNING: $(nproc) vCPUs; the protocol is a 16-vCPU pod" >&2
  fi
  if [ "$(awk '/^MemTotal:/ { print $2 }' /proc/meminfo)" -lt $((72 * 1024 * 1024)) ]; then
    echo "calibrate_wdep: WARNING: below 72 GiB: every run gets THVM_ATP_RSS_ABORT_MB=3500," \
         "not the 4500 MB cap J13 ran with" >&2
  fi
  cat "$dir/setup.txt"
}

do_calib_ref() {
  local here=$1 dir cands
  [ $# -eq 3 ] || die "calib-ref DIR CANDIDATES_TSV"
  dir=$(cd "$2" && pwd) || die "no DIR $2"
  cands=$3
  python3 "$dir/bin/run_arms.py" calib-ref --calib "$cands" \
    --calib-bin "$dir/sub/FindProof/bin/FindProof" --calib-root "$dir/tptp_pool" \
    --ref-out "$dir/calib_ref.tsv" --out "$dir/calib_ref_out"
}

do_run() {
  local here=$1 dir shard ref k
  [ $# -ge 4 ] || die "run DIR K/N CALIB_REF [run_arms options]"
  dir=$(cd "$2" && pwd) || die "no DIR $2"
  shard=$3
  ref=$4
  shift 4
  [ -f "$ref" ] || die "no calibration reference $ref"
  [ -f "$dir/problems.tsv" ] && [ -f "$dir/arms.tsv" ] || die "run setup first"
  k=${shard%%/*}
  python3 "$dir/bin/run_arms.py" run --problems "$dir/problems.tsv" --arms "$dir/arms.tsv" \
    --walls 45,53,60,70 --passes 1 --no-repeats --no-check --no-errdir --sealed off \
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
  python3 "$here/stats.py" wdep ${opts[@]+"${opts[@]}"} --rows "${outs[@]}"
}

# Everything above is a function, so bash has read the whole file before
# it runs any of it: editing the file under a running instance is safe.
main "$@"
exit $?
