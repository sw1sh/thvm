#!/bin/bash
# Score the raw-MSL matmul kernel.
# Usage: ./score.sh [M] [N] [K]    (defaults: 512 512 512)
set -e
cd "$(dirname "$0")"
ROOT=$(cd ../../.. && pwd)
PYTHONPATH=$ROOT THVM_BACKEND=metal "$ROOT/bench/metal-problems/.venv/bin/python3" \
    -W ignore -u score.py "$@"
