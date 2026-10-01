#!/bin/bash
# The synthetic -10 corpus on a pod, in the order of README.md's section
# "Synthetic -10 corpus" and gate G2.6 (plan section 5.4), each step with
# its own deadline, stopping at the first failure:
#   unit       make bin/test_atp_ifeq10 && bin/test_atp_ifeq10
#   diff       tests/atp_ifeq10_diff.sh (ifeq10_diff.py, ifeq10_keys.py)
#   tptp       ALL=1 tests/atp_ifeq10_tptp.sh bin/FindProof: writes
#              bin/FindProof.ifeq10-pass
#   driver     tests/atp_ifeq10_corpus.sh bin/FindProof
#   corpus     tools/atp_enigma/make_ifeq10_corpus.sh (CORPUS=1 only)
# The builds of FindProof (gcc, clang 14, static) and G2.3 (test_atp, the
# OA and DN pins) are the lane matrix's pod_gates.sh; bin/FindProof must
# exist.  Run from the repository root.
#
# Usage: tools/atp_enigma/ifeq10_pod.sh TPTP_ROOT INPUT_DIR OUT_DIR
#
# INPUT_DIR holds the files copied from the Mac, checked against the
# sha256 in INPUT_DIR/SHA256SUMS before anything runs (sha256sum -c):
#   j13_ueq_names.txt  ~/.cache/thvm/casc/J13/neural_p0/j13_ueq_names.txt
#   pool.tsv           ~/.cache/thvm/casc/J13/neural_p0/pool.tsv
#   dev_t.tsv          ~/.cache/thvm/enigma/matrix/dev_t.tsv
# (their sha256 on 2026-10-02 are in README.md).  TPTP_ROOT is the
# TPTP-v9.3.1 tree, which the driver fingerprints itself.  Sentinels in
# OUT_DIR/sentinels: STEP.started, STEP.done (ok or FAIL rc=N) and
# ALL_DONE; logs in OUT_DIR/logs.  Env: JOBS, DEADLINE_S (passed to the
# ALL=1 survey and the corpus, 14400 each), CORPUS=1.
set -u
if [ $# -ne 3 ]; then
  awk 'NR == 1 { next } /^#/ { sub(/^# ?/, ""); print; next } { exit }' "$0"
  exit 2
fi
ROOT=$(pwd)
TPTP_ROOT=$(cd "$1" && pwd) || exit 2
IN=$(cd "$2" && pwd) || exit 2
mkdir -p "$3/sentinels" "$3/logs" || exit 2
OUT=$(cd "$3" && pwd)
S=$OUT/sentinels
DEADLINE_S=${DEADLINE_S:-14400}
export JOBS=${JOBS:-$(getconf _NPROCESSORS_ONLN 2> /dev/null || echo 8)}

[ -x "$ROOT/bin/FindProof" ] || { echo "ifeq10_pod: no bin/FindProof under $ROOT" >&2; exit 2; }
[ -f "$ROOT/tests/atp_ifeq10_tptp.sh" ] || { echo "ifeq10_pod: run from the repository root" >&2; exit 2; }
for f in j13_ueq_names.txt pool.tsv dev_t.tsv SHA256SUMS; do
  [ -s "$IN/$f" ] || { echo "ifeq10_pod: $IN/$f is missing" >&2; exit 2; }
done
if ! (cd "$IN" && sha256sum -c --quiet SHA256SUMS) > "$OUT/logs/inputs.log" 2>&1; then
  echo "ifeq10_pod: the inputs do not match SHA256SUMS:" >&2
  cat "$OUT/logs/inputs.log" >&2
  exit 2
fi

# step NAME SECONDS CMD...: runs CMD under timeout, logs to logs/NAME.log.
step() {
  local name=$1 secs=$2
  shift 2
  echo "$(date -u +%FT%TZ)" > "$S/$name.started"
  echo "=== $(date -u +%FT%TZ) $name"
  timeout -k 30 "$secs" "$@" < /dev/null > "$OUT/logs/$name.log" 2>&1
  local rc=$?
  tail -n 5 "$OUT/logs/$name.log"
  if [ $rc -ne 0 ]; then
    echo "$(date -u +%FT%TZ) FAIL rc=$rc" > "$S/$name.done"
    echo "$(date -u +%FT%TZ) stopped at $name" > "$S/ALL_DONE"
    exit 1
  fi
  echo "$(date -u +%FT%TZ) ok" > "$S/$name.done"
}

step unit 1800 bash -c 'make bin/test_atp_ifeq10 && bin/test_atp_ifeq10'
step diff 300 bash tests/atp_ifeq10_diff.sh
step tptp $((DEADLINE_S + 1800)) env TPTP="$TPTP_ROOT" ALL=1 J13_NAMES="$IN/j13_ueq_names.txt" \
  DEADLINE_S="$DEADLINE_S" bash tests/atp_ifeq10_tptp.sh bin/FindProof
step driver 3600 bash tests/atp_ifeq10_corpus.sh bin/FindProof
if [ "${CORPUS:-0}" = 1 ]; then
  awk -F'\t' '$5 == "heldout" { print $1 }' "$IN/pool.tsv" > "$OUT/devh_names.txt"
  awk -F'\t' '$5 == "train" { print $1 }' "$IN/pool.tsv" > "$OUT/pool_train_names.txt"
  step corpus $((DEADLINE_S + 3600)) env TRAIN_NAMES="$OUT/pool_train_names.txt" DEADLINE_S="$DEADLINE_S" \
    bash tools/atp_enigma/make_ifeq10_corpus.sh bin/FindProof "$TPTP_ROOT" "$OUT/corpus" \
    "$IN/j13_ueq_names.txt" "$OUT/devh_names.txt" "$IN/dev_t.tsv"
  # The driver exits 0 on an incomplete or alerted run; its .done says which.
  cat "$OUT/corpus/.done" >> "$S/corpus.done"
fi
echo "$(date -u +%FT%TZ) ok" > "$S/ALL_DONE"
