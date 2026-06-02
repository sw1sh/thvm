#!/usr/bin/env bash
# Drive compare_proof_shape.wls across the easy-case batch + emit
# an aggregated TSV with per-rule deltas.
set -u
PRESET=${1:-VampireUEQ}
TC=${2:-10}
HARDLIMIT=$((TC + 15))
CASES=${CASES:-/tmp/cmp_cases.tsv}
OUT=tools/baselines/compare_proof_shape_batch.tsv

if [[ ! -f $CASES ]]; then
    cat > "$CASES" <<'EOF'
AbelianGroupAxioms	ImpliesAbelianMcCuneAxioms
AbelianGroupAxioms	ImpliesMcCuneAxioms
AbelianGroupAxioms	InverseOfComposite
AbelianGroupAxioms	InverseOfInverse
BooleanAxioms	DoubleNegation
GroupAxioms	InverseOfInverse
HillmanAxioms	Commutativity
MeredithAxioms	Commutativity
EOF
fi

echo -e "theory\tthm\tpreset\tthvm_status\tthvm_secs\tthvm_inf\tvamp_status\tvamp_secs\tvamp_inf\tidentical" > $OUT

while IFS=$'\t' read -r theory thm; do
    line=$(timeout --kill-after=5s ${HARDLIMIT}s wolframscript \
        -f tools/baselines/compare_proof_shape.wls \
        "$theory" "$thm" "$PRESET" "$TC" \
        </dev/null 2>/dev/null | tail -1)
    if [[ -z $line ]]; then
        line="${theory}\t${thm}\t${PRESET}\tCRASH\t-\t-\tCRASH\t-\t-\t-"
    fi
    printf '%b\n' "$line" | tee -a $OUT
done < "$CASES"

echo
echo "=== identity summary ==="
awk -F'\t' 'NR>1 {c[$10]++} END {for (k in c) printf "  %-15s %d\n", k, c[k]}' $OUT
