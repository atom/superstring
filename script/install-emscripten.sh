#!/usr/bin/env bash

set -e

EMSCRIPTEN_DOWNLOAD_URL='https://s3.amazonaws.com/mozilla-games/emscripten/releases/emsdk-portable.tar.gz'
EMSDK_PATH="./emsdk-portable/emsdk"

if [ ! -f $EMSDK_PATH ]; then
  echo 'Downloading emscripten SDK installer...'
  curl $EMSCRIPTEN_DOWNLOAD_URL | tar xz
fi

echo 'Installing emscripten SDK...'

$EMSDK_PATH update
$EMSDK_PATH list
$EMSDK_PATH install sdk-1.37.9-64bit
$EMSDK_PATH activate sdk-1.37.9-64bit
