#!/bin/bash
# kernel_reaper.sh -- standing safety net against runaway WolframKernels.
#
# Every POLL_S seconds, kill -9 any SCRIPT-MODE WolframKernel whose RSS
# exceeds CAP_MB.  Script-mode kernels are identified by the
#   -runfirst ...$EvaluationEnvironment="Script"...
# launch signature that wolframscript stamps; WSTP / notebook / MCP
# kernels (the user's sessions) never match and are never touched.
#
# This complements tools/wolframscript_guarded.sh: the guard caps the
# kernels it spawns, but a kernel escapes the guard when (a) an agent
# runs wolframscript directly, or (b) the guard's watchdog dies with
# its parent (rate-limit kill, TaskStop) while the kernel lives on.
# The reaper catches both.  See memory: a single uncapped kernel has
# reached 100GB+ RSS and bricked the box three times.
#
# Usage:
#   tools/kernel_reaper.sh [cap_mb] [poll_s]     # foreground loop
# Defaults: cap 8192 MB, poll 30 s.  Log lines go to stdout.

set -u
CAP_MB=${1:-8192}
POLL_S=${2:-30}
CAP_KB=$((CAP_MB * 1024))

echo "[reaper] watching script-mode WolframKernels: cap=${CAP_MB}MB poll=${POLL_S}s"

while true; do
    # pid rss(args...) rows for script-mode kernels only.
    ps -axo pid=,rss=,command= | grep '[W]olframKernel' \
        | grep -F 'EvaluationEnvironment="Script"' \
        | while read -r pid rss _; do
        if [ "$rss" -gt "$CAP_KB" ]; then
            echo "[reaper] $(date '+%H:%M:%S') killing pid=$pid rss=$((rss / 1024))MB (cap ${CAP_MB}MB)"
            kill -9 "$pid" 2>/dev/null
        fi
    done
    sleep "$POLL_S"
done
