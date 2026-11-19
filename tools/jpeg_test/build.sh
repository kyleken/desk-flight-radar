#!/usr/bin/env bash
# Builds the decoder test harness (with address/undefined-behaviour checks) and runs the tests.
# Needs: g++, python3 with numpy, pillow and mozjpeg-lossless-optimization, ImageMagick (convert).
set -euo pipefail
cd "$(dirname "$0")"
g++ -O2 -g -Wall -Wextra -fsanitize=address,undefined -I../../src -o harness harness.cpp ../../src/jpeg_small.cpp
python3 test.py   # baseline vs progressive must match exactly; "LOW" lines are only reference-score notes
python3 fuzz.py   # damaged files must never crash or hang
