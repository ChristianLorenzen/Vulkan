#!/bin/bash

# Exit if any command fails
set -e

cmake -S . -B build
cmake --build build -j

./bin/faye_app