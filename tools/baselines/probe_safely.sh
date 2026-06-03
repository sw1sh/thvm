#!/bin/bash
# Safe wrapper for wolframscript probes that can hang the C engine
# (per docs/atp/wolfram_assoc_wl_hang.md).  Sets a sensible heap cap
# and enforces a hard wall-clock timeout via `timeout`.
#
# If the inner wolframscript overruns the timeout, `timeout` sends
# SIGTERM and the wolframscript wrapper exits with 124.  The
# WolframKernel grandchild process may survive briefly; if it does,
# reap manually after the fact with:
#
#   ps -e -o pid,etime,command | grep WolframKernel | grep -v VsCode \
#     | grep -v WolframNB | grep -v WolframLLM | awk '$2>"01:00" {print $1}' \
#     | xargs -n1 kill -9
#
# Usage:
#   probe_safely.sh <wall-seconds> <wolframscript-file> [args...]
#   THVM_HEAP_CELLS=... probe_safely.sh ... (override heap)
set -o pipefail

if [ $# -lt 2 ]; then
    echo "usage: $0 <wall-seconds> <script.wls> [args...]" >&2
    exit 2
fi

# Default heap: 512M cells (4GB).  Big enough for the easy ATP
# theorems + Vampire-SZS reparse without overshooting on tight
# machines.  Override via env for the deeper Sheffer-class probes.
export THVM_HEAP_CELLS="${THVM_HEAP_CELLS:-536870912}"

# exec replaces the script's bash process with `timeout`, so the
# returned exit code is the actual wolframscript exit (or 124 on
# overrun).  Nothing for the caller to clean up beyond what
# `timeout` already terminates.
exec timeout "$1" wolframscript -file "${@:2}"
