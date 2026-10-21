#!/usr/bin/env bash
# Desktop preview of Claude mode: compiles the firmware's drawing code against stubs and writes PNGs.
# Usage: tools/preview/run.sh [output dir]   (default: tools/preview/out)
set -euo pipefail
cd "$(dirname "$0")"
OUT="${1:-out}"
mkdir -p "$OUT" build/inc
# Quoted includes look next to the including header first, so the stubs and the few real headers
# the drawing code needs go into one private folder (the real App.h would pull in the hardware).
cp stubs/*.h build/inc/
cp ../../lib/modified_TFT_eSPI/Fonts/glcdfont.c build/inc/
for h in ClaudeApp.h Clawd.h OnlineData.h Pixel.h; do cp "../../include/$h" build/inc/; done
clang++ -std=c++17 -O1 -w -Ibuild/inc -o build/preview \
  preview.cpp ../../src/ClaudeApp.cpp ../../src/Clawd.cpp ../../src/Pixel.cpp
./build/preview "$OUT" | while read -r ppm; do
  uv run --quiet --with pillow python -c "import sys; from PIL import Image; Image.open(sys.argv[1]).save(sys.argv[1][:-4] + '.png')" "$ppm"
  rm "$ppm"; echo "${ppm%.ppm}.png"
done
