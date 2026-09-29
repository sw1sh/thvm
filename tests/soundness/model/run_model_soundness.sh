#!/bin/bash
# One-element model claims: every trap_*.p is Unsatisfiable, so a
# Satisfiable / CounterSatisfiable answer is UNSOUND; every sat_*.p must be
# answered with the status its header names and a FiniteModel that
# tools/casc/model_check.py verifies.  Run from the repo root after `make`
# (on a pod: the CLI runs the engine).
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../../.." && pwd)
BIN=${1:-$ROOT/bin/FindProof}
export TPTP="$HERE" THVM_ATP_SCHED_MAX_PAR=${THVM_ATP_SCHED_MAX_PAR:-16}
OUT=$(mktemp -d "${TMPDIR:-/tmp}/model_sound.XXXXXX")
trap 'rm -rf "$OUT"' EXIT
fail=0
for f in "$HERE"/*.p; do
  name=$(basename "$f" .p)
  timeout 60 "$BIN" --schedule --wall=20 --szs --proof "$f" > "$OUT/$name.out" 2>/dev/null < /dev/null
  st=$(grep -m1 -oE 'SZS status [A-Za-z]+' "$OUT/$name.out" | awk '{print $3}')
  case $name in
    trap_*)
      case $st in
        Satisfiable|CounterSatisfiable) echo "  UNSOUND: $name -> $st"; fail=1 ;;
        *) echo "  ok: $name -> ${st:-no status}" ;;
      esac ;;
    sat_*)
      want=Satisfiable; grep -q ',conjecture,' "$f" && want=CounterSatisfiable
      chk=$(python3 "$ROOT/tools/casc/model_check.py" "$f" "$OUT/$name.out" --tptp "$HERE" 2>&1 | tail -1)
      if [ "$st" = "$want" ] && [ "${chk%% *}" = VERIFIED ]; then
        echo "  ok: $name -> $st, $chk"
      else
        echo "  FAIL: $name -> ${st:-no status} (want $want), $chk"; fail=1
      fi ;;
  esac
done
if [ "$fail" = 0 ]; then echo "MODEL SOUNDNESS OK"; else echo "MODEL SOUNDNESS FAILED"; exit 1; fi
