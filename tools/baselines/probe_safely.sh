#!/bin/bash
# Safe wrapper for wolframscript probes that can hang the C engine
# (per docs/atp/wolfram_assoc_wl_hang.md).  Sets a sensible heap cap,
# enforces a hard wall-clock timeout, and reaps the orphan kernel
# if the inner wolframscript overruns.
#
# Usage:
#   probe_safely.sh <wall-seconds> <wolframscript-file> [args...]
#   THVM_HEAP_CELLS=... probe_safely.sh ... (override heap)
#
# Example:
#   probe_safely.sh 60 tools/baselines/diff_one_case.wls \
#       AbelianGroupAxioms InverseOfInverse Waldmeister
set -uo pipefail

if [ $# -lt 2 ]; then
    echo "usage: $0 <wall-seconds> <script.wls> [args...]" >&2
    exit 2
fi

WALL="$1"
SCRIPT="$2"
shift 2

# Default heap: 512M cells (4GB).  Big enough for the easy ATP
# theorems + Vampire-SZS reparse without overshooting on tight
# machines.  Override via env for the deeper Sheffer-class probes.
export THVM_HEAP_CELLS="${THVM_HEAP_CELLS:-536870912}"

# Use the absolute path for orphan-reap matching after kill.
SCRIPT_NAME="$(basename "$SCRIPT")"

# Track child + grandchild PIDs so we can kill the WolframKernel
# directly (per memory: killing wolframscript ≠ killing its
# WolframKernel child).
timeout --kill-after=5 "$WALL" \
    wolframscript -file "$SCRIPT" "$@"
EXIT=$?

# After timeout (exit 124) or kill-after (exit 137), reap any
# WolframKernel still running this script's name.  Filter is
# narrow: only kernels whose ETIME ≥ wall-seconds AND whose
# wrapper command contains the script's basename.  User's
# notebook + VsCodeWolfram + WolframLLMUtilities kernels stay
# untouched.
if [ "$EXIT" -ne 0 ]; then
    # Find wolframscript wrappers running this script.
    for pid in $(ps -ef | \
        grep -F "$SCRIPT_NAME" | grep -v grep | \
        awk '{print $2}'); do
        # Get its WolframKernel child PID.
        for kpid in $(pgrep -P "$pid" 2>/dev/null); do
            # And the grandchild kernel.
            for gkpid in $(pgrep -P "$kpid" 2>/dev/null); do
                kill -9 "$gkpid" 2>/dev/null && \
                    echo "  reaped orphan kernel pid=$gkpid" >&2
            done
            kill -9 "$kpid" 2>/dev/null && \
                echo "  reaped orphan wrapper pid=$kpid" >&2
        done
        kill -9 "$pid" 2>/dev/null && \
            echo "  reaped orphan timeout pid=$pid" >&2
    done
fi

exit "$EXIT"
