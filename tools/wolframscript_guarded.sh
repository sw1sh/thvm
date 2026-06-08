#!/bin/bash
# wolframscript_guarded.sh -- spawn a wolframscript with a real memory guard.
#
# Why: WolframKernel reserves ~440GB VSZ on macOS, and Darwin VM accounting
# pressures the OS even at low RSS.  A diverging TFindProof on a hard
# AxiomaticTheory NotableTheorem (McCune, AbelianMcCune, deep Sheffer,
# Robbins-class) eats heap until heap_alloc aborts (typically 48s+) -- by
# which point the box is already in trouble.  Plain `timeout 30
# wolframscript` only catches wall-clock; it doesn't enforce RSS.
#
# What this script does:
#   1. Spawns the .wls file with a hard wall timeout (default 60s).
#   2. Watchdogs the spawned WolframKernel child every 2s.
#   3. SIGKILLs the kernel (NOT pkill -- only the spawned PID) when RSS
#      exceeds the cap (default 2GB).
#   4. Returns the wolframscript exit code; nonzero on kill.
#
# Usage:
#   tools/wolframscript_guarded.sh [-w WALL] [-m RSS_MB] script.wls [args...]
#
#   -w WALL    wall-clock seconds (default 60)
#   -m RSS_MB  RSS cap in megabytes for the spawned WolframKernel (default 2000)
#
# Defaults are conservative.  Override only if you know the case fits.
# Never raise the RSS cap above 4000 without explicit user confirmation.

set -u

WALL=60
RSS_MB=2000

while getopts "w:m:" opt; do
  case $opt in
    w) WALL=$OPTARG ;;
    m) RSS_MB=$OPTARG ;;
    *) echo "usage: $0 [-w WALL] [-m RSS_MB] script.wls [args...]" >&2; exit 2 ;;
  esac
done
shift $((OPTIND - 1))

if [ $# -lt 1 ]; then
  echo "usage: $0 [-w WALL] [-m RSS_MB] script.wls [args...]" >&2
  exit 2
fi

SCRIPT="$1"
shift

# Refuse if any non-trivial WolframKernel is already running.  Each
# WolframKernel pre-reserves ~443GB VSZ on macOS (built into the binary,
# independent of thvm), and accumulated kernels strain Darwin VM
# accounting even at low RSS.  An interactive notebook / MCP server
# kernel typically sits at ~50MB; an active ATP one runs hundreds of MB.
# Cap: refuse if >1 alive (one is plausibly user notebook/MCP), or any
# kernel above 500MB RSS (probably leftover from a prior crash).
STALE_KERNS=$(ps -axo pid,rss,command | awk '/WolframKernel/ && !/awk/ {print $1":"$2}')
if [ -n "$STALE_KERNS" ]; then
  N_KERNS=$(echo "$STALE_KERNS" | wc -l | tr -d ' ')
  BIG_KERNS=$(echo "$STALE_KERNS" | awk -F: '$2 > 512000 {print $1}' | tr '\n' ' ')
  if [ "$N_KERNS" -gt 1 ] || [ -n "$BIG_KERNS" ]; then
    echo "[guard] REFUSING SPAWN: $N_KERNS WolframKernel(s) already alive" >&2
    echo "        $STALE_KERNS" >&2
    if [ -n "$BIG_KERNS" ]; then
      echo "        big kernels (>500MB RSS): $BIG_KERNS" >&2
      echo "        kill those manually (`kill -9 ...`) then retry" >&2
    fi
    exit 3
  fi
fi

# Spawn wolframscript in background with the wall-clock timeout as a
# hard outer bound.  The watchdog enforces RSS independently.
( exec timeout "$WALL" wolframscript -script "$SCRIPT" "$@" ) &
WS_PID=$!

# Watchdog loop.  Every 2 seconds:
#   - Check the wolframscript wrapper still exists.
#   - Find its WolframKernel CHILD via pgrep -P.
#   - Read the child's RSS in KB.
#   - If RSS > cap, SIGKILL the child + the wrapper.  WolframKernel
#     doesn't respond to SIGTERM cleanly under heap pressure, so SIGKILL.
RSS_CAP_KB=$((RSS_MB * 1024))
KILLED=0

# Recursive descendant lookup: find any WolframKernel under the WS_PID
# tree.  wolframscript -> timeout -> WolframKernel is N levels deep on
# macOS depending on the wrapper used, so single pgrep -P misses it.
descendants() {
  local pid=$1 child
  for child in $(pgrep -P "$pid" 2>/dev/null); do
    echo "$child"
    descendants "$child"
  done
}

# Snapshot the WolframKernel PIDs alive at spawn time so we DON'T kill
# any pre-existing kernel (the user's notebook, MCP server, etc).
PRE_KERNS=$(ps -axo pid,command | awk '/WolframKernel/ && !/awk/ {print $1}' | tr '\n' '|' | sed 's/|$//')

while kill -0 "$WS_PID" 2>/dev/null; do
  sleep 0.5
  # Find the WolframKernel under the WS subtree that ISN'T in PRE_KERNS.
  KERN_PID=$(descendants "$WS_PID" | while read p; do
    if ps -o command= -p "$p" 2>/dev/null | grep -q WolframKernel; then
      if [ -z "$PRE_KERNS" ] || ! echo "$p" | grep -qE "^(${PRE_KERNS})$"; then
        echo "$p"; break
      fi
    fi
  done | head -1)
  # macOS workaround: if pgrep -P traversal doesn't find it, fall back to
  # ANY WolframKernel born AFTER guard start that isn't in PRE_KERNS.
  if [ -z "$KERN_PID" ]; then
    KERN_PID=$(ps -axo pid,command | awk '/WolframKernel/ && !/awk/ {print $1}' \
      | while read p; do
          if [ -z "$PRE_KERNS" ] || ! echo "$p" | grep -qE "^(${PRE_KERNS})$"; then
            echo "$p"; break
          fi
        done | head -1)
  fi
  if [ -n "$KERN_PID" ]; then
    RSS_KB=$(ps -o rss= -p "$KERN_PID" 2>/dev/null | tr -d ' ')
    if [ -n "$RSS_KB" ] && [ "$RSS_KB" -gt "$RSS_CAP_KB" ]; then
      echo "[guard] WolframKernel pid=$KERN_PID RSS=${RSS_KB}KB > cap=${RSS_CAP_KB}KB -- SIGKILL" >&2
      kill -KILL "$KERN_PID" 2>/dev/null
      kill -KILL "$WS_PID" 2>/dev/null
      KILLED=1
      break
    fi
  fi
done

wait "$WS_PID" 2>/dev/null
RC=$?
if [ "$KILLED" -eq 1 ]; then
  echo "[guard] killed on RSS cap; exit code overridden to 137" >&2
  exit 137
fi
exit "$RC"
