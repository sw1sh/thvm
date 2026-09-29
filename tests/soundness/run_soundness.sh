#!/bin/bash
# Soundness regression: these TPTP problems are all Satisfiable, so a
# refutation (SZS Unsatisfiable / Theorem) is UNSOUND.  FindProof must
# report GaveUp / Satisfiable / ResourceOut -- never a refutation.
# Reported by Geoff Sutcliffe (CASC J13 soundness testing, 2026-07-17);
# root causes fixed in c68b1bd7.  edge_include_select.p is satisfiable
# only when its include() selection is honoured.  curry_wide*_dc.p are
# satisfiable only in a model too small to host a wide functor's curried
# partial applications; wide_skolem17_pp.p only when its 17-ary Skolem
# term keeps its variables.  Run from the repo root after `make`.
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
BIN=${1:-$(cd "$HERE/../.." && pwd)/bin/FindProof}
export TPTP="$HERE" THVM_ATP_RSS_ABORT_MB=6000 THVM_ATP_SCHED_MAX_PAR=29
fail=0
for f in "$HERE"/*.p; do
  name=$(basename "$f" .p)
  st=$(timeout 90 "$BIN" --schedule --wall=20 --szs "$f" 2>/dev/null | grep -m1 -oE 'SZS status [A-Za-z]+' | awk '{print $3}')
  if [ "$st" = "Unsatisfiable" ] || [ "$st" = "Theorem" ]; then
    echo "  UNSOUND: $name -> $st (this problem is Satisfiable)"; fail=1
  else
    echo "  ok: $name -> ${st:-GaveUp}"
  fi
done
if [ "$fail" = 0 ]; then echo "SOUNDNESS OK: no Satisfiable problem refuted"; else echo "SOUNDNESS FAILED"; exit 1; fi
