#!/bin/bash
# Gate G2.1 (docs/plans/enigma_portfolio.md, M2): history export v2 only
# observes.  Every listed (family, lane, problem) runs twice through the
# harvest path -- FindProof --lane=NAME --szs --proof under the
# instruction work clock (THVM_ATP_WORK_CLOCK=instr) -- with the export
# off, then on (THVM_ATP_HISTORY_EXPORT, rate 1), and the two runs must
# agree on
#   the selection sequence  every CPSEL / CPSKIP line of the per-pick trace
#                           (THVM_ATP_CP_PICK_TRACE: pick, heap position,
#                           seq, priority, rules, both sides);
#   the proof               stdout: the SZS status line and the proof;
#   the stats               the engine's deterministic stats lines (status,
#                           steps/rules/cps without the time, trace size,
#                           drop counters, LRS and Otter counters) and the
#                           lane's end report.
# Verdict per problem: PASS when all three agree; FAIL when they differ
# and both runs ended before the wall, or when the selection sequences
# already differ within their common prefix; WALL when a run reached the
# wall and the sequences agree up to where the shorter one stopped (the
# exporting run is slower in wall time, so its wall falls at an earlier
# pick: inconclusive -- replace the problem or raise the wall).  A family
# passes G2.1 when all of its problems PASS (the plan asks for 7).  The
# exporting run's file is also read back (np0_extract.py --stats, when
# python3 with numpy is available): it must be format version 2, carry
# the lane tag, and decode every rule record.
#
# Usage:
#   tests/atp_history_v2_obs.sh <FindProof> <TPTP_root> <list.tsv> <outdir> [wall=180]
#   tests/atp_history_v2_obs.sh --pick <matrix.tsv> <problems.tsv> [per_family=7]
# list.tsv: family TAB lane TAB problem path relative to <TPTP_root>.
# --pick prints such a list from the lane matrix (runner rows, 14 columns,
# docs: ~/.cache/thvm/enigma/matrix/FORMAT.txt) and its problems.tsv: per
# family, the base lane's TRAIN problems proved at 60 s in PICK_MIN to
# PICK_MAX seconds (default 1 to 40), spread over that range.  It refuses a matrix that holds any j13 row (the seal:
# pass the sealed working copy).  FAMILIES="name=lane ..." overrides the
# default family -> base lane map.
# Environment: PAR parallel problems (default nproc / 2), KEEP_HX=1 keeps
# the export files, PYTHON (default python3).
# Pod-only: it runs FindProof.  Writes <outdir>/g21.tsv (one row per
# problem), <outdir>/g21.summary (per family) and <outdir>/g21.done.
set -u
HERE=$(cd "$(dirname "$0")/.." && pwd)
FAMILIES=${FAMILIES:-"twee=twee wm=wm flatten=flatten otter=otter-lpo tgt=tgt-sel narrow=narrowq"}

if [ "${1:-}" = "--pick" ]; then
  MATRIX=$2; PROBS=$3; PER=${4:-7}
  LO=${PICK_MIN:-1}; HI=${PICK_MAX:-40}
  if awk -F'\t' '$3 == "j13" { found = 1 } END { exit !found }' "$MATRIX"; then
    echo "atp_history_v2_obs: $MATRIX holds j13 rows; pass the sealed working copy" >&2
    exit 2
  fi
  for fl in $FAMILIES; do
    fam=${fl%%=*}; lane=${fl#*=}
    awk -F'\t' -v lane="$lane" -v fam="$fam" -v per="$PER" -v lo="$LO" -v hi="$HI" '
      FNR == NR { if ($3 == "train") path[$1] = $2; next }
      $2 == lane && $3 == "train" && $9 == "proved" && ($1 in path) &&
      $6 + 0 >= lo + 0 && $6 + 0 <= hi + 0 { n++; t[n] = $6 + 0; p[n] = $1 }
      END {
        # insertion sort by time (a few hundred rows at most)
        for (i = 2; i <= n; i++) {
          for (j = i; j > 1 && t[j - 1] > t[j]; j--) {
            x = t[j]; t[j] = t[j - 1]; t[j - 1] = x
            y = p[j]; p[j] = p[j - 1]; p[j - 1] = y
          }
        }
        if (n < per) {
          printf "atp_history_v2_obs: %s (%s): only %d candidates\n", fam, lane, n > "/dev/stderr"
        }
        last = 0
        for (k = 0; k < per && k < n; k++) {
          i = (per > 1 && n > 1) ? 1 + int(k * (n - 1) / (per - 1) + 0.5) : 1
          if (i <= last) i = last + 1
          if (i > n) break
          last = i
          printf "%s\t%s\t%s\n", fam, lane, path[p[i]]
        }
      }' "$PROBS" "$MATRIX"
  done
  exit 0
fi

if [ $# -lt 4 ]; then
  sed -n '2,/^set -u/p' "$0" | sed '$d'
  exit 2
fi
BIN=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
ROOT=$(cd "$2" && pwd)
LIST=$3; OUT=$4; WALL=${5:-180}
NPROC=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)
PAR=${PAR:-$(( NPROC / 2 > 0 ? NPROC / 2 : 1 ))}
PY=${PYTHON:-python3}
EXT=$HERE/tools/atp_enigma/np0_extract.py
mkdir -p "$OUT/runs"
: > "$OUT/g21.tsv"
export BIN ROOT OUT WALL PY EXT
export KEEP_HX=${KEEP_HX:-0}

run_one() {
  fam=$(printf '%s' "$1" | cut -f1)
  lane=$(printf '%s' "$1" | cut -f2)
  rel=$(printf '%s' "$1" | cut -f3)
  nm=$(basename "$rel" .p)
  d=$OUT/runs/$fam/$nm
  mkdir -p "$d"
  for mode in off on; do
    rm -rf "$d/$mode.clk" "$d/hx.bin"
    mkdir -p "$d/$mode.clk"
    ex=()
    if [ "$mode" = on ]; then
      ex=(THVM_ATP_HISTORY_EXPORT="$d/hx.bin" THVM_ATP_HISTORY_RATE=1)
    fi
    (cd "$d" && env -i PATH="$PATH" HOME="${HOME:-/tmp}" TPTP="$ROOT" \
       THVM_ATP_WORK_CLOCK=instr THVM_ATP_CP_PICK_TRACE=1 \
       THVM_ATP_CLOCK_TRACE="$d/$mode.clk" ${ex[@]+"${ex[@]}"} \
       timeout -k 5 $((WALL + 30)) "$BIN" --lane="$lane" --szs --proof \
       --wall="$WALL" "$ROOT/$rel" </dev/null >"$d/$mode.out" 2>"$d/$mode.err")
    grep -aE '^CPSEL|^CPSKIP' "$d/$mode.err" > "$d/$mode.sel"
    {
      grep -a -m1 '^=> ' "$d/$mode.err"
      grep -a -m1 '^   goal=' "$d/$mode.err" | sed -E 's/goal=[^ ]* +//; s/ +[0-9.]+s$//'
      grep -aE '^   (trace: |dropped: |lrs: |otter: |pop-gj: dropped|drc-off: |drc-needle: |spb-goal: |right-reduced|lazy-normalize=|unorientable rules:)' "$d/$mode.err"
      grep -a -m1 "^% FindProof lane $lane: " "$d/$mode.err"
    } > "$d/$mode.stats"
    if grep -aqE '^=> RUNNING \(wall cap\)|^=> TIMEOUT|stopped at the wall' "$d/$mode.err" ||
       grep -aq 'SZS status Timeout' "$d/$mode.out"; then
      echo wall > "$d/$mode.end"
    else
      echo done > "$d/$mode.end"
    fi
    cat "$d"/$mode.clk/clock.*.tsv 2>/dev/null | grep -v '^#' |
      sed -E 's/ (wall|cpu|work|el|spent|cost|rem|pred)=[0-9]+//g' > "$d/$mode.clock"
    rm -f "$d/$mode.err"
  done
  szs_off=$(grep -a -m1 -o 'SZS status [A-Za-z]*' "$d/off.out" | awk '{print $3}')
  szs_on=$(grep -a -m1 -o 'SZS status [A-Za-z]*' "$d/on.out" | awk '{print $3}')
  picks_off=$(grep -c '^CPSEL' "$d/off.sel")
  picks_on=$(grep -c '^CPSEL' "$d/on.sel")
  sel=same; prf=same; st=same
  cmp -s "$d/off.sel" "$d/on.sel" || sel=DIFF
  cmp -s "$d/off.out" "$d/on.out" || prf=DIFF
  cmp -s "$d/off.stats" "$d/on.stats" || st=DIFF
  # First differing line within the common prefix (the last line of a
  # killed run may be cut short, so it is left out).
  n_off=$(wc -l < "$d/off.sel"); n_on=$(wc -l < "$d/on.sel")
  n=$(( n_off < n_on ? n_off : n_on ))
  [ "$n" -gt 0 ] && n=$((n - 1))
  first_div=$(cmp <(head -n "$n" "$d/off.sel") <(head -n "$n" "$d/on.sel") 2>/dev/null |
              sed -nE 's/.* line ([0-9]+).*/\1/p')
  [ -z "$first_div" ] && first_div=-
  clock_div=$(diff "$d/off.clock" "$d/on.clock" 2>/dev/null | grep -m1 '^[<>]' |
              cut -c1-160 | tr '\t' ' ')
  [ -z "$clock_div" ] && clock_div=-
  end_off=$(cat "$d/off.end"); end_on=$(cat "$d/on.end")
  if [ "$sel" = same ] && [ "$prf" = same ] && [ "$st" = same ]; then
    verdict=PASS
  elif [ "$first_div" != - ]; then
    verdict=FAIL
  elif [ "$end_off" = wall ] || [ "$end_on" = wall ]; then
    verdict=WALL
  else
    verdict=FAIL
  fi
  hx_bytes=$(wc -c < "$d/hx.bin" 2>/dev/null | tr -d ' ')
  [ -z "$hx_bytes" ] && hx_bytes=0
  export_check=-
  if [ -s "$d/hx.bin" ] && "$PY" -c 'import numpy' 2>/dev/null; then
    "$PY" "$EXT" --stats "$d/hx.bin" > "$d/hx.stats" 2>&1
    if grep -q "^session@[0-9]*: v2 lane=$lane " "$d/hx.stats" &&
       ! grep -q 'rule_decode_errors=[1-9]' "$d/hx.stats" &&
       ! grep -q 'Traceback' "$d/hx.stats"; then
      export_check=ok
    else
      export_check=BAD
    fi
  fi
  [ "$KEEP_HX" = 1 ] || rm -f "$d/hx.bin"
  printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\tselection=%s\tproof=%s\tstats=%s\tend=%s/%s\thx=%s\texport=%s\tclock=%s\n' \
    "$fam" "$lane" "$nm" "$verdict" "${szs_off:-none}" "${szs_on:-none}" \
    "$picks_off" "$picks_on" "$first_div" "$sel" "$prf" "$st" \
    "$end_off" "$end_on" "$hx_bytes" "$export_check" "$clock_div" >> "$OUT/g21.tsv"
}
export -f run_one

grep -v '^#' "$LIST" | awk -F'\t' 'NF >= 3' | tr '\n' '\0' |
  xargs -0 -P "$PAR" -I{} bash -c 'run_one "$@"' _ {}

{
  printf 'family\tlane\tproblems\tpass\tfail\twall\texport_bad\tG2.1\n'
  awk -F'\t' '
    { n[$1]++; lane[$1] = $2; v[$1, $4]++; if ($15 == "export=BAD") bad[$1]++ }
    END {
      for (f in n) {
        g = (v[f, "PASS"] == n[f] && bad[f] == 0) ? "PASS" :
            ((v[f, "FAIL"] > 0 || bad[f] > 0) ? "FAIL" : "INCONCLUSIVE")
        printf "%s\t%s\t%d\t%d\t%d\t%d\t%d\t%s\n", f, lane[f], n[f], v[f, "PASS"] + 0,
               v[f, "FAIL"] + 0, v[f, "WALL"] + 0, bad[f] + 0, g
      }
    }' "$OUT/g21.tsv" | sort
} > "$OUT/g21.summary"
cat "$OUT/g21.summary"
echo done > "$OUT/g21.done"
if awk -F'\t' 'NR > 1 && $8 != "PASS" { bad = 1 } END { exit !bad }' "$OUT/g21.summary"; then
  echo "G2.1 NOT PASSED"
  exit 1
fi
echo "G2.1 PASSED"
