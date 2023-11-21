#!/bin/bash

cmake -DCMAKE_INSTALL_PREFIX="path/to/save" -DCMAKE_TOOLCHAIN_FILE="<path/to/vcpkg>/scripts/buildsystems/vcpkg.cmake" ..
cmake --build . 
cmake --install . --config Debug