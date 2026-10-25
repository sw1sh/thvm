#!/bin/bash
# The selection layer's phi(p) seam (docs/ATP/guide.md): a FindProof-shaped
# run whose head reads phi(p) dumps x[20 .. 20 + Phi) equal to the
# problem's FindProof --problem-features row, bit for bit as float32, on
# lanes whose front ends differ (phi(p) reads the default configuration
# whatever the lane's flags are).  Runs on a pod, never on the Mac.
#
# Every listed row runs
#   FindProof --problem-features --header p
#   env -i PATH HOME TPTP THVM_ATP_GUIDE=L THVM_ATP_GUIDE_ZERO=1
#     THVM_ATP_GUIDE_MODEL=$MODEL THVM_ATP_GUIDE_DUMP=... (sampled)
#     FindProof --lane=L --szs --proof --wall=$WALL p
# and reads back the dump: every session's header must say phi=40 and
# every row's phi columns must equal the row --problem-features printed.
# A row whose lane exits before the engine (an exit-fast gate) dumps no
# session and is NOENGINE, which does not count.  Two fixed rows check
# the other side of the seam on a .pr input, which has no phi(p): with
# MODEL the layer stays off ("guide: no phi(p) for this input") and the
# run still ends with its SZS line; with the default head, which reads no
# phi(p), the layer arms as before and computes nothing.
#
# Usage: tests/atp_guide_phi.sh <FindProof> <TPTP_root> <list.tsv> <outdir>
#   list.tsv: lane TAB problem path relative to <TPTP_root> ('#' comments)
#   MODEL (harvest), WALL (60), MIN_PASS (8: rows that must PASS).
# Writes <outdir>/phi.tsv (one row per list row) and prints
# "GUIDE PHI: P pass, F fail, N noengine"; exits 1 on any FAIL or fewer
# than MIN_PASS PASS rows.
set -u
HERE=$(cd "$(dirname "$0")/.." && pwd)
BIN=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
ROOT=$(cd "$2" && pwd)
LIST=$3
OUT=$4
MODEL=${MODEL:-harvest}
WALL=${WALL:-60}
MIN_PASS=${MIN_PASS:-8}
PR=$HERE/tools/baselines/wm_pr/WolframAxioms__DoubleNegation.pr
mkdir -p "$OUT"
: > "$OUT/phi.tsv"
n_pass=0
n_fail=0
n_none=0

# check DUMP PHI_ROW_FILE: PASS / FAIL <why> / NOENGINE.
check() {
  python3 - "$1" "$2" <<'PY'
import sys
import numpy as np
dump, phi_path = sys.argv[1], sys.argv[2]
lines = [l.rstrip("\n").split("\t") for l in open(phi_path) if l.strip()]
hdr = lines[0]
row = lines[1]
n_phi = sum(1 for c in hdr[1:] if not c.startswith("meta_"))
want = np.array(row[1:1 + n_phi], dtype=np.float32)
sessions = bad = rows = 0
why = []
try:
    fp = open(dump)
except OSError:
    print("NOENGINE no dump")
    sys.exit()
for line in fp:
    if line.startswith("# guide end"):
        continue
    if line.startswith("#"):
        sessions += 1
        kv = dict(t.split("=", 1) for t in line[1:].split() if "=" in t)
        if kv.get("phi") != str(n_phi):
            why.append(f"header phi={kv.get('phi')} (want {n_phi})")
        continue
    f = line.rstrip("\n").split("\t")
    x = np.array([float(v) for v in f[4 + 20:4 + 20 + n_phi]], dtype=np.float64)
    rows += 1
    if len(x) != n_phi or not np.array_equal(x, want.astype(np.float64)):
        bad += 1
if sessions == 0:
    print("NOENGINE no dump session")
elif rows == 0:
    print("NOENGINE no dumped pick")
elif bad or why:
    print(f"FAIL {bad} of {rows} rows differ from the phi(p) row; " + "; ".join(why))
else:
    print(f"PASS {rows} rows over {sessions} sessions equal the {n_phi}-column phi(p) row")
PY
}

while IFS=$'\t' read -r lane rel; do
  case "$lane" in
    ''|'#'*) continue ;;
  esac
  nm=$(basename "$rel" .p)
  d=$OUT/$lane/$nm
  rm -rf "$d"
  mkdir -p "$d"
  env -i PATH="$PATH" HOME="${HOME:-/tmp}" TPTP="$ROOT" \
    "$BIN" --problem-features --header "$ROOT/$rel" > "$d/phi.row" 2> "$d/phi.err" < /dev/null
  if [ "$(wc -l < "$d/phi.row" | tr -d ' ')" != 2 ]; then
    res="FAIL --problem-features printed no row: $(head -c 200 "$d/phi.err")"
  else
    (cd "$d" && env -i PATH="$PATH" HOME="${HOME:-/tmp}" TPTP="$ROOT" \
       THVM_ATP_GUIDE="$lane" THVM_ATP_GUIDE_ZERO=1 THVM_ATP_GUIDE_MODEL="$MODEL" \
       THVM_ATP_GUIDE_DUMP="$d/dump.tsv" THVM_ATP_GUIDE_DUMP_EVERY=5 THVM_ATP_GUIDE_DUMP_MAX=20 \
       timeout -k 5 $((WALL + 30)) "$BIN" --lane="$lane" --szs --proof --wall="$WALL" "$ROOT/$rel" \
       < /dev/null > "$d/out" 2> "$d/err")
    res=$(check "$d/dump.tsv" "$d/phi.row")
    if grep -q '^guide: ' "$d/err"; then
      res="FAIL the layer went off: $(grep -m1 '^guide: ' "$d/err")"
    fi
  fi
  case $res in
    PASS*) n_pass=$((n_pass + 1)) ;;
    NOENGINE*) n_none=$((n_none + 1)) ;;
    *) n_fail=$((n_fail + 1)) ;;
  esac
  printf '%s\t%s\t%s\n' "$lane" "$nm" "$res" | tee -a "$OUT/phi.tsv"
done < "$LIST"

# The other side of the seam, on a .pr input (no phi(p)).
d=$OUT/pr
rm -rf "$d"
mkdir -p "$d"
env -i PATH="$PATH" HOME="${HOME:-/tmp}" THVM_ATP_GUIDE=wm THVM_ATP_GUIDE_ZERO=1 \
  THVM_ATP_GUIDE_MODEL="$MODEL" timeout -k 5 $((WALL + 30)) "$BIN" --lane=wm --szs --wall="$WALL" "$PR" \
  < /dev/null > "$d/off.out" 2> "$d/off.err"
if grep -q '^guide: no phi(p) for this input; selection layer off' "$d/off.err" &&
   ! grep -q '^% guide: ' "$d/off.err" && grep -q 'SZS status' "$d/off.out"; then
  res="PASS a head that reads phi(p) stays off on a .pr input, and the run ends: $(grep -m1 'SZS status' "$d/off.out")"
  n_pass=$((n_pass + 1))
else
  res="FAIL on a .pr input the layer of a head reading phi(p): $(grep -m1 -e '^guide: ' -e '^% guide: ' "$d/off.err" | cut -c1-160)"
  n_fail=$((n_fail + 1))
fi
printf 'wm\tpr-%s\t%s\n' "$MODEL" "$res" | tee -a "$OUT/phi.tsv"
env -i PATH="$PATH" HOME="${HOME:-/tmp}" THVM_ATP_GUIDE=wm THVM_ATP_GUIDE_ZERO=1 \
  timeout -k 5 $((WALL + 30)) "$BIN" --lane=wm --szs --wall="$WALL" "$PR" \
  < /dev/null > "$d/on.out" 2> "$d/on.err"
gl=$(grep -m1 '^% guide: ' "$d/on.err")
picks=$(echo "$gl" | sed -n -E 's/.* picks=([0-9]+) .*/\1/p')
if [ "${picks:-0}" -gt 0 ] && ! grep -q '^guide: ' "$d/on.err" && ! grep -q 'problem features' "$d/on.err"; then
  res="PASS the default head (no phi(p)) arms on a .pr input: picks=$picks"
  n_pass=$((n_pass + 1))
else
  res="FAIL the default head on a .pr input: '${gl:-no % guide: line}' $(grep -m1 -e '^guide: ' -e 'problem features' "$d/on.err" | cut -c1-160)"
  n_fail=$((n_fail + 1))
fi
printf 'wm\tpr-default\t%s\n' "$res" | tee -a "$OUT/phi.tsv"

echo "GUIDE PHI: $n_pass pass, $n_fail fail, $n_none noengine (runs in $OUT)"
if [ "$n_fail" != 0 ] || [ "$n_pass" -lt "$MIN_PASS" ]; then
  exit 1
fi
