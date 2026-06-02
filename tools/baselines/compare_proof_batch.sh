#!/usr/bin/env bash
# Run compare_proof_one.wls across a small batch of theorems +
# emit an aggregated TSV.  Default cases are 5 easy theorems that
# both thvm and Vampire CLI PROVE (see the smoke test in
# compare_vampireueq_vs_vampire.tsv).  Override via $CASES file.
#
# Output: tools/baselines/compare_proof_batch.tsv
#   theory \t thm \t preset \t thvm_status \t thvm_secs \t thvm_pl
#   \t vamp_status \t vamp_secs \t vamp_pl \t wall_ratio

set -u
PRESET=${1:-VampireUEQ}
TC=${2:-10}
HARDLIMIT=$((TC + 15))
CASES=${CASES:-/tmp/cmp_cases.tsv}
OUT=tools/baselines/compare_proof_batch.tsv

if [[ ! -f "$CASES" ]]; then
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

echo -e "theory\tthm\tpreset\tthvm_status\tthvm_secs\tthvm_pl\tvamp_status\tvamp_secs\tvamp_pl\twall_ratio" > $OUT

while IFS=$'\t' read -r theory thm; do
    line=$(timeout --kill-after=5s ${HARDLIMIT}s wolframscript \
        -f tools/baselines/compare_proof_one.wls \
        "$theory" "$thm" "$PRESET" "$TC" \
        </dev/null 2>/dev/null | tail -1)
    if [[ -z "$line" ]]; then
        line="${theory}\t${thm}\t${PRESET}\tCRASH\t-\t-\tCRASH\t-\t-\t-"
    fi
    printf '%b\n' "$line" | tee -a $OUT
done < "$CASES"

echo
echo "=== summary ==="
awk -F'\t' 'NR>1 && $4 == "Proved" && $7 == "Proved" {
    n++; sum += $10
} END {
    if (n > 0) printf "  AGREE-PROVED: %d cases, mean wall_ratio=%.1fx\n", n, sum/n
}' $OUT
awk -F'\t' 'NR>1 {c[$4 "/" $7]++} END {for (k in c) printf "  %-25s %d\n", k, c[k]}' $OUT
