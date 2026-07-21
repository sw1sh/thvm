#!/usr/bin/env bash
# ENIGMA Tier-2 phase-2b DAgger rollout: re-prove the CASC UEQ training
# corpora with the round-k GCN ARMED (coop re-rank) and the anonymised-
# graph export ON, so the exported CP stream is the LEARNER-guided
# distribution -- exactly the states the deployed model visits.  On PROVED
# the proof-DAG labeller marks the learner-trajectory positives; every
# PROVED problem's rows are kept (the guided negatives are the DAgger
# signal).  Problems the guided run fails to prove yield no rows (export
# fires only on PROVED) but are COUNTED, so a degenerate round (guidance
# losing many previously-solved problems) is visible.
#
# The casc29 + casc30 UEQ sets share 168 of 276 TPTP problems by name; the
# worklist is DEDUPED to the 384 unique problems (first occurrence wins) so
# the guided export makes no duplicate rows and the pass is ~30% cheaper.
# cascJ12 stays a FIXED baseline-distribution held-out set (never rolled).
#
# SERIAL, one problem per FindProof process, RSS self-abort, FOREGROUND.
# RESUMABLE: each problem is recorded in a .progress file after its rows
# are appended (exports go to a per-problem temp, appended only on
# success), so re-invoking resumes where a wall-clock-capped call stopped.
#
#   RTAG=r1 ASSET=tools/atp_enigma/gcn_tier2_casc_round1.safetensors \
#     bash tools/atp_enigma/dagger_round.sh          # call until "ROUND COMPLETE"
#
# ASSET empty => unarmed baseline reference pass (no export; solved
# manifest only) -- the degeneracy reference.
#
# Env: BIN, OUTDIR (~/.cache/thvm/enigma_tier2), WALL (6), PERIOD (200),
# TOPK (512), COOP (2), TPTP (~/.cache/thvm/casc), RESET (1 to restart),
# THVM_ATP_RSS_ABORT_MB (4500).
set -u
export TPTP="${TPTP:-$HOME/.cache/thvm/casc}"
BIN="${BIN:-bin/FindProof}"
OUTDIR="${OUTDIR:-$HOME/.cache/thvm/enigma_tier2}"
WALL="${WALL:-6}"
PERIOD="${PERIOD:-200}"
TOPK="${TOPK:-512}"
COOP="${COOP:-2}"
RTAG="${RTAG:-r1}"
ASSET="${ASSET:-}"
RESET="${RESET:-0}"
export THVM_ATP_RSS_ABORT_MB="${THVM_ATP_RSS_ABORT_MB:-4500}"
mkdir -p "$OUTDIR"
C=~/.cache/thvm/casc

PROG="$OUTDIR/roll_${RTAG}.progress"
WORK="$OUTDIR/roll_${RTAG}.worklist"
OUT="$OUTDIR/roll_${RTAG}.jsonl"
SOLVED="$OUTDIR/roll_${RTAG}.solved"
STATS="$OUTDIR/roll_${RTAG}.stats"

# Build the DEDUPED worklist once (probpath<TAB>prob; first name wins).
if [ "$RESET" = "1" ] || [ ! -s "$WORK" ]; then
  : > "$WORK.raw"
  for spec in "$C/enigma_dataset/casc29_proofs $C/casc29/Problems/UEQ/UEQProblemFiles" \
              "$C/enigma_dataset/casc30_proofs $C/Problems30/UEQ"; do
    set -- $spec; proofs="$1"; pdir="$2"
    for pf in "$proofs"/*.proof; do
      prob=$(basename "$pf" .proof)
      [ -f "$pdir/$prob.p" ] && printf "%s\t%s\n" "$pdir/$prob.p" "$prob" >> "$WORK.raw"
    done
  done
  awk -F'\t' '!seen[$2]++' "$WORK.raw" > "$WORK"; rm -f "$WORK.raw"
  : > "$PROG"; : > "$SOLVED"
  [ -n "$ASSET" ] && { : > "$OUT"; : > "$STATS"; }
fi
touch "$PROG"

if [ -n "$ASSET" ]; then
  export THVM_ATP_GNN_ASSET="$ASSET" THVM_ATP_GNN_RERANK_PERIOD="$PERIOD" \
         THVM_ATP_GNN_TOPK="$TOPK" THVM_ATP_GNN_COOP_RATIO="$COOP"
  echo "DAgger rollout [$RTAG] ARMED asset=$ASSET period=$PERIOD topk=$TOPK coop=$COOP wall=$WALL"
else
  echo "DAgger reference [$RTAG] UNARMED (baseline) wall=$WALL"
fi
total=$(wc -l < "$WORK" | tr -d ' ')
echo "worklist(dedup)=$total done=$(wc -l < "$PROG" | tr -d ' ')"

tmp="$OUTDIR/.roll_${RTAG}.tmp.jsonl"
processed=0
while IFS=$'\t' read -r p prob; do
  grep -qxF "$prob" "$PROG" && continue           # resume: skip done
  msg=""
  # < /dev/null: the loop's stdin is $WORK; without detaching, FindProof
  # consumes worklist lines and the read loop skips problems.
  if [ -n "$ASSET" ]; then
    rm -f "$tmp"
    msg=$(THVM_ATP_GOAL_NF_DEBUG=1 THVM_ATP_ENIGMA_GRAPH_EXPORT="$tmp" \
          "$BIN" "$p" --preset=waldmeister-lazy --wall="$WALL" </dev/null 2>&1)
  else
    msg=$("$BIN" "$p" --preset=waldmeister-lazy --wall="$WALL" </dev/null 2>&1)
  fi
  isproved=0
  echo "$msg" | grep -qE "=> PROVED|SZS status (Unsatisfiable|Theorem|ContradictoryAxioms)" && isproved=1
  [ "$isproved" -eq 1 ] && echo "$prob" >> "$SOLVED"
  if [ -n "$ASSET" ] && [ "$isproved" -eq 1 ]; then
    kbo=$(echo "$msg" | grep -oE "kbo_eq=[01]" | tail -1 | cut -d= -f2)
    npos=$(echo "$msg" | grep -oE "\([0-9]+ proof-relevant" | grep -oE "[0-9]+")
    ng=$(echo "$msg" | grep -oE "graph export: [0-9]+ graphs" | grep -oE "[0-9]+")
    : "${kbo:=-}"; : "${npos:=0}"; : "${ng:=0}"
    [ -f "$tmp" ] && cat "$tmp" >> "$OUT"
    printf "%s kbo=%s graphs=%s npos=%s\n" "$prob" "$kbo" "$ng" "$npos" >> "$STATS"
    rm -f "$tmp"
  fi
  echo "$prob" >> "$PROG"
  processed=$((processed + 1))
  [ $((processed % 25)) -eq 0 ] && echo "[$RTAG] +$processed ($(wc -l < "$PROG" | tr -d ' ')/$total)"
done < "$WORK"

done_n=$(wc -l < "$PROG" | tr -d ' ')
if [ "$done_n" -ge "$total" ]; then
  ns=$(wc -l < "$SOLVED" | tr -d ' ')
  echo "ROUND COMPLETE [$RTAG]: $done_n/$total processed, solved=$ns"
  if [ -n "$ASSET" ]; then
    ng=$(wc -l < "$OUT" | tr -d ' ')
    wp=$(awk '$4!="npos=0"' "$STATS" | wc -l | tr -d ' ')
    ef=$(grep -c "kbo=0" "$STATS" 2>/dev/null || echo 0)
    echo "[$RTAG] graphs=$ng  proofs-with-positives=$wp  extract-fail(kbo=0)=$ef"
    ref="$OUTDIR/roll_base.solved"
    if [ -f "$ref" ]; then
      lost=$(comm -23 <(sort "$ref") <(sort "$SOLVED") | wc -l | tr -d ' ')
      gained=$(comm -13 <(sort "$ref") <(sort "$SOLVED") | wc -l | tr -d ' ')
      echo "[$RTAG] vs baseline(solved $(wc -l < "$ref" | tr -d ' ')): LOST $lost, GAINED $gained"
      [ "$lost" -gt 0 ] && { echo "[$RTAG] LOST:"; comm -23 <(sort "$ref") <(sort "$SOLVED") | tr '\n' ' '; echo; }
    fi
  fi
else
  echo "PARTIAL [$RTAG]: $done_n/$total processed -- re-invoke to resume"
fi
