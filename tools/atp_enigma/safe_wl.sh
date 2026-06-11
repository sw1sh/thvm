#!/bin/bash
# safe_wl.sh RSS_CAP_MB -- <wl-command...>
# Runs a `wl` job and KILLS it the moment its WolframKernel exceeds RSS_CAP_MB.
# `wl -t` bounds time; this bounds MEMORY, so a pass self-limits well below the
# system reaper's threshold instead of relying on it. Snapshots pre-existing
# `WolframKernel -noprompt` kernels so it only ever kills THIS run's kernel
# (never the user's -wstp/MCP kernels, never another pass's). Poll 2s.
CAP=$1; shift
kpids() { pgrep -f "WolframKernel -noprompt" 2>/dev/null; }
before=" $(kpids | tr '\n' ' ') "
"$@" & wpid=$!
tripped=""
while kill -0 "$wpid" 2>/dev/null; do
  for k in $(kpids); do
    case "$before" in *" $k "*) continue;; esac
    rss=$(ps -o rss= -p "$k" 2>/dev/null | awk '{print int($1/1024)}')
    if [ "${rss:-0}" -gt "$CAP" ]; then
      echo "[safe_wl] RSS LEASH: kernel $k ${rss}MB > ${CAP}MB -> kill run" >&2
      kill -9 "$k" 2>/dev/null; kill -9 "$wpid" 2>/dev/null; tripped=1; break 2
    fi
  done
  sleep 2
done
wait "$wpid" 2>/dev/null; rc=$?
# reap any run-spawned kernel that outlived the wrapper
for k in $(kpids); do case "$before" in *" $k "*) ;; *) kill -9 "$k" 2>/dev/null;; esac; done
[ -n "$tripped" ] && exit 137 || exit "$rc"
