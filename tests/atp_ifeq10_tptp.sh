#!/bin/bash
# FindProof --emit-ifeq10 against the -10 files of a TPTP tree.  The cases
# are the ten of tests/test_atp_ifeq10.c plus SYN143-1 (one clause on
# Axioms/SYN001-0.ax, too big to inline there), whose -10 file pins a part
# of the lazily interned symbol order: TPTP ships a -10 version of each,
# and no name stem among them is the stem of a CASC-J13 problem.  Each
# source is encoded straight from the tree (its include()s resolved there,
# its header Status carried over) and compared with TPTP's -10 file by
# tools/atp_enigma/ifeq10_diff.py, modulo clause order and variable
# renaming; the clause names must agree as well, as in
# tests/test_atp_ifeq10.c.  A miss also says whether the two agree once
# generated symbols are renumbered ("generated") or the conditions of each
# ifeq tower are taken as a set ("nest").  Then the command's other exits:
# a non-Horn problem exits 3 with a message and no output, a one-element
# model exits 4 with the SZS line, an unreadable file exits 2.
#
# ALL=1 adds every other [CS18] pair of the tree outside the J13 stems,
# the held-out test of the port, and then needs J13_NAMES.  Its decision
# rule, fixed before the first run: the run fails if a case fails, or a
# surveyed source exits non-zero (2, 3, 4, timeout, crash), lands at
# "differs" or cannot be compared ("differror").  "names", "generated"
# and "nest" are reported and pass; the verdict is "exact" only when every
# case and every surveyed pair matches, clause names included, and
# "equivalent" otherwise.  A passing ALL=1 run writes PASS_FILE (default
# <binary>.ifeq10-pass): the binary's sha256, the counts and the verdict,
# which tools/atp_enigma/make_ifeq10_corpus.sh requires.
#
# Usage: TPTP=/path/to/TPTP-v9.x [ALL=1] tests/atp_ifeq10_tptp.sh [bin/FindProof]
#   J13_NAMES defaults to ~/.cache/thvm/casc/J13/neural_p0/j13_ueq_names.txt
#   (checked against the cases when present).  Exits 0 when every check
#   passes.  Pods only.
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
BIN=${1:-$ROOT/bin/FindProof}
DIFF=$ROOT/tools/atp_enigma/ifeq10_diff.py
J13_NAMES=${J13_NAMES:-$HOME/.cache/thvm/casc/J13/neural_p0/j13_ueq_names.txt}
ALL=${ALL:-0}
if [ -z "${TPTP:-}" ] || [ ! -d "$TPTP/Problems" ]; then
  echo "  FAIL: set TPTP to a TPTP tree (Problems/, Axioms/)"
  exit 2
fi
if [ ! -x "$BIN" ]; then
  echo "  FAIL: $BIN is not an executable"
  exit 2
fi
if [ "$ALL" = 1 ] && [ ! -s "$J13_NAMES" ]; then
  echo "  FAIL: ALL=1 needs the J13 name list (J13_NAMES=$J13_NAMES is missing or empty)"
  exit 2
fi
# Absolute: the encoder runs in a scratch directory, so that no include
# resolves against the caller's working directory.
TPTP=$(cd "$TPTP" && pwd)
BIN=$(cd "$(dirname "$BIN")" && pwd)/$(basename "$BIN")
PASS_FILE=${PASS_FILE:-$BIN.ifeq10-pass}
if command -v sha256sum > /dev/null 2>&1; then
  SHA=sha256sum
else
  SHA="shasum -a 256"
fi
TMO=""
if command -v timeout > /dev/null 2>&1; then
  TMO="timeout -k 5 120"
fi
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
fail=0
if [ "$ALL" = 1 ]; then
  rm -f "$PASS_FILE"
fi

# source, TPTP's -10 version
CASES="LCL096-1 LCL096-10
LCL423-1 LCL423-10
ANA004-1 ANA004-10
PUZ131+1 PUZ131-10
KLE090+1 KLE090-10
SEU323+1 SEU323-10
PLA023-1 PLA023-10
LAT183-1 LAT183-10
ALG069+1 ALG069-10
CAT010-4 CAT010-10
SYN143-1 SYN143-10"
ncases=$(printf '%s\n' "$CASES" | wc -l | tr -d ' ')

: > "$TMP/j13_stems"
if [ -s "$J13_NAMES" ]; then
  sed -E 's/^([A-Z]{3}[0-9]{3}).*/\1/' "$J13_NAMES" | sort -u > "$TMP/j13_stems"
  hit=$(printf '%s\n' "$CASES" | cut -c1-6 | sort -u | comm -12 - "$TMP/j13_stems" | tr '\n' ' ')
  if [ -n "$hit" ]; then
    echo "  FAIL: cases share a stem with CASC-J13: $hit"
    fail=1
  else
    echo "  ok: no case shares a stem with the $(wc -l < "$J13_NAMES" | tr -d ' ') J13 names"
  fi
else
  echo "  note: $J13_NAMES not found; the J13 stem check of the cases is skipped"
fi

exact=0
while read -r src ten; do
  dom=$(printf '%s' "$src" | cut -c1-3)
  sp=$TPTP/Problems/$dom/$src.p
  tp=$TPTP/Problems/$dom/$ten.p
  if [ ! -f "$sp" ] || [ ! -f "$tp" ]; then
    echo "  FAIL: $src: $sp or $tp is missing"
    fail=1
    continue
  fi
  out=$TMP/$src.out
  # shellcheck disable=SC2086
  (cd "$TMP" && TPTP="$TPTP" $TMO "$BIN" --emit-ifeq10 "$sp" < /dev/null > "$out" 2> "$TMP/$src.err")
  rc=$?
  if [ $rc -ne 0 ]; then
    echo "  FAIL: $src: exit $rc: $(head -1 "$TMP/$src.err")"
    fail=1
    continue
  fi
  if ! grep -q '^% Status   : Unsatisfiable$' "$out"; then
    echo "  FAIL: $src: the header does not carry Status Unsatisfiable"
    fail=1
  fi
  if python3 "$DIFF" "$out" "$tp" < /dev/null > "$TMP/$src.diff" 2>&1; then
    if grep -q 'clause names differ' "$TMP/$src.diff"; then
      echo "  FAIL: $src -> $ten: $(head -1 "$TMP/$src.diff")"
      fail=1
    else
      echo "  ok: $src -> $ten: $(head -1 "$TMP/$src.diff")"
      exact=$((exact + 1))
    fi
  else
    echo "  FAIL: $src -> $ten:"
    sed 's/^/        /' "$TMP/$src.diff"
    fail=1
  fi
done <<EOF
$CASES
EOF
echo "  $exact/$ncases cases match TPTP's -10 files modulo clause order and variable renaming"

nsurv=0
nexact=0
if [ "$ALL" = 1 ]; then
  : > "$TMP/survey"
  printf '%s\n' "$CASES" | cut -d' ' -f1 > "$TMP/cases"
  nskip=0
  grep -l 'to UEQ using \[CS18\]' "$TPTP"/Problems/*/*-10*.p > "$TMP/cs18" 2> /dev/null
  while read -r tp; do
    src=$(sed -n 's/^% Comments : Converted from \([^ ]*\) to UEQ using \[CS18\].*/\1/p' "$tp" | head -1)
    stem=$(printf '%s' "$src" | cut -c1-6)
    sp=$TPTP/Problems/$(printf '%s' "$src" | cut -c1-3)/$src.p
    if [ -z "$src" ] || [ ! -f "$sp" ] || grep -qx "$src" "$TMP/cases" || grep -qx "$stem" "$TMP/j13_stems"; then
      nskip=$((nskip + 1))
      continue
    fi
    # shellcheck disable=SC2086
    (cd "$TMP" && TPTP="$TPTP" $TMO "$BIN" --emit-ifeq10 "$sp" < /dev/null > "$TMP/s.out" 2> /dev/null)
    rc=$?
    if [ $rc -ne 0 ]; then
      level="exit$rc"
    else
      python3 "$DIFF" "$TMP/s.out" "$tp" < /dev/null > "$TMP/s.diff" 2>&1
      case $? in
        0)
          level=exact
          if grep -q 'clause names differ' "$TMP/s.diff"; then
            level=names
          fi
          ;;
        1)
          if grep -q '^  generated match' "$TMP/s.diff"; then
            level=generated
          elif grep -q '^  nest *match' "$TMP/s.diff"; then
            level=nest
          else
            level=differs
          fi
          ;;
        *)
          level=differror
          ;;
      esac
    fi
    printf '%s\t%s\t%s\n' "$level" "$src" "$(basename "$tp" .p)" >> "$TMP/survey"
  done < "$TMP/cs18"
  nsurv=$(wc -l < "$TMP/survey" | tr -d ' ')
  nexact=$(awk -F'\t' '$1 == "exact"' "$TMP/survey" | wc -l | tr -d ' ')
  echo "  survey of $nsurv further [CS18] pairs ($nskip skipped: a J13 stem, a case or no source):"
  cut -f1 "$TMP/survey" | sort | uniq -c | sed 's/^ */      /'
  awk -F'\t' '$1 ~ /^(names|generated|nest)$/ { print "      not exact: " $1 ": " $2 " -> " $3 }' \
    "$TMP/survey" | head -n 40
  nbad=$(awk -F'\t' '$1 !~ /^(exact|names|generated|nest)$/' "$TMP/survey" | wc -l | tr -d ' ')
  if [ "$nbad" -gt 0 ]; then
    echo "  FAIL: $nbad surveyed sources exit non-zero, differ or cannot be compared:"
    awk -F'\t' '$1 !~ /^(exact|names|generated|nest)$/ { print "      " $1 ": " $2 " -> " $3 }' \
      "$TMP/survey" | head -n 60
    fail=1
  fi
fi

cat > "$TMP/nonhorn.p" <<'EOF'
cnf(c1,axiom, p(X) | q(X) ).
cnf(c2,negated_conjecture, ~ p(a) ).
cnf(c3,negated_conjecture, ~ q(a) ).
EOF
cat > "$TMP/sat1.p" <<'EOF'
cnf(c1,axiom, ~ q(X) | p(X) ).
cnf(c2,negated_conjecture, ~ p(a) ).
EOF
check_exit() {
  local what=$1 want=$2 file=$3 pat=$4 where=$5
  "$BIN" --emit-ifeq10 "$file" < /dev/null > "$TMP/x.out" 2> "$TMP/x.err"
  local rc=$?
  local ok=1
  [ $rc -eq "$want" ] || ok=0
  if [ "$where" = stdout ]; then
    grep -q "$pat" "$TMP/x.out" || ok=0
  else
    grep -q "$pat" "$TMP/x.err" || ok=0
    [ ! -s "$TMP/x.out" ] || ok=0
  fi
  if [ $ok -eq 1 ]; then
    echo "  ok: $what -> exit $rc"
  else
    echo "  FAIL: $what: exit $rc (want $want), stdout $(wc -c < "$TMP/x.out" | tr -d ' ') bytes," \
      "stderr: $(head -1 "$TMP/x.err")"
    fail=1
  fi
}
check_exit "non-Horn problem" 3 "$TMP/nonhorn.p" "not Horn" stderr
check_exit "one-element model" 4 "$TMP/sat1.p" "SZS status Satisfiable" stdout
check_exit "unreadable file" 2 "$TMP/no-such-file.p" "cannot read" stderr

if [ $fail -eq 0 ] && [ "$ALL" = 1 ]; then
  verdict=equivalent
  if [ "$exact" -eq "$ncases" ] && [ "$nexact" -eq "$nsurv" ]; then
    verdict=exact
  fi
  {
    echo "findproof_sha256=$($SHA "$BIN" | cut -d' ' -f1)"
    echo "tptp_root=$TPTP"
    echo "date=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
    echo "cases=$exact/$ncases exact"
    echo "survey=$nsurv pairs: $(cut -f1 "$TMP/survey" | sort | uniq -c | awk '{ printf "%s%s %s", (NR > 1) ? ", " : "", $2, $1 }')"
    echo "verdict=$verdict"
  } > "$PASS_FILE"
  echo "  verdict: $verdict (pass file $PASS_FILE)"
fi
if [ $fail -eq 0 ]; then
  echo "atp_ifeq10_tptp: ok"
else
  echo "atp_ifeq10_tptp: FAIL"
fi
exit $fail
