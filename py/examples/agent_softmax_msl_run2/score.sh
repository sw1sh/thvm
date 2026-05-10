#!/bin/bash
# Score the raw-MSL softmax kernel.
# Usage: ./score.sh [R] [C]    (defaults: R=32 C=256)
set -e
cd "$(dirname "$0")"
ROOT=/Users/swish/src/thvm
PYTHONPATH=$ROOT THVM_BACKEND=metal $ROOT/bench/metal-problems/.venv/bin/python3 \
    -W ignore -u score.py "$@"
