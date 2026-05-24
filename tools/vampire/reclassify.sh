#!/usr/bin/env bash
# reclassify.sh - rebuild /tmp/vampire/results.tsv from the raw
# /tmp/vampire/raw/*.out files, using strict refutation detection.

set -u
RAW_DIR="/tmp/vampire/raw"
TSV="/tmp/vampire/results.tsv"

printf "Theory\tTheorem\tConjunct\tResult\tTimeS\tWinningStrategy\tProofClauses\n" > "$TSV"

count=0
proven=0
for f in "$RAW_DIR"/*.out; do
    base=$(basename "$f" .out)
    theory="${base%%__*}"
    rest="${base#*__}"
    if [[ "$rest" == *"__c"* ]]; then
        theorem="${rest%__c*}"
        conjunct="${rest##*__c}"
    else
        theorem="$rest"
        conjunct="1"
    fi

    count=$((count + 1))

    if grep -qE "^% Termination reason: Refutation$" "$f"; then
        result="PROVED"
        proven=$((proven + 1))
    elif grep -q "Refutation not found" "$f"; then
        result="SATURATED"
    elif grep -q "Time limit reached" "$f"; then
        result="TIMEOUT"
    else
        result="OTHER"
    fi

    if [ "$result" = "PROVED" ]; then
        # Time = the Time elapsed line from the successful subrun (last one before final summary).
        # The summary block follows the strategy line that proved it.
        time_s=$(grep "Time elapsed" "$f" | tail -1 | awk '{print $4}')
        ref_line=$(grep -n "Refutation found" "$f" | head -1 | cut -d: -f1)
        strat=$(head -n "$ref_line" "$f" | grep -E "^% [a-z]+\\+[0-9]" | tail -1)
        proof_clauses=$(awk '/SZS output start/,/SZS output end/' "$f" | grep -c "^fof(")
    else
        time_s=$(grep "Time elapsed" "$f" | tail -1 | awk '{print $4}')
        strat=$(grep -E "^% [a-z]+\\+[0-9]" "$f" | tail -1)
        proof_clauses="0"
    fi
    [ -z "$time_s" ] && time_s="-"
    strat_short=$(echo "$strat" | sed 's/^% //' | sed 's/ on .*//')
    [ -z "$strat_short" ] && strat_short="-"

    printf "%s\t%s\t%s\t%s\t%s\t%s\t%s\n" \
        "$theory" "$theorem" "$conjunct" "$result" "$time_s" "$strat_short" "$proof_clauses" >> "$TSV"
done

echo "Total: $count, Proven: $proven" >&2
