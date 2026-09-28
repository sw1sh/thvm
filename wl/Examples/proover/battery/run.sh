#!/bin/sh
# Run CheckProof over the adversarial battery and diff against
# expected.tsv.  The battery is the distilled ProoVer 2026 post-mortem:
# H1-H5 are the false-accept families the shipped checker verified
# (-10 each at the competition), H6-H7 the false-reject families it
# rejected or abstained on.  H10-H17 are the soundness holes found in
# the rebuilt checker against the published 2026 set (binder capture in
# the instantiate fast path, predicate abstraction, sibling Skolem
# reuse, cth hypotheses, circular / conjecture demodulation) and the
# valid shapes it rejected.  Usage: run.sh [CheckProof]
HERE=$(cd "$(dirname "$0")" && pwd)
BIN=${1:-$HERE/../../../../bin/CheckProof}
fail=0
while IFS="	" read -r f want family; do
  [ "$f" = "file" ] && continue
  got=$("$BIN" --szs "$HERE/$f" 2>/dev/null | head -1 | awk '{print $4}')
  if [ "$got" = "$want" ]; then
    printf 'ok   %-28s %s\n' "$f" "$got"
  else
    printf 'FAIL %-28s got %s want %s\n' "$f" "$got" "$want"
    fail=1
  fi
done < "$HERE/expected.tsv"
exit $fail
