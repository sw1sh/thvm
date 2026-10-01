#!/bin/bash
# Rerun the 24 (problem, lane) rows that crashed in the G0.1 lane matrix
# (docs/plans/enigma_portfolio.md M0; ~/.cache/thvm/enigma/matrix/g01.txt):
# SWX233-1 on 14 lanes, SYN986-10.005 and SYN986-10.006 on 5 lanes each,
# all slice exit 139 (SIGSEGV) at 60 s on binary d3946ff1.
#
# Two passes, each row one `--lane=NAME --szs --wall=60` run exactly as
# the matrix runner ran it (clean environment, TPTP set, the lane's own
# 4.5 GB slice RSS cap):
#   1. the competition build under `ulimit -s 8192`, FindProof's
#      deployment stack;
#   2. an AddressSanitizer build of the same tree under `ulimit -s 1024`,
#      so a per-level recursion on a deep term overflows early and ASan
#      names the frame (a stack overflow report whose frames repeat is a
#      recursion; one large frame is not).
# The script fails if any run ends on a signal, exits other than 0 / 1,
# or leaves an ASan report.  Reports land in <out_dir>/<pass>/<row>.*.
#
# Usage: crash_rows.sh <TPTP_root> <out_dir> [wall=60]
# Pod-only (compute).  Run from the repository root.  Each run has its
# own `timeout`, so the script ends on its own; it runs the rows one at
# a time (the ASan build takes several GB per slice).
set -u
ROOT=$1
OUT=$2
WALL=${3:-60}
mkdir -p "$OUT/deploy" "$OUT/asan" || exit 2

ROWS="SWX233-1 wm
SWX233-1 unfold
SWX233-1 flatten
SWX233-1 flatten-bare
SWX233-1 twee
SWX233-1 twee-deep
SWX233-1 twee-freq
SWX233-1 tgt-ax
SWX233-1 mnf-gj-amw
SWX233-1 encompass
SWX233-1 twee-raw
SWX233-1 flatten-bare-raw
SWX233-1 wm-gj
SWX233-1 lazy-gj
SYN986-10.005 tgt-sel
SYN986-10.005 bare
SYN986-10.005 ifeq
SYN986-10.005 bare-raw
SYN986-10.005 bare-gj
SYN986-10.006 tgt-sel
SYN986-10.006 bare
SYN986-10.006 ifeq
SYN986-10.006 bare-raw
SYN986-10.006 bare-gj"

CFL="-std=c11 -O2 -w -D_GNU_SOURCE"
make -B -f tools/casc/Makefile bin/FindProof CFLAGS="$CFL -g -O1 -fsanitize=address -fno-omit-frame-pointer" \
  > "$OUT/build_asan.log" 2>&1 || { echo "asan build failed"; exit 2; }
mv bin/FindProof bin/FindProof_asan
make -B -f tools/casc/Makefile bin/FindProof CFLAGS="$CFL" > "$OUT/build.log" 2>&1 \
  || { echo "build failed"; exit 2; }
sha256sum bin/FindProof bin/FindProof_asan > "$OUT/binaries.sha256"
git rev-parse HEAD > "$OUT/commit.txt"

bad=0
run_pass() {
  pass=$1
  bin=$2
  stack_kb=$3
  printf '%s\n' "$ROWS" | while read -r prob lane; do
    dom=${prob:0:3}
    key="$prob.$lane"
    log="$OUT/$pass/$key"
    (
      ulimit -s "$stack_kb"
      cd /tmp && env -i PATH=/usr/local/bin:/usr/bin:/bin HOME="$HOME" TPTP="$ROOT" \
        ASAN_OPTIONS="detect_leaks=0:log_path=$log.asan" \
        timeout -k 5 $((WALL + 60)) "$OLDPWD/$bin" --lane="$lane" --szs --wall="$WALL" \
        "$ROOT/Problems/$dom/$prob.p" > "$log.out" 2> "$log.err" < /dev/null
    )
    rc=$?
    szs=$(grep -o 'SZS status [A-Za-z]*' "$log.out" | head -1)
    # The lane's slice reports its end as "% FindProof lane NAME: exit N"
    # or "...: signal N" on stderr (G0.1's crashes were "exit 139").
    slice=$(grep -o 'FindProof lane [^:]*: [a-z]* -*[0-9]*' "$log.err" | tail -n 1 \
            | awk '{ print $(NF - 1) "=" $NF }')
    nasan=$(cat "$log".asan.* 2> /dev/null | grep -c 'ERROR: AddressSanitizer')
    verdict=ok
    if [ "$rc" != 0 ] && [ "$rc" != 1 ]; then
      verdict=FAIL
    fi
    # A wall stop kills the slice (signal 9 or 15) and reports Timeout.
    case "$slice" in
      exit=0 | exit=1) ;;
      signal=9 | signal=15)
        if [ "$szs" != "SZS status Timeout" ]; then
          verdict=FAIL
        fi
        ;;
      *) verdict=FAIL ;;
    esac
    if [ "$nasan" != 0 ]; then
      verdict=FAIL
    fi
    printf '%s\t%s\t%s\trc=%s\tslice=%s\t%s\tasan_reports=%s\n' \
      "$verdict" "$pass" "$key" "$rc" "${slice:-none}" "${szs:-no-szs}" "$nasan" \
      | tee -a "$OUT/summary.tsv"
  done
}

: > "$OUT/summary.tsv"
run_pass deploy bin/FindProof 8192
run_pass asan bin/FindProof_asan 1024
nfail=$(grep -c '^FAIL' "$OUT/summary.tsv")
echo "crash_rows: $nfail of 48 runs failed; summary $OUT/summary.tsv"
[ "$nfail" = 0 ] || bad=1
exit $bad
