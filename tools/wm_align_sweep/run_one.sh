#!/bin/bash
# Selection-sequence identity probe for ONE Waldmeister .pr theorem:
# wmcli (reference) vs thvm's WM-preset C bench, aligned by
# tools/wm_align_sweep/align.py.  Appends exactly one row to the matrix
# TSV (crash-safe: the row lands before exit on every path).
#
# usage:  run_one.sh <theorem.pr> [matrix.tsv]
# env:    WMCLI       path to the wmcli binary (required)
#         WMCLI_DYLD  DYLD_FRAMEWORK_PATH for wmcli's mathlink (optional)
#
# Safety (the box has been bricked by uncapped ATP sweeps -- see
# memory feedback_wolframscript_oom_risk): every external call carries a
# hard `timeout`; the thvm bench additionally has its own in-loop wall
# cap, exits cleanly, and never spawns a WolframKernel.  Run theorems
# ONE AT A TIME, never in parallel.
set -u

PR="${1:?usage: run_one.sh <theorem.pr> [matrix.tsv]}"
[ -f "$PR" ] || { echo "no such .pr: $PR" >&2; exit 2; }
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NAME="$(basename "$PR" .pr)"
MATRIX="${2:-$ROOT/tools/baselines/wm_align_matrix.tsv}"
REPDIR="$ROOT/tools/baselines/wm_align_reports"
BENCH="$ROOT/bin/test_atp_wolfram_bench"
: "${WMCLI:?set WMCLI to the wmcli binary}"
[ -x "$BENCH" ] || { echo "build first: make bin/test_atp_wolfram_bench" >&2; exit 2; }

WORK="$(mktemp -d /tmp/wm_align_XXXXXX)"
trap 'rm -rf "$WORK"' EXIT
mkdir -p "$REPDIR"
[ -f "$MATRIX" ] || printf 'theorem\twm_selections\tthvm_picks\tidentical_prefix\tfull_identity\tfirst_divergence_index\tnote\n' > "$MATRIX"

row() {
    printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
        "$NAME" "$1" "$2" "$3" "$4" "$5" "$6" >> "$MATRIX"
    echo "ROW $NAME | wm=$1 thvm=$2 prefix=$3 identical=$4 firstdiv=$5 | $6"
}

# 1. Cheap probe: anything wmcli itself does not crack in 10s is skipped
#    outright (never trace hard cases at -a 4; the logs explode).
if ! DYLD_FRAMEWORK_PATH="${WMCLI_DYLD:-}" timeout 10 \
        "$WMCLI" -a 2 "$PR" > "$WORK/probe.txt" 2>&1; then
    row - - - - - SKIPPED_SLOW
    exit 0
fi

# 2. Full verbose reference trace.  WM_TRACE_LINES=<n> caps the captured
#    trace to the first n lines: `head` closing the pipe SIGPIPEs wmcli, so
#    a heavy theorem (the associativity tier proves but emits a 3M-CP, far-
#    >30s verbose trace -- the "logs explode" class above) is bounded to a
#    PREFIX.  wm= is then a truncated count, but the identical-prefix +
#    first_divergence are valid as long as the cap exceeds firstdiv (the
#    -a 4 trace is ~95 lines per selection; 50000 lines ~= 520 selections).
WM_TRACE_LINES="${WM_TRACE_LINES:-0}"
if [ "$WM_TRACE_LINES" -gt 0 ]; then
    DYLD_FRAMEWORK_PATH="${WMCLI_DYLD:-}" timeout 90 \
        "$WMCLI" -a 4 "$PR" 2>&1 | head -n "$WM_TRACE_LINES" > "$WORK/wm.txt"
    [ -s "$WORK/wm.txt" ] || { row - - - - - SKIPPED_WM_TRACE; exit 0; }
elif ! DYLD_FRAMEWORK_PATH="${WMCLI_DYLD:-}" timeout 30 \
        "$WMCLI" -a 4 "$PR" > "$WORK/wm.txt" 2>&1; then
    row - - - - - SKIPPED_WM_TRACE
    exit 0
fi

# 3. thvm WM-preset run on the byte-same .pr (bench pr-mode).
#    THVM_ATP_FIFO_THRESHOLD=0: the reference above is PLAIN `wmcli -a 4`,
#    so the thvm side must run plain too -- the default preset is
#    -auto-faithful (formation-FIFO ON), and that config mismatch alone
#    used to explain 10 divergent rows.  For an -auto-vs-auto comparison
#    instead, use a `wmcli -auto -a 4` reference and drop this override
#    (thvm default); no second mode is wired here -- do that run by hand.
THVM_ATP_WALDMEISTER=1 THVM_ATP_CP_PICK_TRACE=1 THVM_ATP_FIFO_THRESHOLD=0 \
    timeout 60 \
    "$BENCH" "$PR" 200000 20 > "$WORK/thvm_out.txt" 2> "$WORK/thvm_err.txt"
ST="$(sed -n 's/^=> \([A-Z_]*\).*/\1/p' "$WORK/thvm_out.txt" | head -1)"
[ -n "$ST" ] || ST=CRASHED

# 4. Align the two selection sequences.
if ! python3 "$ROOT/tools/wm_align_sweep/align.py" \
        --wm "$WORK/wm.txt" \
        --thvm-out "$WORK/thvm_out.txt" \
        --thvm-trace "$WORK/thvm_err.txt" \
        --report "$REPDIR/$NAME.txt" > "$WORK/sum.txt" 2> "$WORK/align_err.txt"; then
    row - - - - - "ALIGN_ERROR thvm=$ST $(head -c 160 "$WORK/align_err.txt" | tr '\t\n' '  ')"
    exit 0
fi
read -r _ WM TH PREFIX IDENT FD NOTE < <(sed -E \
    's/^ALIGN wm=([0-9]+) thvm=([0-9]+) prefix=([0-9]+) identical=(Y|N) firstdiv=([0-9-]+) note=(.*)$/x \1 \2 \3 \4 \5 \6/' \
    "$WORK/sum.txt")
row "$WM" "$TH" "$PREFIX" "$IDENT" "$FD" "thvm=$ST $NOTE"

# Identical rows need no detail file.
[ "$IDENT" = "Y" ] && rm -f "$REPDIR/$NAME.txt"
exit 0
