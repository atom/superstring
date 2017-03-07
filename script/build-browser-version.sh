#!/usr/bin/env bash

EM_COMPILER_PATH=$(find emsdk_portable -name em++ | head -n1)

echo "Running ${EM_COMPILER_PATH}"
${EM_COMPILER_PATH}                     \
  --bind                                \
  -o browser.js                         \
  -O3                                   \
  -std=c++14                            \
  -I src/bindings/em                    \
  -I src/core                           \
  --pre-js src/bindings/em/prologue.js  \
  --post-js src/bindings/em/epilogue.js \
  src/core/*.cc                         \
  src/bindings/em/*.cc                  \
  -s TOTAL_MEMORY=134217728             \
  --memory-init-file 0                  \
