#!/bin/sh
#Compress our fonts and other stuff
python3 /Volumes/MuhBoook/emsdk/upstream/emscripten/tools/file_packager.py ancillary-data.data --js-output=ancillary-data-data.js --preload ../share/yeoldedink@yeoldedink --lz4
