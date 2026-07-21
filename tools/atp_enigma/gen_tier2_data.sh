#!/usr/bin/env bash
# Generate ENIGMA Tier-2 graph datasets over the solved CASC UEQ corpora.
#
# SERIAL, one problem per FindProof process (memory released between
# problems), wall 6, RSS-capped.  For each corpus re-prove its
# known-solved problems (the *_proofs/ manifest) with the anonymised-graph
# export armed (THVM_ATP_ENIGMA_GRAPH_EXPORT) and accumulate one JSONL
# graph per processed critical pair, labelled on PROVED by proof-DAG
# reachability.  casc29 + casc30 -> train, cascJ12 -> held out.
#
#   TPTP=~/.cache/thvm/casc bash tools/atp_enigma/gen_tier2_data.sh
#
# Env: BIN (bin/FindProof), OUTDIR (~/.cache/thvm/enigma_tier2), WALL (6),
# THVM_ATP_RSS_ABORT_MB (4500).
set -u
export TPTP="${TPTP:-$HOME/.cache/thvm/casc}"
BIN="${BIN:-bin/FindProof}"
OUTDIR="${OUTDIR:-$HOME/.cache/thvm/enigma_tier2}"
WALL="${WALL:-6}"
export THVM_ATP_RSS_ABORT_MB="${THVM_ATP_RSS_ABORT_MB:-4500}"
mkdir -p "$OUTDIR"

gen() {  # name proofs_dir problem_dir out.jsonl
  local name="$1" proofs="$2" pdir="$3" out="$4"
  : > "$out"
  local n=0 proved=0
  for pf in "$proofs"/*.proof; do
    local prob; prob=$(basename "$pf" .proof)
    local p="$pdir/$prob.p"
    [ -f "$p" ] || continue
    n=$((n + 1))
    local msg
    msg=$(THVM_ATP_ENIGMA_GRAPH_EXPORT="$out" \
          "$BIN" "$p" --preset=waldmeister-lazy --wall="$WALL" 2>&1)
    echo "$msg" | grep -q "graph export" && proved=$((proved + 1))
    if [ $((n % 25)) -eq 0 ]; then
      echo "[$name] $n tried, $proved proved, $(wc -l < "$out" | tr -d ' ') graphs"
    fi
  done
  echo "[$name] DONE: $n tried, $proved proved, $(wc -l < "$out" | tr -d ' ') graphs -> $out"
}

C=~/.cache/thvm/casc
gen casc29  "$C/enigma_dataset/casc29_proofs"  "$C/casc29/Problems/UEQ/UEQProblemFiles"  "$OUTDIR/train_c29.jsonl"
gen casc30  "$C/enigma_dataset/casc30_proofs"  "$C/Problems30/UEQ"                        "$OUTDIR/train_c30.jsonl"
gen cascJ12 "$C/enigma_dataset/cascJ12_proofs" "$C/cascJ12/Problems/UEQ/UEQProblemFiles"  "$OUTDIR/held_j12.jsonl"
echo "ALL DONE"
