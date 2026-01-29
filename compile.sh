#!/bin/bash

cd src
make
cd ..
mkdir -p build
cd build
cmake ..
make
cd ..
