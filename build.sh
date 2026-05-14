#!/bin/bash
# build.sh — Build coping from iputils source
#
# Prerequisites:
#   sudo apt-get install -y gcc make meson ninja-build \
#       libcap-dev libcurl4-openssl-dev pkg-config gettext
#
# Usage:
#   ./build.sh           — build coping binary
#   ./build.sh mechanisms — also compile example mechanism .so files
#   ./build.sh clean      — wipe build directory

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
IPUTILS_DIR="$SCRIPT_DIR/iputils"
BUILD_DIR="$IPUTILS_DIR/build"
MECH_SRC_DIR="$SCRIPT_DIR/mechanism_src"
MECH_OUT_DIR="$SCRIPT_DIR/mechanisms"

case "${1:-}" in
  clean)
    echo "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
    echo "Done."
    exit 0
    ;;

  mechanisms)
    echo "Building mechanisms (shared libraries)..."
    mkdir -p "$MECH_OUT_DIR"
    for src in "$MECH_SRC_DIR"/*.c; do
      name=$(basename "$src" .c)
      out="$MECH_OUT_DIR/${name}.so"
      echo "  $name.c -> $name.so"
      gcc -shared -fPIC -o "$out" "$src"
    done
    echo "Done. Mechanisms in: $MECH_OUT_DIR/"
    exit 0
    ;;
esac

# Main build
echo "Building coping from iputils..."
cd "$IPUTILS_DIR"

if [ ! -d "$BUILD_DIR" ]; then
  meson setup "$BUILD_DIR" \
    -DBUILD_PING=true \
    -DBUILD_ARPING=false \
    -DBUILD_CLOCKDIFF=false \
    -DBUILD_TRACEPATH=false \
    -DBUILD_MANS=false \
    -DBUILD_HTML_MANS=false \
    -DSKIP_TESTS=true \
    --prefix="$SCRIPT_DIR/install"
fi

ninja -C "$BUILD_DIR"

echo ""
echo "Build complete!"
echo "Binary: $BUILD_DIR/ping/coping"
echo ""
echo "Run it:"
echo "  export ANTHROPIC_API_KEY=sk-ant-..."
echo "  sudo $BUILD_DIR/ping/coping google.com"
echo ""
echo "Or without AI context (no API key needed):"
echo "  sudo $BUILD_DIR/ping/coping --no-context google.com"
echo ""
echo "With a mechanism:"
echo "  ./build.sh mechanisms"
echo "  sudo $BUILD_DIR/ping/coping --mechanism existential google.com"
