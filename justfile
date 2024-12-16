alias r := run

# build all targets
build:
    cmake --build .build

# debug build all targets
debug-build:
    cmake --build .build --config Debug

# generate CMake build
generate:
    cmake -H. -B.build -GNinja -DCMAKE_C_COMPILER=$C_COMPILER -DCMAKE_CXX_COMPILER=$CXX_COMPILER

# run main binary
run: build
    ./.build/bin/App
