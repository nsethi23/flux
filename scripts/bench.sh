#!/usr/bin/env bash
# One-command build + correctness + benchmark reproduction for Flux.
#
# Usage:
#   ./scripts/bench.sh                  # build, run tests, run portable benchmark
#   ./scripts/bench.sh path/to.itch     # also run the rdtsc latency benchmark on
#                                        # real NASDAQ ITCH 5.0 data (x86_64 only)
#
# ITCH 5.0 sample files: https://emi.nasdaq.com/ITCH/Nasdaq%20ITCH/

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"
ITCH_FILE="${1:-}"

echo "==> Configuring (Release)"
cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release >/dev/null

echo "==> Building"
cmake --build "$BUILD_DIR" -j"$(getconf _NPROCESSORS_ONLN)"

echo
echo "==> Correctness: unit tests"
"$BUILD_DIR/flux_tests"

echo
echo "==> Performance: synthetic add-order benchmark (portable, no data required)"
"$BUILD_DIR/flux_bench"

if [[ -n "$ITCH_FILE" ]]; then
    if [[ "$(uname -m)" != "x86_64" ]]; then
        echo
        echo "==> Skipping real-data latency benchmark: flux_latency uses rdtsc," \
             "which requires x86_64 for cycle-accurate timing. Detected $(uname -m)."
    elif [[ ! -f "$ITCH_FILE" ]]; then
        echo "ITCH file not found: $ITCH_FILE" >&2
        exit 1
    else
        echo
        echo "==> Performance: p50/p99/p999 latency on real NASDAQ ITCH data"
        "$BUILD_DIR/flux_latency" "$ITCH_FILE"
    fi
else
    echo
    echo "==> Skipping real-data latency benchmark (no ITCH file given)."
    echo "    Download a sample file and re-run:"
    echo "      ./scripts/bench.sh path/to.itch"
    echo "    Samples: https://emi.nasdaq.com/ITCH/Nasdaq%20ITCH/"
fi
