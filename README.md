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
cmake -S . -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug
./build-debug/hello
./build-debug/file_io_demo
ctest --test-dir build-debug --output-on-failure
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
./build-asan/file_io_demo
ctest --test-dir build-asan --output-on-failure
```

## Week 1 progress

- [x] Configure the WSL C++ toolchain
- [x] Create a minimal C++17 CMake project
- [x] Enable compiler warnings
- [x] Verify Debug and Release builds
- [x] Verify ASan and UBSan linkage
- [x] Implement and run file-descriptor, file-offset, and ENOENT experiments
- [ ] Independently explain the file-offset and errno results

## D1 file I/O evidence

See [experiment explanation and acceptance criteria](docs/d1-file-io.md),
[captured output](docs/d1-file-io-output.txt), and
[system-call trace](docs/d1-file-io.strace).

The demo truncates `/tmp/cpp_linux_labs_file_io.txt` on each run and expects
`/tmp/cpp_linux_labs_missing_for_errno.txt` to be absent. Run one copy at a time.
The existing `build/` directory uses Unix Makefiles and has no build type
selected. The `build-debug/` directory provides the regular Ninja Debug
configuration, while `build-clean/` was used to verify configuration,
compilation, linking, execution, and CTest from a separate build directory.
