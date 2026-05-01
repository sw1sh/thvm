#!/usr/bin/env bash
# wl/Examples/_hvmbench/run.sh -- side-by-side cnot_NN comparison.
#
# Reproduces the HigherOrderCO/bench cnot_04 / cnot_16 cases on
# both HVM4 (interpreted) and thvm (interpreted), with matched
# interaction counts.  Runs each five times and prints best / mean
# wall time + Mitrs/s.
#
# Requires:
#   /tmp/hvm4                                            -- compiled HVM4 binary
#   /tmp/hvmbench/bench/cnot_{04,16}/main.hvm.forced     -- our forced variants

set -euo pipefail

cd "$(dirname "$0")/../../.."

HVM4_BIN=${HVM4_BIN:-/tmp/hvm4}
BENCH_DIR=${BENCH_DIR:-/tmp/hvmbench}

if [[ ! -x "$HVM4_BIN" ]]; then
  echo "HVM4 binary not found at $HVM4_BIN" >&2
  echo "Build with: clang -O3 -std=c11 \$HVM4_SRC/clang/main.c -o $HVM4_BIN" >&2
  exit 1
fi
if [[ ! -d "$BENCH_DIR" ]]; then
  echo "Bench dir not found at $BENCH_DIR" >&2
  echo "Clone with: gh repo clone HigherOrderCO/bench $BENCH_DIR" >&2
  exit 1
fi

# Build forced variants of the bench .hvm files (append two ERA
# args so HVM4's NF and thvm's WHNF do the same total work).
mkdir -p /tmp/hvmbench-forced
for n in 04 16; do
  src="$BENCH_DIR/bench/cnot_$n/main.hvm"
  dst="/tmp/hvmbench-forced/cnot_$n.hvm"
  sed 's|@main = @P\([0-9]*\)(\(.*\))|@main = @P\1(\2,*,*)|' "$src" > "$dst"
done

print_sep() { printf -- '----------------------------------------------------------------------\n'; }

run_hvm4() {
  local n=$1 file=/tmp/hvmbench-forced/cnot_$1.hvm
  local times=() itrs=
  for i in 1 2 3 4 5; do
    out=$("$HVM4_BIN" -s -S "$file" 2>&1)
    t=$(echo "$out" | awk '/^- Time:/ {print $3}')
    itrs=$(echo "$out" | awk '/^- Itrs:/ {print $3}')
    times+=("$t")
  done
  printf "  HVM4 (interpreted)  %s itrs   times(s) = %s\n" "$itrs" "${times[*]}"
}

run_thvm() {
  local n=$1
  echo "  thvm:"
  wolframscript -f wl/Examples/_hvmbench/cnot.wls "$n" --runs=5 \
    | grep -E "^\[thvm\]" | sed 's/^/    /'
}

for n in 04 16; do
  print_sep
  echo "cnot_$n  (P${n#0} cnot ctru * *)"
  print_sep
  run_hvm4 "$n"
  run_thvm "${n#0}"
done
print_sep
echo "Note: cnot_24 (16M iterations, 470M heap nodes) exceeds thvm's"
echo "HEAP_CAP = 1<<26 (64M cells).  HVM4 does it in ~2.5 s; thvm needs"
echo "a recompile with HEAP_CAP bumped to 1<<29 or higher to run it."
