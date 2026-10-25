#!/bin/sh
# Assemble the CheckProof ProoVer submission tarball from the live repo:
# the engine source (unity build) + the CLI entry + the standalone
# build/run scripts.  Output: $1 (default ./CheckProof.tgz).
set -e
REPO=$(cd "$(dirname "$0")/../.." && pwd)
OUT=${1:-$PWD/CheckProof.tgz}
STAGE=$(mktemp -d)
D="$STAGE/CheckProof"
mkdir -p "$D/src" "$D/tests/atp" "$D/tools" "$D/bin"
# whole engine unity-build source tree (.c/.h only)
( cd "$REPO/src" && tar cf - $(find . \( -name '*.c' -o -name '*.h' \)) ) | ( cd "$D/src" && tar xf - )
cp "$REPO/tests/atp/test_atp_wolfram_bench.c" "$D/tests/atp/"
cp "$REPO/tools/thvm_atp_cli.c"            "$D/tools/"
cp "$REPO/tools/proof_check.c"             "$D/tools/"
# the CLI modules tools/thvm_atp_cli.c includes
mkdir -p "$D/tools/atp_cli"
cp "$REPO"/tools/atp_cli/*.c              "$D/tools/atp_cli/"
cp "$REPO/tools/proover/Makefile"          "$D/Makefile"
cp "$REPO/tools/proover/starexec_build"    "$D/starexec_build"
cp "$REPO/tools/proover/starexec_run_default" "$D/bin/starexec_run_default"
cp "$REPO/tools/proover/README"            "$D/README"
chmod +x "$D/starexec_build" "$D/bin/starexec_run_default"
( cd "$STAGE" && tar czf "$OUT" CheckProof )
rm -rf "$STAGE"
echo "wrote $OUT ($(du -h "$OUT" | cut -f1))"
