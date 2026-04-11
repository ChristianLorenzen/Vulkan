#!/bin/bash
quill_url="https://github.com/slab/quill.git"
quill_dest="src/include/quill"

assimp_url="https://github.com/assimp/assimp.git"
assimp_dest="src/include/assimp"

# Exit if any command fails
set -e

if [ ! -d "$quill_dest" ]; then
    echo "Cloning quill..."
    git clone --depth 1 $quill_url $quill_dest
else
    echo "Quill already exists, skipping clone."
fi

if [ ! -d "$assimp_dest" ]; then
    echo "Cloning assimp..."
    git clone --depth 1 $assimp_url $assimp_dest
else
    echo "Assimp already exists, skipping clone."
fi

cmake -S . -B build
cmake --build build -j

./bin/faye_app