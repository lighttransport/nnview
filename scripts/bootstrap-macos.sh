#!/bin/bash

rm -rf build
mkdir build

CXX=clang++ CC=clang cmake -Bbuild -H. -DNNVIEW_USE_CCACHE=Off -DNNVIEW_USE_NATIVEFILEDIALOG=On
