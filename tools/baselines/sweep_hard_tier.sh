#!/bin/bash
# Iso-kernel sweep over hard-tier theorems.  Each (theory, theorem,
# preset) cell runs in its own wolframscript subprocess so a heavy
# theorem's memory footprint can't blow up the parent process.
# Accumulates one TSV row per success into the output file; on
# subprocess failure / timeout, emits a row with all numeric
# columns = "-".
set -uo pipefail
cd /Users/swish/src/thvm

OUT="${1:-tools/baselines/parity_perstep_hard.tsv}"

# Per-case wall-clock budget (subprocess timeout).  TFindProof and
# TWaldmeisterProofObject each cap themselves but a hard case can
# spend 12s + 15s + parse overhead, so 60s gives a safety margin.
PER_CASE_TIMEOUT=60

echo -e "preset\ttheory\ttheorem\tpreset_constructs\tcli_constructs\tmatched\tpreset_only\tcli_only" > "$OUT"

# (theory, thm) tuples -- the 4 new hard cases where Method->Automatic
# proved the theorem in <2s on the preset side.
CASES=(
    "WolframAxioms DoubleNegation"
    "WolframAxioms Commutativity"
    "MeredithAxioms AndAssociativity"
    "MeredithAxioms OrAssociativity"
)

# Presets to compare per case.
PRESETS=(Waldmeister Automatic VampireUEQ)

for case in "${CASES[@]}"; do
    read theory thm <<< "$case"
    for preset in "${PRESETS[@]}"; do
        echo "=== $preset :: $theory / $thm ==="
        row=$(timeout "$PER_CASE_TIMEOUT" \
            env WMCLI="${WMCLI:-/Users/swish/src/wolfram/waldmeister/wmcli}" \
            wolframscript -file tools/baselines/diff_one_case.wls \
            "$theory" "$thm" "$preset" 2>/dev/null | tail -1)
        if [ -z "$row" ] || ! echo "$row" | grep -q "^${preset}"; then
            row="${preset}	${theory}	${thm}	-	-	-	-	-"
            echo "  TIMEOUT or CRASH"
        else
            echo "  $row"
        fi
        echo -e "$row" >> "$OUT"
    done
done

echo
echo "wrote $OUT"
