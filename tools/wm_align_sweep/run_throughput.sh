#!/bin/bash
# Higher-budget THROUGHPUT probe for the giant AC-saturation theorems
# whose proofs are too large to complete inside run_one.sh's 20s thvm
# wall (MeredithAxioms And/OrAssociativity: wmcli cracks them in ~0.6s
# at ~600k critical pairs, but thvm's ~64x-slower-throughput engine
# needs ~38s of completion to reach the same closure).  For these the
# selection-sequence alignment is intractable (600k+ equal-weight CPs
# whose emission order forks at pick ~25, the AC-multiplicity class) --
# the meaningful question is only WHETHER thvm proves given enough
# budget, which run_one.sh's 20s cap forbids.
#
# This script answers that one question: it runs thvm's WM-preset bench
# with a large wall and reports PROVED / RUNNING + the step/rule/cp
# counts at termination.  It does NOT run wmcli or the aligner.
#
# usage:  run_throughput.sh <theorem.pr> [wall_secs] [max_steps]
# env:    (none -- thvm only, never spawns a WolframKernel)
#
# Safety: a hard `timeout` wraps the thvm process; thvm additionally has
# its own in-loop wall cap (the wall_secs argument) and exits cleanly.
# Run theorems ONE AT A TIME, never in parallel (the box has been bricked
# by uncapped ATP sweeps -- see memory feedback_wolframscript_oom_risk).
set -u

PR="${1:?usage: run_throughput.sh <theorem.pr> [wall_secs] [max_steps]}"
[ -f "$PR" ] || { echo "no such .pr: $PR" >&2; exit 2; }
WALL="${2:-120}"
STEPS="${3:-2000000}"
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
NAME="$(basename "$PR" .pr)"
BENCH="$ROOT/bin/test_atp_wolfram_bench"
[ -x "$BENCH" ] || { echo "build first: make bin/test_atp_wolfram_bench" >&2; exit 2; }

WORK="$(mktemp -d /tmp/wm_thru_XXXXXX)"
trap 'rm -rf "$WORK"' EXIT

# wall+30 hard kill margin over thvm's own in-loop cap.
THVM_ATP_WALDMEISTER=1 timeout "$((WALL + 30))" \
    "$BENCH" "$PR" "$STEPS" "$WALL" > "$WORK/out.txt" 2>/dev/null
ST="$(sed -n 's/^=> \([A-Z_]*\).*/\1/p' "$WORK/out.txt" | head -1)"
[ -n "$ST" ] || ST=CRASHED
TAIL="$(grep -E '^  step|^   goal=' "$WORK/out.txt" | tail -1 | tr -s ' ')"
printf 'THRU %s | %s |%s\n' "$NAME" "$ST" "$TAIL"
