#!/bin/sh
# Desk Flight Radar screen simulator.
# Compiles the firmware's real UI code for Linux with stand-in hardware and
# renders every screen to tools/simulator/out/*.png.
#
# Needs: g++, gcc, python3 with Pillow, and one `pio run` in the project
# (that downloads the LovyanGFX sources this uses).
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
L="$ROOT/.pio/libdeps/radar/LovyanGFX/src"
OBJ="$HERE/lgfx_obj"
BUILD="$HERE/build"
OUT="$HERE/out"

if [ ! -d "$L" ]; then
  echo "LovyanGFX sources not found. Run 'pio run' in $ROOT once, then try again." >&2
  exit 1
fi

# 1. LovyanGFX for the host (Linux framebuffer platform; only sprites are used). Built once.
if [ ! -f "$OBJ/.done" ]; then
  echo "Building LovyanGFX for the host (first run only)..."
  mkdir -p "$OBJ"
  for f in LGFXBase lgfx_fonts lgfx_v1 lgfx_v1_panel lgfx_v1_platforms; do
    g++ -std=gnu++17 -O1 -DLGFX_LINUX_FB -I"$L" -c "$L/lgfx/v1/$f.cpp" -o "$OBJ/$f.o"
  done
  for f in $(find "$L" -name "*.c"); do
    gcc -O1 -DLGFX_LINUX_FB -I"$L" -c "$f" -o "$OBJ/$(basename "$f").o"
  done
  touch "$OBJ/.done"
fi

# 2. Firmware sources with hardware/network parts swapped for stand-ins
rm -rf "$BUILD"
mkdir -p "$BUILD" "$OUT"
cp "$ROOT"/src/*.h "$ROOT"/src/*.cpp "$BUILD"/
rm -f "$BUILD"/main.cpp "$BUILD"/net.cpp "$BUILD"/board.cpp "$BUILD"/portal.cpp "$BUILD"/audio.cpp
python3 "$HERE/gen_display.py" "$ROOT/src" "$BUILD"
cp "$HERE"/overrides/*.cpp "$BUILD"/

echo "Compiling..."
g++ -std=gnu++17 -O1 -g -DLGFX_LINUX_FB -DSIM_OUT="\"$OUT\"" -DSIM_PHOTO="\"$HERE/sample_photo.jpg\"" \
  -I"$HERE/stubs" -I"$BUILD" -I"$L" -Wno-deprecated-declarations -Wno-format-truncation \
  "$BUILD"/*.cpp "$OBJ"/*.o -o "$BUILD/sim" -lpthread

# 3. Render and convert
rm -f "$OUT"/*.ppm "$OUT"/*.png
"$BUILD/sim"
python3 "$HERE/to_png.py" "$OUT"
