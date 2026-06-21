#!/usr/bin/env bash
# flux_watch.sh -- run a wolframscript with a REAL-TIME memory watchdog.
#
# The failure this fixes: a leaking/hung FLUX run never exits, so a
# post-exit "reap" never fires and memory climbs until the box bricks.
# This polls available memory every second and HARD-KILLS the run + its
# WolframKernel children the instant available memory drops below FLOOR_GB
# (or after TIMEOUT). The watchdog is the leak detector -- not the human.
#
# Usage: flux_watch.sh <script.wls> [FLOOR_GB=8] [TIMEOUT_S=400]
# Exit:  0 = run finished on its own; 99 = watchdog killed it (leak/hang).
set -u
SCRIPT="${1:?usage: flux_watch.sh <script.wls> [FLOOR_GB] [TIMEOUT_S]}"
FLOOR_GB="${2:-8}"
TIMEOUT_S="${3:-400}"
PAGE=$(vm_stat | awk '/page size/{print $8}')
PAGE="${PAGE:-16384}"
FLOOR_PAGES=$(( FLOOR_GB * 1073741824 / PAGE ))

avail_pages() {  # free + inactive + speculative + purgeable = reclaimable/available
  vm_stat | awk '
    /Pages free/{f=$3} /Pages inactive/{i=$4} /Pages speculative/{s=$3} /Pages purgeable/{p=$3}
    END{gsub(/\./,"",f);gsub(/\./,"",i);gsub(/\./,"",s);gsub(/\./,"",p); print f+i+s+p}'
}

BEFORE=$(pgrep -f 'WolframKernel.*-wstp' | sort)

reap_new() {  # kill ONLY kernels spawned by this run (preserve the user's idle kernel)
  local after new
  after=$(pgrep -f 'WolframKernel.*-wstp' | sort)
  new=$(comm -13 <(echo "$BEFORE") <(echo "$after"))
  [ -n "$new" ] && { echo "$new" | xargs -r kill -9; echo "WATCHDOG: reaped kernels: $(echo $new)"; }
}

wl -f "$SCRIPT" & WLPID=$!
start=$SECONDS rc=0
while kill -0 "$WLPID" 2>/dev/null; do
  a=$(avail_pages)
  if [ "${a:-0}" -lt "$FLOOR_PAGES" ]; then
    echo "WATCHDOG: available memory < ${FLOOR_GB}GB ($(( a * PAGE / 1073741824 ))GB) -- KILLING run"
    kill -9 "$WLPID" 2>/dev/null; reap_new; rc=99; break
  fi
  if [ $(( SECONDS - start )) -gt "$TIMEOUT_S" ]; then
    echo "WATCHDOG: timeout ${TIMEOUT_S}s -- KILLING run"
    kill -9 "$WLPID" 2>/dev/null; reap_new; rc=99; break
  fi
  sleep 1
done
wait "$WLPID" 2>/dev/null
[ "$rc" -eq 0 ] && reap_new   # also reap on a clean finish (kernel can outlive the wrapper)
echo "WATCHDOG: done (rc=$rc); available now $(( $(avail_pages) * PAGE / 1073741824 ))GB"
exit "$rc"
