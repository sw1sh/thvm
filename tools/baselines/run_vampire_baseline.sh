#!/usr/bin/env bash
# Run Vampire 5.0.1 portfolio mode on every NotableTheorem of every
# AxiomaticTheory.  ONE Vampire process at a time (200MB RSS each,
# 30s wall + 2GB memory bound enforced by Vampire itself -- no thvm
# kernel involved, so no WolframKernel memory pressure).
#
# Output: tools/baselines/vampire_notable_theorems.tsv (parallel to
# the WL baseline cache; commit once, re-read forever).
#
# Pipeline:
#   1. enumerate all Theory::Theorem pairs via wolframscript (~5s)
#   2. export TPTP problem files (~15s for 107 cases, one wolframscript)
#   3. run Vampire on each TPTP file (~5-15 min realistic)
#   4. tabulate to TSV

set -u

VAMPIRE=${VAMPIRE:-/opt/homebrew/bin/vampire}
TC=${TC:-30}             # seconds per case
TPTP_DIR="tools/baselines/vampire_tptp"
RAW_DIR="tools/baselines/vampire_raw"
LIST_FILE="tools/baselines/all_theorems.txt"
OUT="tools/baselines/vampire_notable_theorems.tsv"

if [[ ! -x $VAMPIRE ]]; then
    echo "Vampire not found at $VAMPIRE (set VAMPIRE=path/to/vampire)" >&2
    exit 1
fi

mkdir -p "$TPTP_DIR" "$RAW_DIR"

# Step 1: enumerate Theory::Theorem pairs
echo "[vampire-baseline] (1/3) enumerating NotableTheorems..."
/Applications/Wolfram.app/Contents/MacOS/wolframscript -code '
Scan[Function[th, Module[{nt = AxiomaticTheory[th, "NotableTheorems"]},
    If[ AssociationQ[nt],
        Scan[WriteString["stdout", ToString[th], "::", ToString[#], "\n"] &,
             Keys[nt]]]]],
    AxiomaticTheory[]]' < /dev/null > "$LIST_FILE" 2>/dev/null

n=$(wc -l < "$LIST_FILE" | tr -d ' ')
echo "[vampire-baseline]   $n theorem entries"

# Step 2: export all TPTP problems
echo "[vampire-baseline] (2/3) exporting TPTP problems..."
/Applications/Wolfram.app/Contents/MacOS/wolframscript \
    -f tools/vampire/export_all.wls "$LIST_FILE" "$TPTP_DIR" \
    < /dev/null 2>&1 | tail -3

n_tptp=$(ls "$TPTP_DIR"/*.p 2>/dev/null | wc -l | tr -d ' ')
echo "[vampire-baseline]   $n_tptp TPTP files generated"

# Step 3: run Vampire batched
echo "[vampire-baseline] (3/3) running Vampire (TC=${TC}s/case)..."
echo -e "theory\tthm\tconjunct\tstatus\tseconds\twinningStrategy\tproofClauses" > "$OUT"

count=0
proven=0
saturated=0
timedout=0
errored=0
for f in "$TPTP_DIR"/*.p; do
    base=$(basename "$f" .p)
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

    t0=$(date +%s.%N)
    "$VAMPIRE" \
        --time_limit "$TC" \
        --memory_limit 2048 \
        --mode portfolio \
        --proof tptp \
        "$f" > "$raw" 2>&1 || true
    t1=$(date +%s.%N)
    dt=$(echo "$t1 - $t0" | bc)

    # PROVED only on exact "Termination reason: Refutation".
    # "Refutation not found, incomplete strategy" = saturated/incomplete.
    if grep -qE "^% Termination reason: Refutation$" "$raw"; then
        result="PROVED"
        proven=$((proven + 1))
        strategy=$(grep -m1 -oE "Termination reason: Refutation" "$raw")
        strategy=$(grep -m1 "Successful strategy:" "$raw" | sed -E 's/.*Successful strategy: *//' | head -c 80)
        [[ -z "$strategy" ]] && strategy="portfolio"
        clauses=$(grep -m1 -oE "[0-9]+ generated clauses" "$raw" | grep -oE "^[0-9]+" || echo "-")
    elif grep -qE "^% Termination reason: Time limit$" "$raw"; then
        result="TimedOut"; strategy="-"; clauses="-"
        timedout=$((timedout + 1))
    elif grep -qE "^% Termination reason: Satisfiable$" "$raw"; then
        result="Saturated"; strategy="-"; clauses="-"
        saturated=$((saturated + 1))
    else
        result="Error"; strategy="-"; clauses="-"
        errored=$((errored + 1))
    fi
    printf "%s\t%s\t%s\t%s\t%.2f\t%s\t%s\n" \
        "$theory" "$theorem" "$conjunct" "$result" "$dt" "$strategy" "$clauses" \
        | tee -a "$OUT" > /dev/null
done

echo "---"
printf "DONE.  %d cases.  PROVED=%d  Saturated=%d  TimedOut=%d  Error=%d\n" \
    "$count" "$proven" "$saturated" "$timedout" "$errored"
echo "Cached: $OUT"
