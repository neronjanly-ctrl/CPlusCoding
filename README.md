# cpp-linux-labs

Linux C++ system-programming experiments for the Tencent backend interview roadmap.

## Environment

- WSL2
- Ubuntu 24.04.4 LTS
- GCC 13.3.0
- CMake 3.28.3
- Ninja 1.11.1
- GDB 15.1
- Clang/clang-format 18.1.3
- C++ standard: C++17

## Build

### Debug

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
./build/hello
```

### Release

```bash
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
./build-release/hello
```

### ASan and UBSan

```bash
cmake -S . -B build-asan \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DENABLE_SANITIZERS=ON

cmake --build build-asan
./build-asan/hello
```

## Week 1 progress

- [x] Configure the WSL C++ toolchain
- [x] Create a minimal C++17 CMake project
- [x] Enable compiler warnings
- [x] Verify Debug and Release builds
- [x] Verify ASan and UBSan linkage
- [ ] Implement file-descriptor and error-handling experiments
