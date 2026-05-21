#!/usr/bin/env bash
# Quick validation script: pick a break + planner config, run 100 trials with
# different seed bases, see if 30% holds across seed bases.
# Usage: ./br_validate.sh <break-speed> <break-follow> [extra args]
SPEED=${1:-8.0}
FOLLOW=${2:-0.0}
shift 2 || true
EXTRA="$@"
echo "config: speed=$SPEED follow=$FOLLOW $EXTRA"
for SEED in 7000 8000 9000; do
  R=$(./build/break_and_run.exe --trials 100 --seed-base $SEED \
      --break-speed $SPEED --break-follow $FOLLOW $EXTRA 2>/dev/null \
      | grep "BREAK-AND-RUN RATE" | head -1)
  echo "  seed-base=$SEED: $R"
done
