#!/bin/sh
# Assemble the FindProof BINARY StarExec package: a statically linked
# Linux binary + the run configuration, no source (CASC delivers source
# separately; see mk_tarball.sh for the source package).
# Requires zig (cross-compiles x86_64-linux-musl from any host).
# Output: $1 (default ./FindProof.tgz).
set -e
REPO=$(cd "$(dirname "$0")/../.." && pwd)
OUT=${1:-$PWD/FindProof.tgz}
STAGE=$(mktemp -d)
D="$STAGE/FindProof"
mkdir -p "$D/bin"
zig cc -target x86_64-linux-musl -static -std=c11 -O2 -w -D_GNU_SOURCE \
  -DATP_CP_GROUND_JOIN -DATP_FV_INDEX -DATP_MNF -DATP_ORDERED_REWRITE \
  -DATP_ORPHAN_KILL -DATP_RULE_INDEX -DATP_VAR_NORM \
  -DTHVM_ATPFT_ALLOC -DTHVM_ATPFT_CONVERT -DTHVM_ATPFT_LPO \
  -DTHVM_ATPFT_MATCH -DTHVM_ATPFT_NORM -DTHVM_ATPFT_RULES -DTHVM_ATP_AC \
  -o "$D/bin/FindProof" "$REPO/tools/thvm_atp_cli.c" -lm
cp "$REPO/tools/casc/starexec_run_default" "$D/bin/"
cp "$REPO/tools/casc/README"               "$D/README"
chmod +x "$D/bin/FindProof" "$D/bin/starexec_run_default"
( cd "$STAGE" && tar czf "$OUT" FindProof )
rm -rf "$STAGE"
echo "wrote $OUT ($(du -h "$OUT" | cut -f1), static linux/x86_64)"
