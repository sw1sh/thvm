#!/usr/bin/env bash
# Stage the Docker build context for the ProoVer deploy images (everything the
# Dockerfiles COPY / bind-mount). Staged artifacts are gitignored.
#
#   ./stage.sh [version]      version defaults to 15.0.0 (Dockerfile.ubuntu);
#                             use 14.0.0 for Dockerfile.centos7.
set -euo pipefail
cd "$(dirname "$0")"
ver="${1:-15.0.0}"

# 1. checker + corpus (from the parent example dir)
rm -rf proover && mkdir proover
cp ../proover.wl ../check.wls proover/
cp -R ../corpus proover/corpus

# 2. Wolfram/WolframParser paclet -- the offline-install source (a paclet dir
#    under Paclets/Repository == PacletInstall). Pure WL, so cross-platform.
pac=$(wolframscript -code 'PacletObject["Wolfram/WolframParser"]["Location"]' 2>/dev/null | tr -d '"')
if [ -z "$pac" ] || [ ! -d "$pac" ]; then
    echo "could not locate Wolfram/WolframParser paclet (need a licensed wolframscript)"; exit 1
fi
rm -rf wolframparser && cp -R "$pac" wolframparser

# 3. Wolfram installer (Standard, Linux x86_64). ~2.1-2.6 GB; gitignored.
sh="W-LINUX-Standard-${ver}.sh"
[ -f "$sh" ] || curl -fSL -o "$sh" "https://files.wolframcdn.com/internal/daily-builds/${sh}"

echo "staged: proover/  wolframparser/ ($(basename "$pac"))  ${sh}"
