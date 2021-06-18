#!/usr/bin/env bash

set -e

EMSCRIPTEN_DOWNLOAD_URL='https://codeload.github.com/emscripten-core/emsdk/tar.gz/2.0.9'
EMSDK_PATH="./emsdk-2.0.9/emsdk"

if [ ! -f $EMSDK_PATH ]; then
  echo 'Downloading emscripten SDK installer...'
  curl $EMSCRIPTEN_DOWNLOAD_URL | tar xz
fi

echo 'Installing emscripten SDK...'

$EMSDK_PATH update
$EMSDK_PATH install 2.0.9
$EMSDK_PATH activate 2.0.9 --permanent
