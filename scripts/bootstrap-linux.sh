#!/bin/bash

rm -rf build
mkdir build

CXX=clang++ cmake -Bbuild -H. -DNNVIEW_USE_CCACHE=On -DNNVIEW_USE_NATIVEFILEDIALOG=On
