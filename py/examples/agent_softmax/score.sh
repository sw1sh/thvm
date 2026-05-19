#!/bin/bash
# Run the softmax agent's kernel.py through the score harness.
# Defaults to (R=32, C=256). Override: ./score.sh 64 1024
set -e
cd "$(dirname "$0")"
ROOT=/Users/swish/src/thvm
PYTHONPATH=$ROOT DEV=metal $ROOT/bench/metal-problems/.venv/bin/python3 \
    -W ignore -u score.py "$@"
