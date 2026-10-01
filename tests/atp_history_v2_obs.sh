#!/bin/bash
# Gate G2.1 (docs/plans/enigma_portfolio.md, M2): history export v2 only
# observes.  Every listed (family, lane, problem) runs through the harvest
# path -- FindProof --lane=NAME --szs --proof -- under the gate's logical
# work clock (THVM_ATP_WORK_CLOCK=steps: the engine's clock-gated
# disciplines read a count of CP inserts and selections, which the
# export's hooks cannot advance) three times: export off, export on
# (THVM_ATP_HISTORY_EXPORT, rate 1), export off again.  The two off runs
# are the A/A control: a run that does not repeat itself cannot show what
# the export changes (AA=0 skips the second one).  All three runs carry
# the same environment variable names -- the off runs set
# THVM_ATP_HISTORY_EXPORT empty, which leaves the export unarmed -- so an
# uncached getenv costs the same in each.  Compared:
#   the selection sequence  every CPSEL / CPSKIP line of the per-pick trace
#                           (THVM_ATP_CP_PICK_TRACE: pick, heap position,
#                           seq, priority, rules, both sides);
#   the proof               stdout: the SZS status line and the proof;
#   the stats               the engine's deterministic stats lines (status,
#                           steps/rules/cps without the time, trace size,
#                           drop counters, LRS and Otter counters) and the
#                           lane's end report.
# Verdict per problem:
#   FAIL        the exporting run died of a signal the plain run did not,
#               or its file does not read back (np0_extract.py --stats:
#               version 2, the lane tag, every rule decoded, every session
#               closed with its 'E' record, a proved run's session with
#               status PROVED and its 'R' dump); or on differs from off
#               while the off runs agree and neither reached the wall --
#               including a clock-gated decision (THVM_ATP_CLOCK_TRACE)
#               taken at another point: under the step clock that is a
#               change of the search, not drift;
#   NONDET      the two off runs differ: the run does not repeat, so it
#               cannot test the export (the summary takes the next row);
#   PASS        export on agrees with off on all three, and the export saw
#               the search (at least one 'S' record);
#   UNOBSERVED  on agrees with off, but the export holds no selection: the
#               lane searched outside the engine (horn-cd's condensed
#               detachment, a front-end proof), so the row tests nothing;
#   WALL        a run reached the wall and the selections agree as far as
#               both went: inconclusive.
# Each row also names the first diverging pick and the first clock-gated
# decision that differs (and its selection index), and the work-clock
# difference, export on minus off, at the last decision both runs took
# alike (0 under the step clock).
# A family passes G2.1 when it has no FAIL row and its first MIN_PASS
# (default 7, the plan's count) conclusive rows in list order are PASS;
# a conclusive row is PASS, FAIL or UNOBSERVED.  Fewer than MIN_PASS
# conclusive rows leave it INCONCLUSIVE; an UNOBSERVED row among them
# makes it UNOBSERVED (the export cannot see that family's search).  The
# reader's self-test (np0_extract.py --selftest: synthetic version-1 and
# version-2 files) runs once first when python3 has numpy.
#
# Usage:
#   tests/atp_history_v2_obs.sh <FindProof> <TPTP_root> <list.tsv> <outdir> [wall=180]
#   tests/atp_history_v2_obs.sh --pick <matrix.tsv> <problems.tsv> [per_family=10]
# list.tsv: family TAB lane TAB problem path relative to <TPTP_root>.
# --pick prints such a list from the lane matrix (runner rows, 14 columns,
# docs: ~/.cache/thvm/enigma/matrix/FORMAT.txt) and its problems.tsv: per
# family, the base lane's TRAIN problems the engine proved at 60 s (class
# proved, detail PROVED: not a front-end proof, which the export cannot
# see) in PICK_MIN to PICK_MAX seconds (default 1 to 40), spread over that
# range, topped up with the slowest faster ones when the range holds too
# few (narrowq and horn-cd prove mostly under a second).  It lists 10 per
# family by default, so NONDET and WALL rows have spares.  It refuses a
# matrix that holds any j13 row (the seal: pass the sealed working copy).
# FAMILIES="name=lane ..." overrides the default family -> base lane map:
# the six lane families of the M2 hook audit and the plan's provisional
# twin lanes (section 5.4).
# Environment: PAR parallel problems (default nproc / 2), AA (default 1),
# MIN_PASS (default 7), KEEP_HX=1 keeps the export files, PYTHON (default
# python3).
# Pod-only: it runs FindProof.  Writes <outdir>/g21.tsv (one row per
# problem, its list index last), <outdir>/g21.summary (per family) and
# <outdir>/g21.done.
set -u
HERE=$(cd "$(dirname "$0")/.." && pwd)
FAMILIES=${FAMILIES:-"twee=twee wm=wm flatten=flatten otter=otter-lpo tgt=tgt-sel narrow=narrowq encompass=encompass horn-cd=horn-cd flatten-bare-raw=flatten-bare-raw"}

if [ "${1:-}" = "--pick" ]; then
  MATRIX=$2; PROBS=$3; PER=${4:-10}
  LO=${PICK_MIN:-1}; HI=${PICK_MAX:-40}
  if awk -F'\t' '$3 == "j13" { found = 1 } END { exit !found }' "$MATRIX"; then
    echo "atp_history_v2_obs: $MATRIX holds j13 rows; pass the sealed working copy" >&2
    exit 2
  fi
  for fl in $FAMILIES; do
    fam=${fl%%=*}; lane=${fl#*=}
    awk -F'\t' -v lane="$lane" -v fam="$fam" -v per="$PER" -v lo="$LO" -v hi="$HI" '
      FNR == NR { if ($3 == "train") path[$1] = $2; next }
      $2 == lane && $3 == "train" && $9 == "proved" && $14 == "PROVED" &&
      ($1 in path) && $6 + 0 <= hi + 0 {
        if ($6 + 0 >= lo + 0) { n++; t[n] = $6 + 0; p[n] = $1 }
        else { m++; u[m] = $6 + 0; q[m] = $1 }
      }
      END {
        # insertion sorts (a few hundred rows at most): in range by time,
        # the faster ones slowest first
        for (i = 2; i <= n; i++) {
          for (j = i; j > 1 && t[j - 1] > t[j]; j--) {
            x = t[j]; t[j] = t[j - 1]; t[j - 1] = x
            y = p[j]; p[j] = p[j - 1]; p[j - 1] = y
          }
        }
        for (i = 2; i <= m; i++) {
          for (j = i; j > 1 && u[j - 1] < u[j]; j--) {
            x = u[j]; u[j] = u[j - 1]; u[j - 1] = x
            y = q[j]; q[j] = q[j - 1]; q[j - 1] = y
          }
        }
        got = 0; last = 0
        for (k = 0; k < per && k < n; k++) {
          i = (per > 1 && n > 1) ? 1 + int(k * (n - 1) / (per - 1) + 0.5) : 1
          if (i <= last) i = last + 1
          if (i > n) break
          last = i
          printf "%s\t%s\t%s\n", fam, lane, path[p[i]]
          got++
        }
        # Too few in range: the slowest of the faster problems fill up.
        for (k = 1; got < per && k <= m; k++) {
          printf "%s\t%s\t%s\n", fam, lane, path[q[k]]
          got++
        }
        if (got < per) {
          printf "atp_history_v2_obs: %s (%s): only %d candidates\n", fam, lane, got > "/dev/stderr"
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
export AA=${AA:-1}
MIN_PASS=${MIN_PASS:-7}

# The step clock drives the engine's disciplines on any host; the
# front-end budgets read the instruction counter, which needs
# perf_event_open (without it they fall back to the CPU clock and front-end
# lanes may stop repeating: NONDET rows).
paranoid=$(cat /proc/sys/kernel/perf_event_paranoid 2>/dev/null || echo 9)
if [ "$(id -u)" != 0 ] && [ "$paranoid" -gt 2 ]; then
  echo "atp_history_v2_obs: kernel.perf_event_paranoid=$paranoid and not root:" \
       "the front-end budgets fall back to the CPU clock" >&2
fi

HAVE_NUMPY=0
"$PY" -c 'import numpy' 2>/dev/null && HAVE_NUMPY=1
export HAVE_NUMPY
selftest=skipped
if [ "$HAVE_NUMPY" = 1 ]; then
  if "$PY" "$EXT" --selftest > "$OUT/selftest.txt" 2>&1 &&
     grep -q '^selftest ok$' "$OUT/selftest.txt"; then
    selftest=ok
  else
    selftest=FAILED
  fi
fi

# The selection index a clock-trace line was written at (its sel= field).
clock_sel() {
  printf '%s\n' "$1" | sed -nE 's/^[a-z-]+ sel=([0-9]+).*/\1/p' | head -1
}
# The pick a per-pick trace line belongs to: CPSEL pick=N is selection N;
# CPSKIP pick=N discards an orphan while selection N + 1 is being made.
line_pick() {
  printf '%s\n' "$1" | awk '
    /^CPSEL / { if (match($0, /pick=[0-9]+/)) print substr($0, RSTART + 5, RLENGTH - 5) + 0 }
    /^CPSKIP / { if (match($0, /pick=[0-9]+/)) print substr($0, RSTART + 5, RLENGTH - 5) + 1 }'
}
export -f clock_sel line_pick

run_one() {
  idx=$(printf '%s' "$1" | cut -f1)
  fam=$(printf '%s' "$1" | cut -f2)
  lane=$(printf '%s' "$1" | cut -f3)
  rel=$(printf '%s' "$1" | cut -f4)
  nm=$(basename "$rel" .p)
  d=$OUT/runs/$fam/$nm
  mkdir -p "$d"
  modes="off on"
  [ "$AA" = 1 ] && modes="off on off2"
  for mode in $modes; do
    rm -rf "$d/$mode.clk"
    mkdir -p "$d/$mode.clk"
    hx=
    if [ "$mode" = on ]; then
      hx=$d/hx.bin
      rm -f "$hx"
    fi
    (cd "$d" && env -i PATH="$PATH" HOME="${HOME:-/tmp}" TPTP="$ROOT" \
       THVM_ATP_WORK_CLOCK=steps THVM_ATP_CP_PICK_TRACE=1 \
       THVM_ATP_CLOCK_TRACE="$d/$mode.clk" \
       THVM_ATP_HISTORY_EXPORT="$hx" THVM_ATP_HISTORY_RATE=1 \
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
    cat "$d"/$mode.clk/clock.*.tsv 2>/dev/null | grep -v '^#' > "$d/$mode.clockraw"
    sed -E 's/ (wall|cpu|work|el|spent|cost|rem|pred)=[0-9]+//g' "$d/$mode.clockraw" \
      > "$d/$mode.clock"
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
  aa=-
  if [ "$AA" = 1 ]; then
    aa=same
    if ! cmp -s "$d/off.sel" "$d/off2.sel" || ! cmp -s "$d/off.out" "$d/off2.out" ||
       ! cmp -s "$d/off.stats" "$d/off2.stats"; then
      aa=DIFF
    fi
  fi
  # First differing selection line within the common prefix (the last
  # line of a killed run may be cut short, so it is left out), and the
  # pick it belongs to (the earlier of the two runs' lines).
  n_off=$(wc -l < "$d/off.sel"); n_on=$(wc -l < "$d/on.sel")
  n=$(( n_off < n_on ? n_off : n_on ))
  [ "$n" -gt 0 ] && n=$((n - 1))
  first_div=-
  div_line=$(cmp <(head -n "$n" "$d/off.sel") <(head -n "$n" "$d/on.sel") 2>/dev/null |
             sed -nE 's/.* differ: .* line ([0-9]+).*/\1/p')
  if [ -n "$div_line" ]; then
    pa=$(line_pick "$(sed -n "${div_line}p" "$d/off.sel")")
    pb=$(line_pick "$(sed -n "${div_line}p" "$d/on.sel")")
    first_div=${pa:-0}
    [ -n "$pb" ] && [ "$pb" -lt "$first_div" ] && first_div=$pb
  fi
  # First clock-gated decision (times stripped) the two runs took
  # differently, and the selection index of the earlier one.  A decision
  # only one run took counts when the other run went past its selection
  # index without taking it; past the other run's end it shows nothing.
  clock_div=-; clock_div_sel=-
  cl_line=$(cmp "$d/off.clock" "$d/on.clock" 2>/dev/null |
            sed -nE 's/.* differ: .* line ([0-9]+).*/\1/p')
  if [ -n "$cl_line" ]; then
    lo=$(sed -n "${cl_line}p" "$d/off.clock")
    ln=$(sed -n "${cl_line}p" "$d/on.clock")
    clock_div=$(printf '< %s | > %s' "$lo" "$ln" | cut -c1-200 | tr '\t' ' ')
    sa=$(clock_sel "$lo"); sb=$(clock_sel "$ln")
    if [ -n "$sa" ] && [ -n "$sb" ]; then
      clock_div_sel=$(( sa < sb ? sa : sb ))
    else
      clock_div_sel=${sa:-${sb:--}}
    fi
  elif ! cmp -s "$d/off.clock" "$d/on.clock"; then
    l_off=$(wc -l < "$d/off.clock"); l_on=$(wc -l < "$d/on.clock")
    if [ "$l_off" -lt "$l_on" ]; then
      extra=$(sed -n "$((l_off + 1))p" "$d/on.clock"); passed=$picks_off; side='>'
    else
      extra=$(sed -n "$((l_on + 1))p" "$d/off.clock"); passed=$picks_on; side='<'
    fi
    se=$(clock_sel "$extra")
    if [ -n "$se" ] && [ "$se" -lt "$passed" ]; then
      clock_div_sel=$se
      clock_div=$(printf '%s %s (only this run)' "$side" "$extra" | cut -c1-200 | tr '\t' ' ')
    fi
  fi
  # Work-clock difference (export on minus off, nominal microseconds) at
  # the last clock-gated decision both runs took alike: 0 under the step
  # clock, which the export cannot advance.
  drift=$(awk '
    FNR == NR { off[FNR] = $0; n = FNR; next }
    FNR <= n {
      a = off[FNR]; b = $0
      sa = a; sb = b
      gsub(/ (wall|cpu|work|el|spent|cost|rem|pred)=[0-9]+/, "", sa)
      gsub(/ (wall|cpu|work|el|spent|cost|rem|pred)=[0-9]+/, "", sb)
      if (sa != sb) exit
      wa = a; sub(/.* work=/, "", wa); sub(/ .*/, "", wa)
      wb = b; sub(/.* work=/, "", wb); sub(/ .*/, "", wb)
      sel = a; sub(/.* sel=/, "", sel); sub(/ .*/, "", sel)
      last = (wb - wa) "us@sel" sel
    }
    END { print (last == "" ? "-" : last) }' "$d/off.clockraw" "$d/on.clockraw" 2>/dev/null)
  [ -z "$drift" ] && drift=-
  end_off=$(cat "$d/off.end"); end_on=$(cat "$d/on.end")
  hx_bytes=$(wc -c < "$d/hx.bin" 2>/dev/null | tr -d ' ')
  [ -z "$hx_bytes" ] && hx_bytes=0
  # The export reads back: every session version 2 with the lane tag,
  # every rule decoded, and -- for a run that ended on its own -- every
  # session closed ('E'); a proved run has a PROVED session with its 'R'
  # dump.  none = no session or no selection: the export saw no search.
  export_check=-
  if [ ! -s "$d/hx.bin" ]; then
    export_check=none
  elif [ "$HAVE_NUMPY" = 1 ]; then
    "$PY" "$EXT" --stats "$d/hx.bin" > "$d/hx.stats" 2>&1
    export_check=ok
    if grep -q 'Traceback' "$d/hx.stats" ||
       ! grep -q '^session@' "$d/hx.stats" ||
       grep '^session@' "$d/hx.stats" | grep -qv "^session@[0-9]*: v2 lane=$lane " ||
       grep -q 'rule_decode_errors=[1-9]' "$d/hx.stats"; then
      export_check=BAD
    elif [ "$end_on" = done ] && grep -q ' closed=0 ' "$d/hx.stats"; then
      export_check=BAD
    elif { [ "$szs_on" = Unsatisfiable ] || [ "$szs_on" = Theorem ]; } &&
         ! grep -qE ' status=1 closed=1 .* n_r=[1-9]' "$d/hx.stats" &&
         awk '/^session@/ && / n_sel=[1-9]/ { f = 1 } END { exit !f }' "$d/hx.stats"; then
      # a proved run whose engine searched must close a PROVED session
      export_check=BAD
    elif ! grep -qE ' n_sel=[1-9]' "$d/hx.stats"; then
      export_check=none
    fi
  fi
  [ "$KEEP_HX" = 1 ] || rm -f "$d/hx.bin"
  # The export run died of a signal where the plain run did not.
  on_crash=0
  if grep -aq "^% FindProof lane $lane: signal" "$d/on.stats" &&
     ! grep -aq "^% FindProof lane $lane: signal" "$d/off.stats"; then
    on_crash=1
  fi
  if [ "$export_check" = BAD ] || [ "$on_crash" = 1 ]; then
    verdict=FAIL
  elif [ "$aa" = DIFF ]; then
    verdict=NONDET
  elif [ "$sel" = same ] && [ "$prf" = same ] && [ "$st" = same ]; then
    if [ "$export_check" = none ]; then
      verdict=UNOBSERVED
    else
      verdict=PASS
    fi
  elif [ "$first_div" = - ] && { [ "$end_off" = wall ] || [ "$end_on" = wall ]; }; then
    verdict=WALL
  else
    verdict=FAIL
  fi
  printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\tselection=%s\tproof=%s\tstats=%s\taa=%s\tend=%s/%s\thx=%s\texport=%s\tdrift=%s\tclock_sel=%s\tclock=%s\t%s\n' \
    "$fam" "$lane" "$nm" "$verdict" "${szs_off:-none}" "${szs_on:-none}" \
    "$picks_off" "$picks_on" "$first_div" "$sel" "$prf" "$st" "$aa" \
    "$end_off" "$end_on" "$hx_bytes" "$export_check" "$drift" \
    "$clock_div_sel" "$clock_div" "$idx" >> "$OUT/g21.tsv"
}
export -f run_one

grep -v '^#' "$LIST" | awk -F'\t' 'NF >= 3 { printf "%d\t%s\n", ++n, $0 }' | tr '\n' '\0' |
  xargs -0 -P "$PAR" -I{} bash -c 'run_one "$@"' _ {}

{
  printf 'family\tlane\tproblems\tpass\tfail\tunobserved\tnondet\twall\tjudged\tG2.1\n'
  # Rows in list order (the last column); the first MIN_PASS conclusive
  # rows of a family decide it, and any FAIL fails it.
  sort -t$'\t' -k20,20n "$OUT/g21.tsv" | awk -F'\t' -v need="$MIN_PASS" '
    { n[$1]++; lane[$1] = $2; v[$1, $4]++
      if ($4 == "PASS" || $4 == "FAIL" || $4 == "UNOBSERVED") {
        if (c[$1] < need) { c[$1]++; j[$1, $4]++ }
      }
    }
    END {
      for (f in n) {
        g = (v[f, "FAIL"] > 0) ? "FAIL" :
            (j[f, "UNOBSERVED"] > 0) ? "UNOBSERVED" :
            (j[f, "PASS"] >= need) ? "PASS" : "INCONCLUSIVE"
        printf "%s\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%s\n", f, lane[f], n[f],
               v[f, "PASS"] + 0, v[f, "FAIL"] + 0, v[f, "UNOBSERVED"] + 0,
               v[f, "NONDET"] + 0, v[f, "WALL"] + 0, c[f] + 0, g
      }
    }' | sort
  printf 'reader selftest\t%s\n' "$selftest"
} > "$OUT/g21.summary"
cat "$OUT/g21.summary"
echo done > "$OUT/g21.done"
if [ "$selftest" = FAILED ] ||
   awk -F'\t' 'NR > 1 && $1 != "reader selftest" && $10 != "PASS" { bad = 1 } END { exit !bad }' "$OUT/g21.summary"; then
  echo "G2.1 NOT PASSED"
  exit 1
fi
echo "G2.1 PASSED"
