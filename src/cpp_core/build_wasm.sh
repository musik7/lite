#!/bin/bash
# ==============================================================================
# Script Compile C++ Engine Chromium DevTools ke WebAssembly (Emscripten)
# ==============================================================================

# Output build directory
OUTPUT_DIR="public/wasm"
mkdir -p $OUTPUT_DIR

echo "[1/2] Compiling C++ Core to WebAssembly using emcc..."

# Flag emcc untuk performa tinggi & multi-threading support (SharedArrayBuffer)
emcc src/cpp_core/main.cpp \
  -Isrc/cpp_core \
  -O3 \
  -std=c++17 \
  -s WASM=1 \
  -s ALLOW_MEMORY_GROWTH=1 \
  -s EXPORTED_RUNTIME_METHODS='["cwrap", "ccall", "getValue", "setValue", "UTF8ToString", "stringToUTF8"]' \
  -s EXPORT_ES6=1 \
  -s MODULARIZE=1 \
  -s EXPORT_NAME="DevToolsCoreModule" \
  -s USE_PTHREADS=1 \
  -s PTHREAD_POOL_SIZE=4 \
  -o $OUTPUT_DIR/devtools_core.js

echo "[2/2] Build WebAssembly selesai!"
echo "Artifacts generated in $OUTPUT_DIR/:"
echo " - devtools_core.js (ES6 Glue Module)"
echo " - devtools_core.wasm (WebAssembly Binary)"
