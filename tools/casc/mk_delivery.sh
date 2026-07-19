#!/bin/sh
# Rebuild the full CASC-J13 / ProoVer 2026 delivery set at the current HEAD:
# the two static-musl binary packages, the two source packages, and the
# combined Descriptions-and-Samples archive.  Runs the regression battery
# first (byte-identical samples, official CheckProof verdicts, synth score)
# and refuses to pack if anything drifts.
#
#   sh tools/casc/mk_delivery.sh [OUTDIR]     (default OUTDIR: ~/.cache/thvm/casc)
#
# Env:
#   SKIP_CHECKS=1   pack without running the regression battery
#   OFFICIAL_DIR=.. dir holding the official ProoVer samples (COR000+1.s etc);
#                   default: fetch proover-competition.github.io/assets/samples.tgz
set -e
REPO=$(cd "$(dirname "$0")/../.." && pwd)
OUT=${1:-$HOME/.cache/thvm/casc}
S="$REPO/wl/Examples/proover/samples"
mkdir -p "$OUT"
cd "$REPO"

echo "== delivery build at HEAD $(git rev-parse --short HEAD 2>/dev/null || echo '?') =="

# --- local binaries (unity build; touch to defeat the #include-mtime trap) ---
touch tools/thvm_atp_cli.c
make -f tools/casc/Makefile    bin/FindProof  >/dev/null
make -f tools/proover/Makefile bin/CheckProof >/dev/null
echo "built bin/FindProof + bin/CheckProof"

# --- regression battery ---
if [ -z "$SKIP_CHECKS" ]; then
  fail=0

  # UEQ + FOF samples reproduce byte-for-byte (basename convention).
  ( cd "$REPO/wl/Examples/proover/problems" \
      && "$REPO/bin/FindProof" --schedule --szs --proof BOO001-1.p 2>/dev/null ) \
    | diff - "$S/BOO001-1.FindProof.sol" >/dev/null \
    && echo "  ok   BOO001-1 sample byte-identical" \
    || { echo "  FAIL BOO001-1 sample drifted"; fail=1; }

  if [ -f "$HOME/.cache/thvm/casc/SEU140+2.p" ]; then
    ( cd "$HOME/.cache/thvm/casc" \
        && "$REPO/bin/FindProof" --schedule --szs --proof SEU140+2.p 2>/dev/null ) \
      | diff - "$S/SEU140+2.FindProof.sol" >/dev/null \
      && echo "  ok   SEU140+2 sample byte-identical" \
      || { echo "  FAIL SEU140+2 sample drifted"; fail=1; }
  else
    echo "  skip SEU140+2 (no ~/.cache/thvm/casc/SEU140+2.p)"
  fi

  # Official ProoVer 3: VerifiedGood / Unknown / VerifiedBad.
  odir="$OFFICIAL_DIR"
  if [ -z "$odir" ]; then
    odir=$(mktemp -d)
    if curl -sfL -o "$odir/s.tgz" \
         https://proover-competition.github.io/assets/samples.tgz 2>/dev/null; then
      tar xzf "$odir/s.tgz" -C "$odir" && odir="$odir/samples"
    else odir=""; fi
  fi
  if [ -n "$odir" ] && [ -f "$odir/COR000+1.s" ]; then
    for pair in COR000+1:VerifiedGood TMO000+1:Unknown EVL000+1:VerifiedBad; do
      s=${pair%:*}; want=${pair#*:}
      got=$("$REPO/bin/CheckProof" --szs "$odir/$s.s" 2>/dev/null \
              | sed -n 's/.*SZS status \([A-Za-z]*\).*/\1/p' | head -1)
      [ "$got" = "$want" ] \
        && echo "  ok   $s -> $got" \
        || { echo "  FAIL $s -> $got (want $want)"; fail=1; }
    done
  else
    echo "  skip official ProoVer 3 (samples unavailable; set OFFICIAL_DIR)"
  fi

  # Synthetic corpus: zero catastrophic errors.
  ( cd "$REPO/wl/Examples/proover/synth" \
      && python3 score.py . "$REPO/bin/CheckProof" >/dev/null 2>&1 ) \
    && echo "  ok   synth corpus (0 catastrophic errors)" \
    || { echo "  FAIL synth corpus (catastrophic error or crash)"; fail=1; }

  [ "$fail" = 0 ] || { echo "== regressions FAILED; not packing =="; exit 1; }
fi

# --- pack all five ---
STAGE=$(mktemp -d); FOLDER="FindProof Descriptions and Samples"
mkdir -p "$STAGE/$FOLDER"
cp "$S/FindProof.CASC-J13.html" "$S/CheckProof.ProoVer.html" \
   "$S/SEU140+2.FindProof.sol"  "$S/BOO001-1.FindProof.sol" \
   "$S/ProoVer.CheckProof.samples.txt" "$STAGE/$FOLDER/"
( cd "$STAGE" && tar czf "$OUT/FindProof-Descriptions-and-Samples.tgz" "$FOLDER" )
rm -rf "$STAGE"
echo "wrote $OUT/FindProof-Descriptions-and-Samples.tgz"

sh tools/casc/mk_bin_tarball.sh    "$OUT/FindProof.tgz"
sh tools/proover/mk_bin_tarball.sh "$OUT/CheckProof.tgz"
sh tools/casc/mk_tarball.sh        "$OUT/FindProof-source.tgz"
sh tools/proover/mk_tarball.sh     "$OUT/CheckProof-source.tgz"

echo "== delivery set (5 files) in $OUT =="
ls -1 "$OUT"/FindProof.tgz "$OUT"/CheckProof.tgz \
      "$OUT"/FindProof-source.tgz "$OUT"/CheckProof-source.tgz \
      "$OUT"/FindProof-Descriptions-and-Samples.tgz
