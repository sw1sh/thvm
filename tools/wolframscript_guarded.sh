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

# Refuse if any individual WolframKernel is over 500MB RSS -- that's
# the actual danger pattern (one runaway ATP kernel diverging).  The
# OS-crash incidents the original guard was built around (see
# [[feedback_wolframscript_oom_risk]]) were caused by a SINGLE
# diverging kernel eating heap until heap_alloc exit(1)'d -- not by
# the count of small kernels alive.  Concurrent agents / MCP / a user
# notebook can all coexist at low RSS without strain.  After the
# heap_alloc longjmp-recovery fix (ebfc1742, thvm_fatal -> longjmp
# back to the WL bridge instead of exit(1)), even a diverging kernel
# survives -- so this gate is now a soft-cap on the per-kernel
# footprint, not a hard count.  The watchdog below still SIGKILLs a
# spawned kernel that exceeds RSS_CAP mid-run.
STALE_KERNS=$(ps -axo pid,rss,command | awk '/WolframKernel/ && !/awk/ {print $1":"$2}')
if [ -n "$STALE_KERNS" ]; then
  BIG_KERNS=$(echo "$STALE_KERNS" | awk -F: '$2 > 512000 {print $1}' | tr '\n' ' ')
  if [ -n "$BIG_KERNS" ]; then
    echo "[guard] REFUSING SPAWN: WolframKernel(s) over 500MB RSS already alive" >&2
    echo "        $STALE_KERNS" >&2
    echo "        big kernels (>500MB RSS): $BIG_KERNS" >&2
    echo "        kill those manually (kill -9 $BIG_KERNS) then retry" >&2
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

# Post-wait reap.  Even on a clean wolframscript exit, the WolframKernel
# child can survive (the wrapper exits before sending the kernel a
# disconnect over its SharedMemory mathlink).  Per
# [[feedback_wolframscript_kill_child]], an orphan ATP kernel sits at
# hundreds-of-MB-to-multi-GB until manually killed -- the guard's
# next-spawn refusal then blocks legitimate work.  Hunt for any
# WolframKernel that (a) isn't in the PRE_KERNS snapshot AND (b) is
# specifically a wolframscript-spawned one (script-mode signature), and
# SIGKILL each.  Skips the user's notebook/MCP kernels (they wear the
# -wstp -linkprotocol signature, not the -runfirst Unprotect signature).
LEFTOVER_KERNS=$(ps -axo pid,command | awk '
  /WolframKernel/ && /-runfirst Unprotect/ && !/awk/ { print $1 }
')
if [ -n "$LEFTOVER_KERNS" ]; then
  for p in $LEFTOVER_KERNS; do
    if [ -z "$PRE_KERNS" ] || ! echo "$p" | grep -qE "^(${PRE_KERNS})$"; then
      kill -KILL "$p" 2>/dev/null && \
        echo "[guard] post-exit reap: killed orphan script-mode WolframKernel pid=$p" >&2
    fi
  done
fi

if [ "$KILLED" -eq 1 ]; then
  echo "[guard] killed on RSS cap; exit code overridden to 137" >&2
  exit 137
fi
exit "$RC"
