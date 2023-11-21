cmake -DCMAKE_INSTALL_PREFIX="path/to/deploy" -DCMAKE_TOOLCHAIN_FILE="<path/to/vcpkg>/vcpkg/scripts/buildsystems/vcpkg.cmake" ..
cmake --build . 
cmake --install . --config Debug