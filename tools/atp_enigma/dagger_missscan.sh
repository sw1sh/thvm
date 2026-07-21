#!/usr/bin/env bash
# ENIGMA Tier-2 phase-2b MISS-SCAN: arm the best DAgger round and single-
# engine scan the 35 known CASC-30 UEQ misses at wall=60.  ANY flip (a
# problem the full schedule never solved, now proved) is a headline.
# Resumable via a .progress file (10-min foreground caps).  GRP* use LPO
# (the flatterm-LPO cluster); the rest use the preset KBO.
#
#   ASSET=tools/atp_enigma/gcn_tier2_casc_round3.safetensors \
#     bash tools/atp_enigma/dagger_missscan.sh          # call until DONE
#
# Env: BIN, TPTP, WALL (60), PERIOD (200), TOPK (512), COOP (2), ASSET
# (empty => unarmed control), OUTDIR, RESET.
set -u
export TPTP="${TPTP:-$HOME/.cache/thvm/casc}"
BIN="${BIN:-bin/FindProof}"
WALL="${WALL:-60}"
PERIOD="${PERIOD:-200}"
TOPK="${TOPK:-512}"
COOP="${COOP:-2}"
ASSET="${ASSET:-}"
OUTDIR="${OUTDIR:-$HOME/.cache/thvm/enigma_tier2}"
TAG="${TAG:-miss}"
RESET="${RESET:-0}"
export THVM_ATP_RSS_ABORT_MB="${THVM_ATP_RSS_ABORT_MB:-6000}"
C=~/.cache/thvm/casc
PROG="$OUTDIR/${TAG}.progress"
RES="$OUTDIR/${TAG}.results"
[ "$RESET" = "1" ] && { : > "$PROG"; : > "$RES"; }
touch "$PROG" "$RES"

MISS="COL002-10 COL043-1 COL065-1 COL066-1 GRP196-1 GRP661-1 GRP662-1 \
GRP663-1 GRP664-12 GRP666-4 GRP666-5 GRP690-1 GRP692-1 GRP694-1 GRP695-1 \
GRP720-2 LAT065-10 LAT138-1 LAT186-10 LAT190-10 LAT400-3 LAT401-1 \
LCL306-10 LCL339-10 LCL348-10 NUM284-10.014 RNG025-5 RNG027-7 RNG027-9 \
RNG028-5 RNG028-8 RNG029-5 RNG029-6 ROB031-2 ROB035-1"

if [ -n "$ASSET" ]; then
  export THVM_ATP_GNN_ASSET="$ASSET" THVM_ATP_GNN_RERANK_PERIOD="$PERIOD" \
         THVM_ATP_GNN_TOPK="$TOPK" THVM_ATP_GNN_COOP_RATIO="$COOP"
  echo "MISS-SCAN ARMED asset=$ASSET period=$PERIOD topk=$TOPK coop=$COOP wall=$WALL"
else
  echo "MISS-SCAN UNARMED (control) wall=$WALL"
fi

for prob in $MISS; do
  grep -qxF "$prob" "$PROG" && continue
  p="$C/Problems30/UEQ/$prob.p"
  if [ ! -f "$p" ]; then echo "$prob MISSING-FILE" | tee -a "$RES"; echo "$prob" >> "$PROG"; continue; fi
  ord=""; case "$prob" in GRP*) ord="--ord=lpo";; esac
  t0=$(python3 -c 'import time;print(time.time())')
  msg=$("$BIN" "$p" --preset=waldmeister-lazy $ord --wall="$WALL" </dev/null 2>&1)
  t1=$(python3 -c 'import time;print(time.time())')
  el=$(python3 -c "print('%.1f'%($t1-$t0))")
  if echo "$msg" | grep -qE "=> PROVED|SZS status (Unsatisfiable|Theorem|ContradictoryAxioms)"; then
    st="*** PROVED ***"
  elif echo "$msg" | grep -qiE "SZS status (Satisfiable|CounterSatisfiable)|=> SATURATED"; then
    st="SAT"
  else
    st="gaveup"
  fi
  printf "%-16s %-8s %7ss  %s\n" "$prob" "${ord:-kbo}" "$el" "$st" | tee -a "$RES"
  echo "$prob" >> "$PROG"
done

done_n=$(grep -c . "$PROG")
tot=$(echo $MISS | wc -w | tr -d ' ')
if [ "$done_n" -ge "$tot" ]; then
  echo "MISS-SCAN DONE: $done_n/$tot scanned"
  echo "FLIPS: $(grep -c 'PROVED' "$RES") proved"
  grep 'PROVED' "$RES" || echo "(no flips)"
else
  echo "MISS-SCAN PARTIAL: $done_n/$tot -- re-invoke"
fi
