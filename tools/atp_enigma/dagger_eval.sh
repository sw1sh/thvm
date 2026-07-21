#!/usr/bin/env bash
# ENIGMA Tier-2 phase-2b online eval: arm a round-k GCN and time the
# must-not-lose easy provers + the hard CASC UEQ targets.  Prints one
# "PROB  ord  wall  elapsed  STATUS" row per problem.  ASSET empty =
# unarmed baseline (the reference the guided runs must not regress).
#
#   ASSET=tools/atp_enigma/gcn_tier2_casc_round2.safetensors \
#     EASY_PERIOD=2000 HARD_PERIOD=200 WALL=60 \
#     bash tools/atp_enigma/dagger_eval.sh
#
# Env: BIN, TPTP, WALL (hard-target wall, 60), EASY_WALL (30),
# EASY_PERIOD (2000), HARD_PERIOD (200), TOPK (512), COOP (2), ASSET.
set -u
export TPTP="${TPTP:-$HOME/.cache/thvm/casc}"
BIN="${BIN:-bin/FindProof}"
WALL="${WALL:-60}"
EASY_WALL="${EASY_WALL:-30}"
EASY_PERIOD="${EASY_PERIOD:-2000}"
HARD_PERIOD="${HARD_PERIOD:-200}"
TOPK="${TOPK:-512}"
COOP="${COOP:-2}"
ASSET="${ASSET:-}"
C=~/.cache/thvm/casc
export THVM_ATP_RSS_ABORT_MB="${THVM_ATP_RSS_ABORT_MB:-6000}"

find_p() {  # problem-basename -> path under any UEQ dir
  local b="$1"
  for d in "$C/Problems30/UEQ" "$C/casc29/Problems/UEQ/UEQProblemFiles" \
           "$C/cascJ12/Problems/UEQ/UEQProblemFiles"; do
    [ -f "$d/$b.p" ] && { echo "$d/$b.p"; return; }
  done
}

prove() {  # basename  ord  wall  period
  local b="$1" ord="$2" wall="$3" period="$4"
  local p; p=$(find_p "$b")
  if [ -z "$p" ]; then printf "%-14s %-4s  MISSING-FILE\n" "$b" "$ord"; return; fi
  local ordflag=""; [ "$ord" = "lpo" ] && ordflag="--ord=lpo"
  local env=()
  if [ -n "$ASSET" ]; then
    env=(THVM_ATP_GNN_ASSET="$ASSET" THVM_ATP_GNN_RERANK_PERIOD="$period"
         THVM_ATP_GNN_TOPK="$TOPK" THVM_ATP_GNN_COOP_RATIO="$COOP")
  fi
  local t0 t1 msg status
  t0=$(python3 -c 'import time;print(time.time())')
  msg=$(env "${env[@]}" "$BIN" "$p" --preset=waldmeister-lazy $ordflag --wall="$wall" 2>&1)
  t1=$(python3 -c 'import time;print(time.time())')
  if echo "$msg" | grep -qE "=> PROVED|SZS status (Unsatisfiable|Theorem|ContradictoryAxioms)"; then
    status="PROVED"
  elif echo "$msg" | grep -qiE "SZS status (Satisfiable|CounterSatisfiable)|=> SATURATED"; then
    status="SAT"
  else
    status="gaveup/timeout"
  fi
  printf "%-14s %-4s  wall=%-3s  %6.2fs  %s\n" "$b" "$ord" "$wall" \
    "$(python3 -c "print($t1-$t0)")" "$status"
}

echo "=== EASY provers (must-not-lose; period=$EASY_PERIOD topk=$TOPK coop=$COOP wall=$EASY_WALL) asset=${ASSET:-BASELINE} ==="
prove LAT171-1 kbo "$EASY_WALL" "$EASY_PERIOD"
prove BOO034-1 kbo "$EASY_WALL" "$EASY_PERIOD"
prove COL002-5 kbo "$EASY_WALL" "$EASY_PERIOD"

echo "=== HARD targets (period=$HARD_PERIOD topk=$TOPK coop=$COOP wall=$WALL) asset=${ASSET:-BASELINE} ==="
prove GRP690-1 lpo "$WALL" "$HARD_PERIOD"
prove GRP692-1 lpo "$WALL" "$HARD_PERIOD"
prove RNG027-9 kbo "$WALL" "$HARD_PERIOD"
prove LCL306-10 kbo "$WALL" "$HARD_PERIOD"
