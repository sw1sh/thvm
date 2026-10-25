#!/bin/sh
# Assemble the FindProof CASC submission tarball from the live repo:
# the ATP engine source (unity build) + the CLI entry + the standalone
# build/run scripts.  Output: $1 (default ./FindProof.tgz).
# Verified to build on Ubuntu 22.04 / gcc 11.4 (ELF) and macOS.
set -e
REPO=$(cd "$(dirname "$0")/../.." && pwd)
OUT=${1:-$PWD/FindProof.tgz}
STAGE=$(mktemp -d)
D="$STAGE/FindProof"
mkdir -p "$D/src" "$D/tests/atp" "$D/tools" "$D/bin"
# whole ATP unity-build source tree (.c/.h only)
( cd "$REPO/src" && tar cf - $(find . \( -name '*.c' -o -name '*.h' \)) ) | ( cd "$D/src" && tar xf - )
cp "$REPO/tests/atp/test_atp_wolfram_bench.c" "$D/tests/atp/"
cp "$REPO/tools/thvm_atp_cli.c"            "$D/tools/"
cp "$REPO/tools/proof_check.c"             "$D/tools/"
# the CLI modules tools/thvm_atp_cli.c includes
mkdir -p "$D/tools/atp_cli"
cp "$REPO"/tools/atp_cli/*.c              "$D/tools/atp_cli/"
cp "$REPO/tools/casc/Makefile"             "$D/Makefile"
cp "$REPO/tools/casc/starexec_build"       "$D/starexec_build"
cp "$REPO/tools/casc/starexec_run_default" "$D/bin/starexec_run_default"
cp "$REPO/tools/casc/README"               "$D/README"
chmod +x "$D/starexec_build" "$D/bin/starexec_run_default"
( cd "$STAGE" && tar czf "$OUT" FindProof )
rm -rf "$STAGE"
echo "wrote $OUT ($(du -h "$OUT" | cut -f1))"
