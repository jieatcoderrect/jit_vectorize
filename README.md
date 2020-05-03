# jit_vectorize
Performance comparison of jit and vectorization implementations

# Simple test
mkdir cmake_build_release
cd cmake_build_release
cmake -DCMAKE_BUILD_TYPE=Release ..
make
time ./project
time taskset 0x1 ./project