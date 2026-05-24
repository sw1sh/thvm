#!/usr/bin/env bash
# run_batch.sh - run Vampire on every TPTP file under /tmp/vampire/tptp/
# and produce /tmp/vampire/results.tsv.
#
# Bounded: 30s wall + 2GB memory per call.  ONE Vampire at a time.

set -u
shopt -s nullglob

TPTP_DIR="/tmp/vampire/tptp"
RAW_DIR="/tmp/vampire/raw"
TSV="/tmp/vampire/results.tsv"

mkdir -p "$RAW_DIR"

# Header
printf "Theory\tTheorem\tConjunct\tResult\tTimeS\tWinningStrategy\tProofClauses\n" > "$TSV"

count=0
proven=0
for f in "$TPTP_DIR"/*.p; do
    base=$(basename "$f" .p)
    # Parse Theory__Theorem[__cN]
    theory="${base%%__*}"
    rest="${base#*__}"
    if [[ "$rest" == *"__c"* ]]; then
        theorem="${rest%__c*}"
        conjunct="${rest##*__c}"
    else
        theorem="$rest"
        conjunct="1"
    fi

    raw="$RAW_DIR/${base}.out"
    count=$((count + 1))
    echo "[$count] Running vampire on $base..." >&2

    # Vampire run - bounded
    vampire \
        --time_limit 30 \
        --memory_limit 2048 \
        --mode portfolio \
        --proof tptp \
        "$f" > "$raw" 2>&1 || true

    # Result classification (PROVED only on EXACTLY "Termination reason: Refutation" --
    # "Refutation not found, incomplete strategy" is a SATURATED outcome).
    if grep -qE "^% Termination reason: Refutation$" "$raw"; then
        result="PROVED"
        proven=$((proven + 1))
    elif grep -q "Refutation not found" "$raw"; then
        result="SATURATED"
    elif grep -q "Time limit reached" "$raw"; then
        result="TIMEOUT"
    else
        result="OTHER"
    fi

    # Time (last Time elapsed before the Refutation, or final one)
    time_s=$(grep "Time elapsed" "$raw" | tail -1 | awk '{print $4}')
    [ -z "$time_s" ] && time_s="-"

    # Winning strategy: the LAST "% <strategy> on <problem> for (Nds)" line
    # before the Refutation output (or the last one if no refutation).
    if [ "$result" = "PROVED" ]; then
        # Find line number of "Refutation found"
        ref_line=$(grep -n "Refutation found" "$raw" | head -1 | cut -d: -f1)
        # Last "% <alg>+..." line before that
        strat=$(head -n "$ref_line" "$raw" | grep -E "^% (lrs|dis|ott|fmb|inst|otter|fnt|ins)" | tail -1)
    else
        strat=$(grep -E "^% (lrs|dis|ott|fmb|inst|otter|fnt|ins)" "$raw" | tail -1)
    fi
    # Extract just the strategy spec (before " on ")
    strat_short=$(echo "$strat" | sed 's/^% //' | sed 's/ on .*//')
    [ -z "$strat_short" ] && strat_short="-"

    # Proof "clauses" -- count of fof( steps in the proof block
    if [ "$result" = "PROVED" ]; then
        proof_clauses=$(awk '/SZS output start/,/SZS output end/' "$raw" | grep -c "^fof(")
    else
        proof_clauses="0"
    fi

    printf "%s\t%s\t%s\t%s\t%s\t%s\t%s\n" \
        "$theory" "$theorem" "$conjunct" "$result" "$time_s" "$strat_short" "$proof_clauses" >> "$TSV"
done

echo "" >&2
echo "Total: $count, Proven: $proven, Failed: $((count - proven))" >&2
echo "TSV: $TSV" >&2
