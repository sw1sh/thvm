#!/bin/bash
# tools/atp_enigma/make_ifeq10_corpus.sh on a fake TPTP-v9.3.1 tree with
# the real encoder: every skip rule and encoder outcome, the stem split,
# the pool tables, the corpus check, the J13 content-duplicate check, the
# alerts, the deadline and resume (a write outcome is retried, a timeout
# stays), and the refusals (changed inputs on the same OUT_DIR, a held
# lock, no pass file, another TPTP release, no exclusion list or
# TRAIN_NAMES without ALLOW_LEAKY=1, which marks the corpus unguarded).
# Then a fresh run with an exclusion list must leave the excluded stem
# out.  Pods only: it runs FindProof.
#
# Usage: tests/atp/atp_ifeq10_corpus.sh [bin/FindProof]
set -u
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BIN=${1:-$ROOT/bin/FindProof}
DRIVER=$ROOT/tools/atp_enigma/make_ifeq10_corpus.sh
if [ ! -x "$BIN" ]; then
  echo "  FAIL: $BIN is not an executable"
  exit 2
fi
for c in timeout flock python3; do
  if ! command -v "$c" > /dev/null 2>&1; then
    echo "  FAIL: no $c(1)"
    exit 2
  fi
done
BIN=$(cd "$(dirname "$BIN")" && pwd)/$(basename "$BIN")
if command -v sha256sum > /dev/null 2>&1; then
  SHA=sha256sum
else
  SHA="shasum -a 256"
fi
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
fail=0

ok() {
  echo "  ok: $*"
}

bad() {
  echo "  FAIL: $*"
  fail=1
}

# The fake tree.  Stems by matrix_tables.py's stem_hash: TST001, TST007
# go to TRAIN-X, TST015 and TST019 to DEV-X (checked below, not assumed).
T=$TMP/TPTP-v9.3.1
mkdir -p "$T/Problems/TST" "$T/Axioms"
problem() {
  {
    echo "%------------------------------------------------------------------------------"
    echo "% File     : $1 : a fake problem of tests/atp/atp_ifeq10_corpus.sh"
    echo "% Status   : $2"
    echo "% Rating   : 0.25 v9.3.0, 0.50 v9.0.0"
    echo "% SPC      : $3"
    echo "%------------------------------------------------------------------------------"
    printf '%s\n' "$4"
  } > "$T/Problems/TST/$1.p"
}
HORN='cnf(c1,axiom, ~ p(X) | q(X) ).
cnf(c2,axiom, p(a) ).
cnf(c3,negated_conjecture, ~ q(a) ).'
problem TST001-1 Unsatisfiable CNF_UNS_EPR_NEQ_HRN "$HORN"
problem TST002-1 Unsatisfiable CNF_UNS_EPR_NEQ_HRN "$HORN"
problem TST002+1 Theorem FOF_THM_EPR_NEQ 'fof(a1,axiom, p(a) ).
fof(g,conjecture, p(a) ).'
problem TST003-1 Unsatisfiable CNF_UNS_RFO_PEQ_UEQ 'cnf(c1,axiom, f(e,X) = X ).
cnf(c2,negated_conjecture, f(e,a) != a ).'
problem TST004-1 Unsatisfiable CNF_UNS_EPR_NEQ_HRN "$HORN"
problem TST004-10 Unsatisfiable CNF_UNS_RFO_PEQ_UEQ 'cnf(ifeq_axiom,axiom, ifeq(A,A,B,C) = B ).
cnf(c1,axiom, ifeq(p(X),true,q(X),true) = true ).
cnf(c2,axiom, p(a) = true ).
cnf(c3,negated_conjecture, q(a) != true ).'
problem TST005-1 Unsatisfiable CNF_UNS_EPR_NEQ_NHN 'cnf(c1,axiom, p(X) | q(X) ).
cnf(c2,negated_conjecture, ~ p(a) ).
cnf(c3,negated_conjecture, ~ q(a) ).'
problem TST006+1 Theorem FOF_THM_RFO_PEQ 'fof(left_identity,axiom, ! [X] : f(e,X) = X ).
fof(goal,conjecture, f(e,a) = a ).'
echo "cnf(successor,axiom, ~ p(X) | p(s(X)) )." > "$T/Axioms/TST007-0.ax"
problem TST007-1 Unsatisfiable CNF_UNS_RFO_NEQ_HRN "include('Axioms/TST007-0.ax').
cnf(c2,axiom, p(a) ).
cnf(c3,negated_conjecture, ~ p(s(s(a))) )."
problem TST008-1 Satisfiable CNF_SAT_EPR_NEQ_HRN 'cnf(c1,axiom, p(a) ).'
# Labelled Horn but not Horn: the nonhorn ALERT.
problem TST011-1 Unsatisfiable CNF_UNS_EPR_NEQ_HRN 'cnf(c1,axiom, p(X) | q(X) ).
cnf(c2,negated_conjecture, ~ p(a) ).
cnf(c3,negated_conjecture, ~ q(a) ).'
# No record in column 0: the header is not read, the file is noheader.
problem TST012-1 Unsatisfiable CNF_UNS_EPR_NEQ_HRN '  cnf(c1,axiom, p(a) ).'
# TST023-1 encodes to TST002-10 (a J13 name) with every symbol, variable
# and clause name renamed: j13dup, though no stem is shared.
problem TST023-1 Unsatisfiable CNF_UNS_RFO_NEQ_HRN 'cnf(c1,axiom, ~ p(X) | q(f(X)) ).
cnf(c2,axiom, p(a) ).
cnf(c3,negated_conjecture, ~ q(f(a)) ).'
problem TST002-10 Unsatisfiable CNF_UNS_RFO_PEQ_UEQ 'cnf(ifeq_axiom,axiom, ifeq(A,A,B,C) = B ).
cnf(goal,negated_conjecture, s(g(b)) != true ).
cnf(r2,axiom, true = r(b) ).
cnf(r1,axiom, ifeq(r(Y),true,s(g(Y)),true) = true ).'
problem TST010-1 Unsatisfiable CNF_UNS_EPR_NEQ_HRN 'cnf(c1,axiom, ~ q(X) | p(X) ).
cnf(c2,negated_conjecture, ~ p(a) ).'
problem TST015-1 Unsatisfiable CNF_UNS_EPR_NEQ_HRN "$HORN"
problem TST019-1 Unsatisfiable CNF_UNS_EPR_NEQ_HRN "$HORN"
echo "TST002-10" > "$TMP/j13.txt"
echo "TST015-3" > "$TMP/train_names.txt"
echo "TST001-5" > "$TMP/excl.txt"
echo "TST099-1" > "$TMP/excl0.txt"

split_of() {
  local h
  h=$(printf '%s' "$1" | $SHA | cut -c1-8)
  if [ $((16#$h % 100)) -lt 80 ]; then
    echo TRAIN-X
  else
    echo DEV-X
  fi
}

# run OUT [EXCLUDE_NAMES ...]: the driver on the fake tree with the
# exclusion list excl0.txt (a stem the tree lacks) and the given ones; its
# exit code in $rc, its output in OUT.log.  UNCHECKED=0 makes it require
# the pass file PASS, DEADLINE sets its DEADLINE_S.
run() {
  local out=$1
  shift
  env JOBS=2 TIMEOUT_S=60 DEADLINE_S="${DEADLINE:-14400}" TRAIN_NAMES="$TMP/train_names.txt" \
    IFEQ10_UNCHECKED="${UNCHECKED:-1}" IFEQ10_PASS="${PASS:-$TMP/no-such.ifeq10-pass}" \
    bash "$DRIVER" "$BIN" "$T" "$out" "$TMP/j13.txt" "$TMP/excl0.txt" "$@" < /dev/null > "$out.log" 2>&1
  rc=$?
}

col() {
  awk -F'\t' -v s="Problems/TST/$2.p" -v c="$3" 'NR > 1 && $1 == s { print $c }' "$1/manifest.tsv"
}

expect() {
  local got
  got=$(col "$1" "$2" 6)
  if [ "$got" = "$3" ]; then
    ok "$2 -> $3"
  else
    bad "$2: outcome '$got', want $3"
  fi
}

O1=$TMP/out1
run "$O1"
if [ $rc -eq 0 ] && grep -qx 'status=complete-alerts alerts=2' "$O1/.done"; then
  ok "first run: exit 0, $(cat "$O1/.done")"
else
  bad "first run: exit $rc, .done '$(cat "$O1/.done" 2> /dev/null)'"
  sed 's/^/      /' "$O1.log" | tail -n 30
fi
expect "$O1" TST001-1 emitted
expect "$O1" TST002-1 j13
expect "$O1" TST002+1 j13
expect "$O1" TST003-1 ueq
expect "$O1" TST004-1 tptp10
expect "$O1" TST004-10 ueq
expect "$O1" TST005-1 nonhorn
expect "$O1" TST006+1 unit
expect "$O1" TST007-1 emitted
expect "$O1" TST010-1 sat1
expect "$O1" TST015-1 pooltrain
expect "$O1" TST019-1 emitted
expect "$O1" TST002-10 j13
expect "$O1" TST011-1 nonhorn
expect "$O1" TST012-1 noheader
expect "$O1" TST023-1 j13dup
if [ -n "$(find "$O1/TRAIN-X" "$O1/DEV-X" -name 'TST023*')" ]; then
  bad "the J13 duplicate TST023-1 was written"
fi
if grep -q 'TST008-1' "$O1/manifest.tsv"; then
  bad "the Satisfiable TST008-1 is a candidate"
fi
if [ "$(split_of TST015)" != DEV-X ] || [ "$(split_of TST019)" != DEV-X ]; then
  bad "TST015 and TST019 no longer hash to DEV-X: pick other stems"
fi
for n in TST001-1 TST007-1 TST019-1; do
  want=$(split_of "$(printf '%s' "$n" | cut -c1-6)")
  label=train
  [ "$want" = DEV-X ] && label=heldout
  row=$(printf '%s\tProblems/TST/%s.p\tUnsatisfiable\tCNF_UNS_RFO_PEQ_UEQ\t%s' "$n-10" "$n-10" "$label")
  if [ "$(col "$O1" "$n" 7)" = "$want" ] && [ -s "$O1/$want/Problems/TST/$n-10.p" ] \
     && grep -qxF "$row" "$O1/$want/pool.tsv"; then
    ok "$n -> $want/Problems/TST/$n-10.p and its pool.tsv row"
  else
    bad "$n: split '$(col "$O1" "$n" 7)', want $want, or its file or pool.tsv row is missing"
  fi
done
if [ "$(cat "$O1/TRAIN-X/pool.tsv" "$O1/DEV-X/pool.tsv" | wc -l | tr -d ' ')" = 3 ]; then
  ok "the pool tables hold the 3 emitted problems"
else
  bad "the pool tables hold $(cat "$O1/TRAIN-X/pool.tsv" "$O1/DEV-X/pool.tsv" | wc -l | tr -d ' ') rows, want 3"
fi
if [ -n "$(find "$O1/TRAIN-X" "$O1/DEV-X" -name 'TST006*')" ]; then
  bad "the unit-equality TST006+1 was written"
fi
if [ "$(col "$O1" TST007-1 13)" = Axioms/TST007-0.ax ] && [ "$(col "$O1" TST007-1 12)" = 0.25 ] \
   && [ "$(col "$O1" TST001-1 9)" = 1 ] && [ "$(col "$O1" TST001-1 10)" = 0 ]; then
  ok "manifest columns: includes, rating, dispatchers, definitions"
else
  bad "manifest columns of TST007-1 / TST001-1: $(grep -E 'TST00[17]-1' "$O1/manifest.tsv" | tr '\t\n' '| ')"
fi
if grep -q '^ALERT sat1: Problems/TST/TST010-1.p' "$O1/summary.txt" \
   && grep -q '^ALERT nonhorn: Problems/TST/TST011-1.p' "$O1/summary.txt" \
   && grep -q '^corpus check: ok' "$O1/summary.txt" && grep -q '^j13_files=1 keyed' "$O1/summary.txt" \
   && grep -q '^pass=unchecked' "$O1/summary.txt" && grep -q '^j13_10_variants_skipped=2 ' "$O1/summary.txt"; then
  ok "summary: the sat1 and nonhorn alerts, the corpus check, 1 keyed J13 file, the unchecked binary, 2 J13 -10 variants"
else
  bad "summary.txt:"
  sed 's/^/      /' "$O1/summary.txt"
fi

# A rerun with the same inputs resumes: nothing is encoded again.
cp "$O1/manifest.tsv" "$TMP/manifest1.tsv"
run "$O1"
if [ $rc -eq 0 ] && cmp -s "$O1/manifest.tsv" "$TMP/manifest1.tsv"; then
  ok "a rerun with the same inputs resumes to the same manifest"
else
  bad "resume: exit $rc, manifest $(cmp -s "$O1/manifest.tsv" "$TMP/manifest1.tsv" && echo same || echo changed)"
fi

# Changed inputs on the same OUT_DIR are refused, and nothing changes.
run "$O1" "$TMP/excl.txt"
if [ $rc -eq 2 ] && grep -q 'inputs changed' "$O1.log" && cmp -s "$O1/manifest.tsv" "$TMP/manifest1.tsv"; then
  ok "an added exclusion list on the same OUT_DIR exits 2"
else
  bad "changed inputs: exit $rc: $(tail -n 1 "$O1.log")"
fi

# A held lock is refused.
flock "$O1/.work/.lock" env JOBS=2 TIMEOUT_S=60 TRAIN_NAMES="$TMP/train_names.txt" IFEQ10_UNCHECKED=1 \
  bash "$DRIVER" "$BIN" "$T" "$O1" "$TMP/j13.txt" "$TMP/excl0.txt" < /dev/null > "$TMP/lock.log" 2>&1
rc=$?
if [ $rc -eq 2 ] && grep -q 'another run holds' "$TMP/lock.log"; then
  ok "a second launch on a locked OUT_DIR exits 2"
else
  bad "lock: exit $rc: $(tail -n 1 "$TMP/lock.log")"
fi

# A fresh OUT_DIR with the exclusion list leaves TST001 out.
O2=$TMP/out2
run "$O2" "$TMP/excl.txt"
if [ $rc -eq 0 ] && [ "$(col "$O2" TST001-1 6)" = excluded ] && [ "$(col "$O2" TST007-1 6)" = emitted ] \
   && [ -z "$(find "$O2/TRAIN-X" "$O2/DEV-X" -name 'TST001*')" ] \
   && ! grep -q 'TST001' "$O2/TRAIN-X/pool.tsv" "$O2/DEV-X/pool.tsv"; then
  ok "a fresh run with the exclusion list leaves TST001-1 out"
else
  bad "exclusion: exit $rc, TST001-1 $(col "$O2" TST001-1 6)"
fi

# The deadline leaves every job pending; the rerun finishes them.
O3=$TMP/out3
DEADLINE=0 run "$O3"
if [ $rc -eq 0 ] && grep -qx 'status=incomplete pending=8' "$O3/.done"; then
  run "$O3"
  if [ $rc -eq 0 ] && grep -q '^status=complete-alerts' "$O3/.done" \
     && [ "$(cut -f1-10 "$O3/manifest.tsv")" = "$(cut -f1-10 "$TMP/manifest1.tsv")" ]; then
    ok "deadline then resume: the first run's manifest"
  else
    bad "resume after the deadline: exit $rc, $(cat "$O3/.done")"
  fi
else
  bad "deadline: exit $rc, $(cat "$O3/.done" 2> /dev/null)"
fi

# On resume a write outcome is retried and a timeout stays final.
r1=$O3/.work/res/TST001-1.tsv
r5=$O3/.work/res/TST005-1.tsv
sp1=$(col "$O3" TST001-1 7)
if [ -s "$r1" ] && [ -s "$r5" ] && [ -n "$sp1" ] && [ "$sp1" != - ]; then
  rm -f "$O3/$sp1/Problems/TST/TST001-1-10.p"
  awk -F'\t' -v OFS='\t' '{ $2 = "-"; $6 = "write"; $7 = "-" } 1' "$r1" > "$r1.x" && mv "$r1.x" "$r1"
  awk -F'\t' -v OFS='\t' '{ $6 = "timeout" } 1' "$r5" > "$r5.x" && mv "$r5.x" "$r5"
  run "$O3"
  if [ $rc -eq 0 ] && [ "$(col "$O3" TST001-1 6)" = emitted ] && [ -s "$O3/$sp1/Problems/TST/TST001-1-10.p" ] \
     && [ "$(col "$O3" TST005-1 6)" = timeout ]; then
    ok "resume re-encodes a write outcome and keeps a timeout"
  else
    bad "write/timeout resume: exit $rc, TST001-1 $(col "$O3" TST001-1 6), TST005-1 $(col "$O3" TST005-1 6)"
  fi
else
  bad "write/timeout resume: no result rows to seed in $O3"
fi

# No exclusion list, or no TRAIN_NAMES, is refused; ALLOW_LEAKY=1 runs
# and marks the corpus unguarded.
O6=$TMP/out6
env JOBS=2 TIMEOUT_S=60 TRAIN_NAMES="$TMP/train_names.txt" IFEQ10_UNCHECKED=1 \
  bash "$DRIVER" "$BIN" "$T" "$O6" "$TMP/j13.txt" < /dev/null > "$O6.log" 2>&1
rc=$?
if [ $rc -eq 2 ] && grep -q 'no EXCLUDE_NAMES' "$O6.log"; then
  ok "no exclusion list -> exit 2"
else
  bad "no exclusion list: exit $rc: $(tail -n 1 "$O6.log")"
fi
env JOBS=2 TIMEOUT_S=60 IFEQ10_UNCHECKED=1 \
  bash "$DRIVER" "$BIN" "$T" "$O6" "$TMP/j13.txt" "$TMP/excl0.txt" < /dev/null > "$O6.log" 2>&1
rc=$?
if [ $rc -eq 2 ] && grep -q 'no TRAIN_NAMES' "$O6.log"; then
  ok "no TRAIN_NAMES -> exit 2"
else
  bad "no TRAIN_NAMES: exit $rc: $(tail -n 1 "$O6.log")"
fi
env JOBS=2 TIMEOUT_S=60 IFEQ10_UNCHECKED=1 ALLOW_LEAKY=1 \
  bash "$DRIVER" "$BIN" "$T" "$O6" "$TMP/j13.txt" < /dev/null > "$O6.log" 2>&1
rc=$?
if [ $rc -eq 0 ] && grep -qx 'status=complete-unguarded alerts=2' "$O6/.done" \
   && grep -qx 'allow_leaky=1' "$O6/.work/inputs" && grep -q '^UNGUARDED: no EXCLUDE_NAMES' "$O6/summary.txt"; then
  ok "ALLOW_LEAKY=1 -> $(cat "$O6/.done"), recorded in the inputs"
else
  bad "ALLOW_LEAKY=1: exit $rc, .done '$(cat "$O6/.done" 2> /dev/null)'"
fi

# The pass file: none, another binary's, then this binary's.
O4=$TMP/out4
UNCHECKED=0 run "$O4"
if [ $rc -eq 2 ] && grep -q 'no pass file' "$O4.log"; then
  ok "no pass file -> exit 2"
else
  bad "no pass file: exit $rc: $(tail -n 1 "$O4.log")"
fi
printf 'findproof_sha256=%064d\nverdict=exact\n' 0 > "$TMP/other.ifeq10-pass"
UNCHECKED=0 PASS=$TMP/other.ifeq10-pass run "$O4"
if [ $rc -eq 2 ] && grep -q 'pass file of another binary' "$O4.log"; then
  ok "another binary's pass file -> exit 2"
else
  bad "another binary's pass file: exit $rc: $(tail -n 1 "$O4.log")"
fi
printf 'findproof_sha256=%s\ncases=11/11 exact\nverdict=exact\n' "$($SHA "$BIN" | cut -d' ' -f1)" \
  > "$TMP/this.ifeq10-pass"
UNCHECKED=0 PASS=$TMP/this.ifeq10-pass run "$O4"
if [ $rc -eq 0 ] && grep -q "^pass=$TMP/this.ifeq10-pass: cases=11/11 exact verdict=exact" "$O4/summary.txt"; then
  ok "this binary's pass file is accepted and recorded"
else
  bad "this binary's pass file: exit $rc: $(grep '^pass=' "$O4/summary.txt" 2> /dev/null)"
fi

# Another TPTP release is refused.
ln -s "$T" "$TMP/TPTP-v9.4.0"
O5=$TMP/out5
env JOBS=2 IFEQ10_UNCHECKED=1 TRAIN_NAMES="$TMP/train_names.txt" \
  bash "$DRIVER" "$BIN" "$TMP/TPTP-v9.4.0" "$O5" "$TMP/j13.txt" "$TMP/excl0.txt" < /dev/null > "$O5.log" 2>&1
rc=$?
if [ $rc -eq 2 ] && grep -q 'not v9.3.1' "$O5.log"; then
  ok "a v9.4.0 tree -> exit 2"
else
  bad "release: exit $rc: $(tail -n 1 "$O5.log")"
fi

if [ $fail -eq 0 ]; then
  echo "atp_ifeq10_corpus: ok"
else
  echo "atp_ifeq10_corpus: FAIL"
fi
exit $fail
