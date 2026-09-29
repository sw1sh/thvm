#!/bin/bash
# Neural-selection phase 0: harvest labelled derivation histories.
# One FindProof run per pool problem (one per core), ONE fixed schedule
# lane, --proof, with the history export armed; a proved run is labelled
# from its emitted proof by np0_extract.py and the raw stream deleted.
# Satisfiable problems are skipped (no refutation, no labels).  Pod-only.
#
# Usage: np0_harvest.sh <FindProof> <TPTP_root> <pool.tsv> <lane> "<flags>" \
#                       <outdir> [wall=60] [par=16] [rate=1] [rss_mb=3500] \
#                       [max_neg_train=100000] [max_neg_heldout=400000]
# pool.tsv: name, relpath, status, spc, split (np0_pool.py).
# Writes <outdir>/harvest.tsv (name, split, SZS, seconds, raw bytes,
# extract summary), err/<name>.topk (the re-ranker's counter lines when
# the flags arm it) and <outdir>/harvest.done at the end.
set -u
BIN=$1; ROOT=$2; POOL=$3; LANE=$4; FLAGS=$5; OUT=$6
WALL=${7:-60}; PAR=${8:-16}; RATE=${9:-1}; RSS=${10:-3500}
MNT=${11:-100000}; MNH=${12:-400000}
EXT=$(cd "$(dirname "$0")" && pwd)/np0_extract.py
mkdir -p "$OUT/raw" "$OUT/out" "$OUT/err" "$OUT/npz" "$OUT/ext"
touch "$OUT/harvest.tsv"
awk -F'\t' '$3 != "Satisfiable" {print $1 "\t" $2 "\t" $5}' "$POOL" \
  > "$OUT/jobs.tsv"
export BIN ROOT LANE FLAGS OUT WALL RATE RSS MNT MNH EXT
run_one() {
  nm=$(printf '%s' "$1" | cut -f1)
  rel=$(printf '%s' "$1" | cut -f2)
  split=$(printf '%s' "$1" | cut -f3)
  grep -q -P "^\Q$nm\E\t" "$OUT/harvest.tsv" && return 0   # resumable
  hx=$OUT/raw/$nm.hx
  rm -f "$hx"
  t0=$(date +%s.%N)
  # shellcheck disable=SC2086
  (cd /tmp && env FINDPROOF_SLICE="$LANE" THVM_ATP_RSS_ABORT_MB="$RSS" \
     TPTP="$ROOT" THVM_ATP_HISTORY_EXPORT="$hx" THVM_ATP_HISTORY_RATE="$RATE" \
     timeout $((WALL + 60)) "$BIN" $FLAGS --wall="$WALL" --szs --proof \
     "$ROOT/$rel" </dev/null >"$OUT/out/$nm.out" 2>"$OUT/err/$nm.full")
  t1=$(date +%s.%N)
  tail -40 "$OUT/err/$nm.full" > "$OUT/err/$nm.err"
  grep -h 'topk-rerank:' "$OUT/err/$nm.full" > "$OUT/err/$nm.topk"
  rm -f "$OUT/err/$nm.full"
  st=$(grep -m1 -o 'SZS status [A-Za-z]*' "$OUT/out/$nm.out" | awk '{print $3}')
  [ -z "$st" ] && st=NoOutput
  sz=$(stat -c %s "$hx" 2>/dev/null || echo 0)
  ex="-"
  if [ "$st" = Unsatisfiable ]; then
    mn=$MNT; [ "$split" = heldout ] && mn=$MNH
    if [ "$sz" -gt 2500000000 ]; then
      ex="skipped:raw>2.5GB"
    else
      ex=$(nice -n 5 python3 "$EXT" "$hx" "$OUT/out/$nm.out" \
             "$OUT/npz/$nm.npz" "$mn" 2>&1 | tail -1 | tr '\t' ' ')
    fi
  else
    rm -f "$OUT/out/$nm.out"
  fi
  rm -f "$hx"
  printf '%s\t%s\t%s\t%s\t%s\t%s\n' "$nm" "$split" "$st" \
    "$(awk -v a="$t0" -v b="$t1" 'BEGIN{printf "%.2f", b-a}')" "$sz" "$ex" \
    >> "$OUT/harvest.tsv"
}
export -f run_one
tr '\n' '\0' < "$OUT/jobs.tsv" | xargs -0 -P "$PAR" -I{} bash -c 'run_one "$@"' _ {}
echo done > "$OUT/harvest.done"
