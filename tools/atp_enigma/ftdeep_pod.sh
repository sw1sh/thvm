#!/bin/bash
# Pod validation of the explicit-stack branch (enigma-ftdeep) against main
# before it merges: the deep-term tests, the unarmed-identity pins and a
# paired cost measurement.  Writes everything under <out_dir>; the
# numbers go into the commit message and docs/ATP/enigma_portfolio.md 5.4.
#
#   1. Builds, in each tree: the competition FindProof
#      (tools/casc/Makefile) and bin/test_atp_wolfram_bench; sha256 of each.
#   2. Branch tree only: test_ft_deep, test_atp_deep, test_atp_cli_deep,
#      test_atp_deep_cp and test_atp, one at a time, each under its own
#      timeout and GNU time (peak RSS per binary, for the Makefile note).
#   3. The G2.3 pins on both builds: OA and DN under
#      THVM_ATP_WALDMEISTER=1 THVM_ATP_LAZY_NORM=1 (their stdout must be
#      identical apart from timing lines), with user-space instruction
#      counts (perf stat -e instructions:u, when perf works) and CPU time.
#   4. Paired lane runs: every problem in <problems.txt> on the lanes
#      twee, wm, otter-lpo (LPO, --lrs) and tgt-sel (--lrs), main then
#      branch, serially: SZS status, the engine's "=> " stats line,
#      instructions and CPU seconds.  A row whose stats line differs is a
#      search that changed: expected only where a clock gates a decision
#      (--lrs, budgets) or a run ends at the wall.
# summary.tsv holds one line per paired run with the branch / main
# instruction and CPU ratios; the script prints their geometric means.
# Accept the branch when the instruction overhead is within noise (at
# most 2%) and every stats difference is explained by a clock or the wall.
#
# Usage: ftdeep_pod.sh <main_tree> <branch_tree> <TPTP_root> <problems.txt> <out_dir> [wall=60]
#   problems.txt: one TPTP-relative path per line (Problems/XXX/Y.p),
#   about 20 TRAIN problems.
# Pod-only (compute).  Runs serially; every run has its own timeout.
set -u
MAIN=$1
BR=$2
ROOT=$3
PROBS=$4
OUT=$5
WALL=${6:-60}
mkdir -p "$OUT/tests" "$OUT/pins" "$OUT/lanes" || exit 2

PERF=""
if perf stat -x, -e instructions:u true > /dev/null 2>&1; then
  PERF="perf stat -x, -e instructions:u -o"
fi

build() {
  tree=$1
  tag=$2
  (cd "$tree" && make -B -f tools/casc/Makefile bin/FindProof > "$OUT/build_$tag.log" 2>&1 &&
   make -B bin/test_atp_wolfram_bench >> "$OUT/build_$tag.log" 2>&1) || { echo "build $tag failed"; exit 2; }
  (cd "$tree" && git rev-parse HEAD && sha256sum bin/FindProof bin/test_atp_wolfram_bench) > "$OUT/sha_$tag.txt"
}
build "$MAIN" main
build "$BR" branch

# Instructions and CPU seconds of one command: run_measured <prefix> <cmd...>
run_measured() {
  pre=$1
  shift
  if [ -n "$PERF" ]; then
    /usr/bin/time -o "$pre.time" -f '%e %U %S %M %x' $PERF "$pre.perf" "$@" > "$pre.out" 2> "$pre.err" < /dev/null
  else
    /usr/bin/time -o "$pre.time" -f '%e %U %S %M %x' "$@" > "$pre.out" 2> "$pre.err" < /dev/null
  fi
}
instr_of() {
  if [ -f "$1.perf" ]; then
    grep 'instructions' "$1.perf" | cut -d, -f1
  else
    echo NA
  fi
}
cpu_of() {
  tail -n 1 "$1.time" | awk '{ printf "%.2f", $2 + $3 }'
}

# --- 2. deep tests and test_atp, branch tree --------------------------
for t in test_ft_deep test_atp_deep test_atp_cli_deep test_atp_deep_cp test_atp; do
  (cd "$BR" && make "bin/$t" > "$OUT/tests/$t.build.log" 2>&1) || { echo "build $t failed"; exit 2; }
  (cd "$BR" && /usr/bin/time -o "$OUT/tests/$t.time" -f '%e %U %S %M %x' \
     timeout -k 10 3600 "bin/$t" > "$OUT/tests/$t.out" 2> "$OUT/tests/$t.err" < /dev/null)
  rc=$?
  printf '%s\trc=%s\tpeak_rss_kb=%s\t%s\n' "$t" "$rc" \
    "$(tail -n 1 "$OUT/tests/$t.time" | awk '{ print $4 }')" \
    "$(tail -n 1 "$OUT/tests/$t.out")" | tee -a "$OUT/tests/summary.tsv"
done

# --- 3. OA / DN pins, both builds -------------------------------------
for pin in WolframAxioms__OrAssociativity WolframAxioms__DoubleNegation; do
  for tag in main branch; do
    tree=$MAIN
    [ "$tag" = branch ] && tree=$BR
    pre="$OUT/pins/$pin.$tag"
    (cd "$tree" && export THVM_ATP_WALDMEISTER=1 THVM_ATP_LAZY_NORM=1 &&
       run_measured "$pre" timeout -k 10 600 bin/test_atp_wolfram_bench \
       "tools/baselines/wm_pr/$pin.pr" 2000000 120)
  done
  same=no
  if diff <(grep -viE 'time|[0-9]s\b|us\b|ms\b' "$OUT/pins/$pin.main.out") \
          <(grep -viE 'time|[0-9]s\b|us\b|ms\b' "$OUT/pins/$pin.branch.out") > /dev/null; then
    same=yes
  fi
  printf 'pin\t%s\tstdout_same=%s\tinstr=%s/%s\tcpu=%s/%s\n' "$pin" "$same" \
    "$(instr_of "$OUT/pins/$pin.branch")" "$(instr_of "$OUT/pins/$pin.main")" \
    "$(cpu_of "$OUT/pins/$pin.branch")" "$(cpu_of "$OUT/pins/$pin.main")" | tee -a "$OUT/summary.tsv"
done

# --- 4. paired lane runs ---------------------------------------------
while read -r rel; do
  [ -z "$rel" ] && continue
  prob=$(basename "$rel" .p)
  for lane in twee wm otter-lpo tgt-sel; do
    for tag in main branch; do
      tree=$MAIN
      [ "$tag" = branch ] && tree=$BR
      pre="$OUT/lanes/$prob.$lane.$tag"
      (cd /tmp && env -i PATH=/usr/local/bin:/usr/bin:/bin HOME="$HOME" TPTP="$ROOT" \
         bash -c "$(declare -f run_measured); PERF='$PERF'; run_measured '$pre' \
         timeout -k 5 $((WALL + 30)) '$tree/bin/FindProof' --lane='$lane' --szs --wall='$WALL' '$ROOT/$rel'")
    done
    m="$OUT/lanes/$prob.$lane.main"
    b="$OUT/lanes/$prob.$lane.branch"
    sm=$(grep -o 'SZS status [A-Za-z]*' "$m.out" | head -n 1 | awk '{ print $3 }')
    sb=$(grep -o 'SZS status [A-Za-z]*' "$b.out" | head -n 1 | awk '{ print $3 }')
    same=no
    [ "$(grep -m1 '^=> ' "$m.err")" = "$(grep -m1 '^=> ' "$b.err")" ] && same=yes
    im=$(instr_of "$m")
    ib=$(instr_of "$b")
    cm=$(cpu_of "$m")
    cb=$(cpu_of "$b")
    printf 'lane\t%s\t%s\tszs=%s/%s\tstats_same=%s\tinstr_ratio=%s\tcpu_ratio=%s\n' \
      "$prob" "$lane" "${sb:-none}" "${sm:-none}" "$same" \
      "$(awk -v b="$ib" -v m="$im" 'BEGIN { if (m + 0 > 0 && b + 0 > 0) printf "%.4f", b / m; else print "NA" }')" \
      "$(awk -v b="$cb" -v m="$cm" 'BEGIN { if (m + 0 > 0) printf "%.4f", b / m; else print "NA" }')" \
      | tee -a "$OUT/summary.tsv"
  done
done < "$PROBS"

awk -F'\t' '$1 == "lane" {
  split($6, a, "="); split($7, c, "=");
  if (a[2] != "NA") { si += log(a[2]); ni++ }
  if (c[2] != "NA") { sc += log(c[2]); nc++ }
  if ($5 != "stats_same=yes") nd++
}
END {
  printf "paired lane runs: instructions branch/main geomean %s over %d, CPU %s over %d, stats differ on %d\n",
    (ni ? sprintf("%.4f", exp(si / ni)) : "NA"), ni, (nc ? sprintf("%.4f", exp(sc / nc)) : "NA"), nc, nd + 0
}' "$OUT/summary.tsv" | tee "$OUT/verdict.txt"
