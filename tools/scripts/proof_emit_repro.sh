#!/bin/bash
# Reproduce the 18 empty-proof CASC-J13 UEQ offenders under --schedule
# --proof and tabulate the winning lane + emitted-proof status.
# Serial, CPU-only.  Artifacts under ~/.cache/thvm/casc/J13/proof_emit/.
set -u
BIN="$1"; shift
OUT="$1"; shift
WALL="${WALL:-240}"
mkdir -p "$OUT"
export TPTP="$HOME/.cache/thvm/casc/J13"
export THVM_ATP_RSS_ABORT_MB="${THVM_ATP_RSS_ABORT_MB:-2500}"
for p in "$@"; do
  f="$TPTP/UEQ/$p.p"
  t0=$(date +%s)
  "$BIN" --schedule --wall="$WALL" --szs --proof "$f" \
      >"$OUT/$p.out" 2>"$OUT/$p.err"
  rc=$?
  t1=$(date +%s)
  winner=$(grep -o 'schedule winner: .*' "$OUT/$p.err" | tail -1 | cut -d' ' -f3)
  status=$(grep -o 'SZS status [A-Za-z]*' "$OUT/$p.out" | head -1 | awk '{print $3}')
  ncnf=$(grep -c '^cnf(' "$OUT/$p.out")
  marker=$(grep -c 'no single-NF chain extractable' "$OUT/$p.out")
  echo "$p rc=$rc t=$((t1-t0))s status=${status:-none} winner=${winner:-none} cnf=$ncnf marker=$marker"
done
