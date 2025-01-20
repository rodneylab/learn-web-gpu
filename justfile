alias r := run

# build all targets
build:
    cmake -B .build -DDEV_MODE=ON -DCMAKE_BUILD_TYPE=Debug

# release build
build-release:
    cmake --build .build-release -DDEV_MODE=OFF -DCMAKE_BUILD_TYPE=Release

# remove build artefacts
clean:
    cd .build && ninja clean

# debug build all targets
debug-build:
    cmake --build .build --config Debug

# generate CMake build
generate:
    cmake -H. -B.build -GNinja -DCMAKE_C_COMPILER=$C_COMPILER -DCMAKE_CXX_COMPILER=$CXX_COMPILER

# run main binary
run: build
    ./.build/bin/App
