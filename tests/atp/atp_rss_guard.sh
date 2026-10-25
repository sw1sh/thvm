#!/bin/zsh
# Run a command with an RSS watchdog: kill it if RSS exceeds CAP_MB.
# Usage: atp_rss_guard.sh CAP_MB -- cmd args...
cap_mb=$1
shift
[[ "$1" == "--" ]] && shift
"$@" &
pid=$!
while kill -0 $pid 2>/dev/null; do
  rss_kb=$(ps -o rss= -p $pid 2>/dev/null | tr -d ' ')
  if [[ -n "$rss_kb" && $((rss_kb/1024)) -gt $cap_mb ]]; then
    echo "[RSS GUARD] pid $pid RSS $((rss_kb/1024))MB > ${cap_mb}MB -- KILLING" >&2
    kill -9 $pid 2>/dev/null
    wait $pid 2>/dev/null
    exit 137
  fi
  sleep 0.5
done
wait $pid
