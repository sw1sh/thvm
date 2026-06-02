#!/usr/bin/env bash
# Compare a thvm preset's per-conjunct status vs an external ATP CLI's
# status on the SHARED (theory, thm) cases.  Methodology pivot
# (2026-06-02): match easy cases against ground truth first, only
# then move to hard ones.
#
# Usage:
#   compare_preset_vs_cli.sh <preset> <cli> [TC]
#     preset = thvm Method string ("VampireUEQ", "Waldmeister",
#              "Twee", "EProver", "WaldmeisterLazy", ...)
#     cli    = external prover ("vampire" only for now;
#              twee/wmcli scaffolded as the WaldmeisterProcess /
#              TweeProcess methods land)
#     TC     = per-case TimeConstraint seconds (default 30)
#
# Output: TSV with columns
#   theory \t thm \t thvm_status \t thvm_secs \t cli_status \t cli_secs \t match
#
# Where match is one of:
#   AGREE-PROVED   both proved
#   AGREE-FAIL     neither proved
#   THVM-ONLY      thvm cracked, CLI did not (we win)
#   CLI-ONLY       CLI cracked, thvm did not (port gap)

set -u
PRESET=${1:?"need preset name"}
CLI=${2:?"need cli name"}
TC=${3:-30}
HARDLIMIT=$((TC + 15))
OUT=tools/baselines/compare_${PRESET,,}_vs_${CLI}.tsv

THVM_RUNNER=tools/baselines/run_thvm_one_conjunct.wls
case $CLI in
    vampire) CLI_RUNNER=tools/baselines/run_vampire_process_one.wls ;;
    *) echo "Unknown CLI '$CLI'" >&2; exit 1 ;;
esac

echo -e "theory\tthm\tthvm_status\tthvm_secs\tcli_status\tcli_secs\tmatch" > $OUT

# Iterate over the per-conjunct counts list (built earlier).
COUNTS=${COUNTS:-/tmp/all_counts.tsv}
if [[ ! -f $COUNTS ]]; then
    echo "Build conjunct count file first: $COUNTS" >&2
    exit 1
fi

i=0
while IFS=$'\t' read -r theory thm n; do
    i=$((i+1))
    # thvm side: pass Method via env (the runner doesn't take Method
    # as arg yet); for now we have to write a tiny shim or use
    # run_thvm_one_method.wls.
    thvm_line=$(timeout --kill-after=5s ${HARDLIMIT}s wolframscript \
        -f tools/baselines/run_thvm_one_method.wls \
        "$theory" "$thm" "$TC" "\"$PRESET\"" \
        </dev/null 2>&1 | grep -E '^[A-Za-z]+	' | tail -1)
    thvm_status=$(echo "$thvm_line" | awk -F'\t' '{print $3}')
    thvm_secs=$(echo "$thvm_line" | awk -F'\t' '{print $4}')

    cli_line=$(timeout --kill-after=5s ${HARDLIMIT}s wolframscript \
        -f $CLI_RUNNER "$theory" "$thm" "$TC" </dev/null 2>&1 | grep -E '^[A-Za-z]+	' | tail -1)
    cli_status=$(echo "$cli_line" | awk -F'\t' '{print $4}')
    cli_secs=$(echo "$cli_line" | awk -F'\t' '{print $5}')

    if [[ "$thvm_status" == "PROVED" && "$cli_status" == "PROVED" ]]; then
        match="AGREE-PROVED"
    elif [[ "$thvm_status" != "PROVED" && "$cli_status" != "PROVED" ]]; then
        match="AGREE-FAIL"
    elif [[ "$thvm_status" == "PROVED" ]]; then
        match="THVM-ONLY"
    else
        match="CLI-ONLY"
    fi

    printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
        "$theory" "$thm" "$thvm_status" "$thvm_secs" \
        "$cli_status" "$cli_secs" "$match" | tee -a $OUT

    if [[ $((i % 20)) -eq 0 ]]; then
        free_mb=$(vm_stat | awk '/Pages free/ {gsub("\\.",""); printf "%d", $3 * 16 / 1024}')
        echo "  [$i] free=${free_mb}MB" >&2
    fi
done < $COUNTS

echo
echo "=== summary ==="
awk -F'\t' 'NR>1 {c[$7]++} END {for (k in c) printf "  %-15s %d\n", k, c[k]}' $OUT
