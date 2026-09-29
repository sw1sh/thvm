#!/bin/sh
# Run CheckProof over the adversarial battery and diff against
# expected.tsv.  The battery is the distilled ProoVer 2026 post-mortem:
# H1-H5 are the false-accept families the shipped checker verified
# (-10 each at the competition), H6-H7 the false-reject families it
# rejected or abstained on.  H10-H20 are the soundness holes found in
# the rebuilt checker against the published 2026 set (binder capture in
# the instantiate fast path, predicate abstraction, sibling Skolem
# reuse, cth hypotheses, circular / conjecture demodulation, definitions
# with free body variables, uncited demodulators) and the valid shapes
# it rejected.  H21-H28 cover problems that include() axiom files: leaves
# match the included formulas (nested includes too), never the ones a
# selection list leaves out, and an include that does not expand abstains.
# H29-H32 cover functors wider than a term node, read through a curried
# encoding: a refutation through it confirms only a Horn step, a name at
# two arities is two symbols, and quoted atoms cannot spell its names.
# H33: an existential under 17 universals: a Skolem term over all of them
# passed the unifier whole (VerifiedGood); the witness takes the universals
# its body mentions, and the step's countermodel is found.
# Usage: run.sh [CheckProof]
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
