#!/bin/bash
# kernel_reaper.sh -- standing safety net against runaway WolframKernels.
#
# Every POLL_S seconds:
#   * kill -9 any NON-WSTP WolframKernel above its class cap:
#       - script-mode kernels (the wolframscript -runfirst
#         EvaluationEnvironment="Script" signature): SCRIPT_CAP_MB
#       - any other non-wstp kernel (e.g. `WolframKernel -noprompt`
#         spawned by the `wl` wrapper / enigma measure runs):
#         OTHER_CAP_MB
#   * NEVER kill a -wstp kernel (the user's notebook front end + MCP
#     server sessions); log a loud warning when one exceeds
#     OTHER_CAP_MB so a runaway MCP evaluation is at least visible.
#
# This complements tools/wolframscript_guarded.sh: the guard caps the
# kernels it spawns, but a kernel escapes the guard when (a) an agent
# runs wolframscript or the `wl` wrapper directly, or (b) the guard's
# watchdog dies with its parent (rate-limit kill, TaskStop) while the
# kernel lives on.  Uncapped kernels have reached 100GB+ RSS and
# forced reboots four times; the reaper is the always-on backstop.
#
# Usage:
#   tools/kernel_reaper.sh [script_cap_mb] [other_cap_mb] [poll_s]
# Defaults: script 8192 MB, other 16384 MB, poll 10 s.

set -u
SCRIPT_CAP_MB=${1:-8192}
OTHER_CAP_MB=${2:-16384}
POLL_S=${3:-10}
SCRIPT_CAP_KB=$((SCRIPT_CAP_MB * 1024))
OTHER_CAP_KB=$((OTHER_CAP_MB * 1024))

echo "[reaper] caps: script=${SCRIPT_CAP_MB}MB other-nonwstp=${OTHER_CAP_MB}MB poll=${POLL_S}s"

while true; do
    ps -axo pid=,rss=,command= | grep '[W]olframKernel' \
        | while read -r pid rss cmd; do
        case "$cmd" in
            *-wstp*)
                if [ "$rss" -gt "$OTHER_CAP_KB" ]; then
                    echo "[reaper] $(date '+%H:%M:%S') WARNING: user WSTP kernel pid=$pid at $((rss / 1024))MB (not killing; check the MCP/notebook session)"
                fi
                ;;
            *'EvaluationEnvironment="Script"'*)
                if [ "$rss" -gt "$SCRIPT_CAP_KB" ]; then
                    echo "[reaper] $(date '+%H:%M:%S') killing script kernel pid=$pid rss=$((rss / 1024))MB (cap ${SCRIPT_CAP_MB}MB)"
                    kill -9 "$pid" 2>/dev/null
                fi
                ;;
            *)
                if [ "$rss" -gt "$OTHER_CAP_KB" ]; then
                    echo "[reaper] $(date '+%H:%M:%S') killing non-wstp kernel pid=$pid rss=$((rss / 1024))MB (cap ${OTHER_CAP_MB}MB): ${cmd:0:120}"
                    kill -9 "$pid" 2>/dev/null
                fi
                ;;
        esac
    done
    sleep "$POLL_S"
done
