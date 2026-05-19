#!/bin/bash
# Score the raw-MSL vector_sum kernel.
# Usage: ./score.sh [N]    (defaults: N=65536)
set -e
cd "$(dirname "$0")"
ROOT=/Users/swish/src/thvm/.claude/worktrees/agent-a21119cdd24009bad
PYTHONPATH=$ROOT DEV=metal /Users/swish/src/thvm/bench/metal-problems/.venv/bin/python3 \
    -W ignore -u score.py "$@"
