#!/bin/bash

set -e

mkdir build
cd build

cmake ..
sudo make

cd ..

make lint
