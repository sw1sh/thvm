#!/bin/bash
# Score the raw-MSL layernorm kernel.
# Usage: ./score.sh [R] [C]    (defaults: R=128 C=768)
set -e
cd "$(dirname "$0")"
ROOT=/Users/swish/src/thvm/.claude/worktrees/agent-a4b051f22cf37b9f9
PYTHONPATH=$ROOT THVM_BACKEND=metal /Users/swish/src/thvm/bench/metal-problems/.venv/bin/python3 \
    -W ignore -u score.py "$@"
