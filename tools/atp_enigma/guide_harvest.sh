#!/bin/bash
# A guided harvest (docs/ATP/enigma_portfolio.md, section 3.2 and M3): one
# FindProof twin run per (problem, lane) job, the label-harvest runner's run
# (--lane=NAME --szs --proof --wall=WALL, history export v2 at rate 1,
# --work-clock=instr on the clock-gated lanes of gated.txt) with the
# selection layer armed:
#   THVM_ATP_GUIDE=<lane> THVM_ATP_GUIDE_MODEL=$MODEL [THVM_ATP_GUIDE_ZERO=1]
#   THVM_ATP_GUIDE_DUMP=<raw>/<key>.dump THVM_ATP_GUIDE_DUMP_EVERY=<the job's>
#   THVM_ATP_GUIDE_DUMP_MAX=$DUMP_MAX [THVM_ATP_GUIDE_DUMP_ROWS=$DUMP_ROWS]
# Loop 0 runs the zero twins of the harvest head (ZERO=1, MODEL=harvest:
# every phi(p) column, top-64), so each run is its base lane's search, and
# dumps the engine's 64 best candidates at a hash-sampled subset of its
# picks.  Then np0_extract.py turns the export plus the emitted proof into
# npz/<problem>__<lane>.npz (proof mode; a proved run whose output has no
# proof block, which FindProof withheld, and an unproved run in hindsight
# mode), the dump is gzipped into dump/<key>.dump.gz and the raw files are
# deleted.  phi.tsv holds FindProof --problem-features --header rows of
# the jobs' problems, computed first.  label_join.py reads this directory.
#
# Usage: bash guide_harvest.sh <jobs.tsv> <outdir> <FindProof> <tptp_root> <gated.txt> <np0_extract.py>
# jobs.tsv: name TAB path TAB split TAB lane TAB dump_every (guide_jobs.py).
# <outdir>/index.tsv, one row per job, no header: the label harvest's 19
# columns (problem, lane, proved, rows, positives, npz_bytes, split, szs,
# exit, wall_s, cpu_s, maxrss_kb, clock, steps, rules, cps, hx_bytes, mode,
# extract), then 20 dump_every, 21 dump_gz_bytes, 22 the twin's "% guide:"
# fields (picks, scored, frac_wall, us_per_scored; '-' without the line).
# mode: proof / hindsight as the extractor reports, frontend (proved with no
# engine selection), none.  Resumable (a job with an index row is skipped).
# Sentinels <outdir>/index.tsv.started / .done / .failed.
# Env: PAR (nproc), WALL (60), TMO (120 per run), XTMO (900 per extraction),
# DEADLINE_S (14400: no job starts after it), MAXNEG (100000), MODEL
# (harvest), ZERO (1), DUMP_MAX (2000 picks), DUMP_ROWS (0: no row cap).
set -u
JOBS_F=$1; OUT=$2; BIN=$3; ROOT=$4; GATED=$5; EXT=$6
WALL=${WALL:-60}; TMO=${TMO:-120}; XTMO=${XTMO:-900}; DEADLINE_S=${DEADLINE_S:-14400}
MAXNEG=${MAXNEG:-100000}; PAR=${PAR:-$(nproc)}; MODEL=${MODEL:-harvest}; ZERO=${ZERO:-1}
DUMP_MAX=${DUMP_MAX:-2000}; DUMP_ROWS=${DUMP_ROWS:-0}
mkdir -p "$OUT/npz" "$OUT/out" "$OUT/err" "$OUT/raw" "$OUT/dump"
OUT=$(cd "$OUT" && pwd); BIN=$(cd "$(dirname "$BIN")" && pwd)/$(basename "$BIN")
ROOT=$(cd "$ROOT" && pwd); IDX=$OUT/index.tsv
rm -f "$IDX.done" "$IDX.failed"
fail() { echo "failed=$(date -u +%FT%TZ) reason=$*" > "$IDX.failed"; exit 2; }
[ -x "$BIN" ] || fail "no binary"
[ -x /usr/bin/time ] || fail "no GNU time"
python3 -c 'import numpy' 2>/dev/null || fail "no numpy"
touch "$IDX"
WORK=$(mktemp -d /tmp/guide_harvest.XXXXXX)
awk -F'\t' 'FILENAME == ARGV[1] { d[$1 "\t" $2] = 1; next } !(($1 "\t" $4) in d)' "$IDX" "$JOBS_F" > "$WORK/pending"
mapfile -t JOBS < "$WORK/pending"
NJOBS=${#JOBS[@]}; NALL=$(wc -l < "$JOBS_F")
START=$(date +%s); DEADLINE=$((START + DEADLINE_S))
{
  echo "started=$(date -u +%FT%TZ)"
  echo "host=$(hostname) nproc=$(nproc) mem_gb=$(awk '/^MemTotal:/ {print int($2/1048576)}' /proc/meminfo) cpu=$(awk -F': ' '/^model name/ {print $2; exit}' /proc/cpuinfo)"
  echo "par=$PAR wall=$WALL timeout=$TMO xtimeout=$XTMO deadline_s=$DEADLINE_S maxneg=$MAXNEG"
  echo "model=$MODEL zero=$ZERO dump_max=$DUMP_MAX dump_rows=$DUMP_ROWS"
  echo "bin=$BIN sha256=$(sha256sum "$BIN" | cut -c1-64)"
  echo "runner_sha256=$(sha256sum "$0" | cut -c1-64) extract_sha256=$(sha256sum "$EXT" | cut -c1-64)"
  echo "jobs=$NALL pending=$NJOBS gated_lanes=$(wc -l < "$GATED")"
} > "$IDX.started"
# phi(p) of every problem, once: label_join checks the dumps against it
# and guide_net reads the Rating from it.
if [ ! -s "$OUT/phi.tsv" ]; then
  "$BIN" --problem-features --header > "$WORK/phi.tsv" 2>/dev/null
  cut -f1,2 "$JOBS_F" | sort -u | while IFS=$'\t' read -r name path; do
    env -i PATH=/usr/local/bin:/usr/bin:/bin HOME="$HOME" TPTP="$ROOT" \
      timeout -k 5 60 "$BIN" --problem-features "$ROOT/$path" < /dev/null 2>> "$WORK/phi.err"
  done >> "$WORK/phi.tsv"
  mv "$WORK/phi.tsv" "$OUT/phi.tsv"
  cp "$WORK/phi.err" "$OUT/phi.err" 2>/dev/null
fi
echo 0 > "$WORK/ctr"
claim() {
  local n
  { flock 9; n=$(cat "$WORK/ctr"); echo $((n + 1)) > "$WORK/ctr"; } 9> "$WORK/ctr.lock"
  echo "$n"
}
run_job() {
  local dir=$1 idx=$2 line=$3 name path split lane every key hx dp clock cflag rc szs tl wall cpu rss
  local stats steps rules cps hxb proved mode ex rows pos nb proof zenv denv gl gf db
  IFS=$'\t' read -r name path split lane every <<< "$line"
  every=${every:-1}
  key=${name}__${lane}; hx=$OUT/raw/$key.hx; dp=$OUT/raw/$key.dump
  rm -f "$hx" "$dp" "$dir/time"
  clock=cpu; cflag=
  if grep -qxF "$lane" "$GATED"; then clock=instr; cflag=--work-clock=instr; fi
  zenv=; [ "$ZERO" = 1 ] && zenv=THVM_ATP_GUIDE_ZERO=1
  denv=; [ "$DUMP_ROWS" != 0 ] && denv=THVM_ATP_GUIDE_DUMP_ROWS=$DUMP_ROWS
  (cd "$dir" && env -i PATH=/usr/local/bin:/usr/bin:/bin HOME="$HOME" TPTP="$ROOT" \
     THVM_ATP_HISTORY_EXPORT="$hx" THVM_ATP_HISTORY_RATE=1 \
     THVM_ATP_GUIDE="$lane" THVM_ATP_GUIDE_MODEL="$MODEL" $zenv \
     THVM_ATP_GUIDE_DUMP="$dp" THVM_ATP_GUIDE_DUMP_EVERY="$every" THVM_ATP_GUIDE_DUMP_MAX="$DUMP_MAX" $denv \
     timeout -k 5 "$TMO" /usr/bin/time -o "$dir/time" -f '%e %U %S %M' \
     "$BIN" --lane="$lane" --szs --proof --wall="$WALL" $cflag "$ROOT/$path" \
     < /dev/null > "$OUT/out/$key.out" 2> "$dir/err")
  rc=$?
  tl=$(tail -n 1 "$dir/time" 2>/dev/null)
  if [[ "$tl" =~ ^([0-9.]+)\ ([0-9.]+)\ ([0-9.]+)\ ([0-9]+)$ ]]; then
    wall=${BASH_REMATCH[1]}; rss=${BASH_REMATCH[4]}
    cpu=$(awk -v u="${BASH_REMATCH[2]}" -v s="${BASH_REMATCH[3]}" 'BEGIN { printf "%.2f", u + s }')
  else
    wall=; cpu=; rss=
  fi
  szs=$(sed -n 's/^% SZS status \([A-Za-z]*\).*/\1/p' "$OUT/out/$key.out" | head -n 1)
  stats=$(grep -a -m1 '^   goal=' "$dir/err"); steps=; rules=; cps=
  if [[ "$stats" =~ steps=([0-9]+)\ +rules=([0-9]+)\ +cps=([0-9]+) ]]; then
    steps=${BASH_REMATCH[1]}; rules=${BASH_REMATCH[2]}; cps=${BASH_REMATCH[3]}
  fi
  gl=$(grep -a -m1 '^% guide: ' "$dir/err")
  gf=-
  if [ -n "$gl" ]; then
    gf=$(echo "$gl" | grep -oE ' (picks|scored|frac_wall|us_per_scored)=[^ ]+' | tr -d ' ' | paste -sd, -)
  fi
  tail -c 20000 "$dir/err" > "$OUT/err/$key.err"
  hxb=$(stat -c %s "$hx" 2>/dev/null || echo 0)
  proved=0; proof=-
  case "$szs" in Unsatisfiable|Theorem) proved=1; proof=$OUT/out/$key.out ;; esac
  mode=none; ex=-; rows=0; pos=0; nb=0
  if [ "$hxb" -gt 0 ]; then
    ex=$(nice -n 5 timeout -k 5 "$XTMO" python3 "$EXT" "$hx" "$proof" "$OUT/npz/$key.npz" "$MAXNEG" 2>&1 | tail -1 | tr '\t\n' '  ')
    if [ "$proved" = 1 ] && [[ "$ex" == "no proof block"* ]]; then
      # FindProof withheld the proof: the run's own hindsight labels.
      ex=$(nice -n 5 timeout -k 5 "$XTMO" python3 "$EXT" "$hx" - "$OUT/npz/$key.npz" "$MAXNEG" 2>&1 | tail -1 | tr '\t\n' '  ')
    fi
  fi
  if [[ "$ex" =~ ^ok\  ]]; then
    mode=$(sed -nE 's/.* mode=([a-z]+) .*/\1/p' <<< "$ex")
    rows=$(sed -nE 's/.* kept=([0-9]+) .*/\1/p' <<< "$ex")
    pos=$(sed -nE 's/.* pos=([0-9]+) .*/\1/p' <<< "$ex")
    if [ "$(sed -nE 's/.* sel=([0-9]+) .*/\1/p' <<< "$ex")" = 0 ]; then mode=frontend; fi
    nb=$(stat -c %s "$OUT/npz/$key.npz" 2>/dev/null || echo 0)
  else
    rm -f "$OUT/npz/$key.npz"
    if [ "$proved" = 1 ]; then mode=frontend; fi
  fi
  db=0
  if [ -s "$dp" ]; then
    gzip -c "$dp" > "$OUT/dump/$key.dump.gz.tmp" && mv "$OUT/dump/$key.dump.gz.tmp" "$OUT/dump/$key.dump.gz"
    db=$(stat -c %s "$OUT/dump/$key.dump.gz" 2>/dev/null || echo 0)
  fi
  [ "$proved" = 1 ] || [ -s "$OUT/out/$key.out" ] || rm -f "$OUT/out/$key.out"
  rm -f "$hx" "$dp"
  { flock 9
    printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
      "$name" "$lane" "$proved" "${rows:-0}" "${pos:-0}" "$nb" "$split" "$szs" "$rc" "$wall" "$cpu" \
      "$rss" "$clock" "$steps" "$rules" "$cps" "$hxb" "$mode" "$ex" "$every" "$db" "$gf" >> "$IDX"
  } 9>> "$IDX.lock"
  echo "$(date -u +%T) [$((idx + 1))/$NJOBS] $name $lane ${szs:--} ${wall}s $mode rows=$rows pos=$pos dump=${db}B ${gf}"
}
worker() {
  local dir=$WORK/w$1 idx
  mkdir -p "$dir"
  while [ "$(date +%s)" -lt "$DEADLINE" ]; do
    idx=$(claim); [ "$idx" -ge "$NJOBS" ] && break
    run_job "$dir" "$idx" "${JOBS[$idx]}"
  done
}
pids=()
for w in $(seq 1 "$PAR"); do worker "$w" & pids+=($!); done
while :; do
  alive=0; for p in "${pids[@]}"; do kill -0 "$p" 2>/dev/null && alive=1; done
  [ "$alive" = 0 ] && break
  if [ "$(date +%s)" -ge $((DEADLINE + TMO + XTMO + 60)) ]; then
    echo "workers past the deadline; stopping them"; pkill -TERM -f -- "^$BIN --lane=" 2>/dev/null
    sleep 10; kill "${pids[@]}" 2>/dev/null; break
  fi
  sleep 5
done
wait 2>/dev/null
left=$(awk -F'\t' 'FILENAME == ARGV[1] { d[$1 "\t" $2] = 1; next } !(($1 "\t" $4) in d)' "$IDX" "$JOBS_F" | wc -l)
st=complete; [ "$left" -gt 0 ] && st=deadline
{ echo "status=$st"; echo "finished=$(date -u +%FT%TZ) elapsed_s=$(($(date +%s) - START))"; echo "jobs=$NALL with_row=$((NALL - left)) pending_left=$left"; } > "$IDX.done"
find /tmp -maxdepth 1 \( -name 'thvm_sched_*' -o -name 'thvm_atp_*' -o -name 'thvm_split_*' -o -name 'thvm_phi_*' \) -mmin +5 -delete 2>/dev/null
rm -rf "$WORK"
echo "runner: $st; $((NALL - left)) of $NALL"
