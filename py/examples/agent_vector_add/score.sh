#!/bin/bash
# Score the raw-MSL vector_add kernel.
# Usage: ./score.sh [N]    (default: 1048576)
set -e
cd "$(dirname "$0")"
ROOT=$(cd ../../.. && pwd)
PYTHONPATH=$ROOT DEV=metal "$ROOT/bench/metal-problems/.venv/bin/python3" \
    -W ignore -u score.py "$@"
