#!/bin/bash
# tools/atp_enigma/ifeq10_diff.py and ifeq10_keys.py on inline files, no
# FindProof: a swapped clause name and a changed clause order are told
# apart from a plain match, a mismatch exits 1, and the duplicate key is
# blind to variable and symbol renaming, clause order, clause names and
# equation orientation, follows include() selections, and separates two
# different problems.  Pods only (it runs python3, as the pod steps do).
#
# Usage: tests/atp_ifeq10_diff.sh
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
DIFF=$ROOT/tools/atp_enigma/ifeq10_diff.py
KEYS=$ROOT/tools/atp_enigma/ifeq10_keys.py
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
fail=0

cat > "$TMP/a.p" <<'EOF'
cnf(ifeq_axiom,axiom, ifeq(A,A,B,C) = B ).
cnf(c1,axiom, ifeq(p(X),true,q(X),true) = true ).
cnf(c2,axiom, p(a) = true ).
cnf(c3,negated_conjecture, q(a) != true ).
EOF
# c1 and c2 swap names.
cat > "$TMP/names.p" <<'EOF'
cnf(ifeq_axiom,axiom, ifeq(A,A,B,C) = B ).
cnf(c2,axiom, ifeq(p(X),true,q(X),true) = true ).
cnf(c1,axiom, p(a) = true ).
cnf(c3,negated_conjecture, q(a) != true ).
EOF
# c1 and c2 swap places.
cat > "$TMP/order.p" <<'EOF'
cnf(ifeq_axiom,axiom, ifeq(A,A,B,C) = B ).
cnf(c2,axiom, p(a) = true ).
cnf(c1,axiom, ifeq(p(X),true,q(X),true) = true ).
cnf(c3,negated_conjecture, q(a) != true ).
EOF
cat > "$TMP/other.p" <<'EOF'
cnf(ifeq_axiom,axiom, ifeq(A,A,B,C) = B ).
cnf(c1,axiom, ifeq(p(X),true,q(X),true) = true ).
cnf(c2,axiom, p(a) = true ).
cnf(c3,negated_conjecture, q(q(a)) != true ).
EOF
# a renamed: other symbols, variables, names and order, c2 flipped.
cat > "$TMP/renamed.p" <<'EOF'
cnf(goal,negated_conjecture, s(k) != true ).
cnf(fact,axiom, true = r(k) ).
cnf(rule,axiom, ifeq(r(Y),true,s(Y),true) = true ).
cnf(dispatch,axiom, ifeq(U,U,V,W) = V ).
EOF
mkdir -p "$TMP/Axioms"
cat > "$TMP/Axioms/inner.ax" <<'EOF'
cnf(c1,axiom, ifeq(p(X),true,q(X),true) = true ).
cnf(junk,axiom, w(a) = true ).
EOF
cat > "$TMP/Axioms/outer.ax" <<'EOF'
cnf(ifeq_axiom,axiom, ifeq(A,A,B,C) = B ).
cnf(junk2,axiom, w(b) = true ).
include('Axioms/inner.ax',[c1]).
EOF
cat > "$TMP/inc.p" <<'EOF'
include('Axioms/outer.ax',[ifeq_axiom,c1]).
cnf(c2,axiom, p(a) = true ).
cnf(c3,negated_conjecture, q(a) != true ).
EOF

diff_case() {
  local what=$1 file=$2 want_rc=$3 want=$4 unwanted=$5
  python3 "$DIFF" "$TMP/a.p" "$file" > "$TMP/out" 2>&1
  local rc=$?
  if [ $rc -eq "$want_rc" ] && grep -q -- "$want" "$TMP/out" \
     && { [ -z "$unwanted" ] || ! grep -q -- "$unwanted" "$TMP/out"; }; then
    echo "  ok: $what: $(head -1 "$TMP/out")"
  else
    echo "  FAIL: $what: exit $rc (want $want_rc): $(head -1 "$TMP/out")"
    fail=1
  fi
}
diff_case "identical" "$TMP/a.p" 0 '^match: 4 clauses$' ''
diff_case "swapped names" "$TMP/names.p" 0 'clause names differ' 'clause order differs'
diff_case "swapped order" "$TMP/order.p" 0 'clause order differs' 'clause names differ'
diff_case "another goal" "$TMP/other.p" 1 '^MISMATCH' ''

printf '%s\n' "$TMP/a.p" "$TMP/renamed.p" "$TMP/order.p" "$TMP/names.p" "$TMP/inc.p" "$TMP/other.p" \
  "$TMP/missing.p" > "$TMP/list"
(cd "$TMP" && python3 "$KEYS" "$TMP" "$TMP/list") > "$TMP/keys" 2> "$TMP/keys.err"
rc=$?
key() {
  awk -F'\t' -v p="$TMP/$1" '$1 == p { print $2 }' "$TMP/keys"
}
ka=$(key a.p)
if [ $rc -ne 0 ] || [ "$(wc -l < "$TMP/keys" | tr -d ' ')" != 7 ] || [ ${#ka} -ne 64 ]; then
  echo "  FAIL: ifeq10_keys: exit $rc, $(wc -l < "$TMP/keys" | tr -d ' ') lines: $(head -1 "$TMP/keys.err")"
  fail=1
fi
for f in renamed.p order.p names.p inc.p; do
  if [ "$(key "$f")" = "$ka" ]; then
    echo "  ok: $f has the key of a.p"
  else
    echo "  FAIL: $f: key '$(key "$f")', a.p '$ka'"
    fail=1
  fi
done
if [ -n "$(key other.p)" ] && [ "$(key other.p)" != "$ka" ]; then
  echo "  ok: other.p has another key"
else
  echo "  FAIL: other.p: key '$(key other.p)'"
  fail=1
fi
if [ "$(key missing.p)" = ERROR ]; then
  echo "  ok: an unreadable file -> ERROR"
else
  echo "  FAIL: missing.p: '$(key missing.p)'"
  fail=1
fi

if [ $fail -eq 0 ]; then
  echo "atp_ifeq10_diff: ok"
else
  echo "atp_ifeq10_diff: FAIL"
fi
exit $fail
