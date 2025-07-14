#!/bin/sh
#Compress a dataset so it will be found by the engine
#change the path for file packer and data location to your own
python3 /Volumes/MuhBoook/emsdk/upstream/emscripten/tools/file_packager.py freedink-data.data --js-output=freedink-data-data.js --preload ~/Downloads/freedink-data-1.08.20190120/dink@/usr/share/dink/dink --lz4
