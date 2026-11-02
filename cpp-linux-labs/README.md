# cpp-linux-labs

Small C++17 programs for observing Linux resource behavior before building the
MiniNet network library. Each program has one teaching purpose and returns a
nonzero status when its checked expectation fails.

## Build from the repository root

```bash
cd /home/neron/workspace/tencent-cpp
cmake --preset debug
cmake --build --preset debug --parallel 2
ctest --preset debug
```

Executables are placed under `build/debug/cpp-linux-labs/`.

## Lab map

| Day | Targets | Focus |
|---|---|---|
| D1 | `file_io_demo` | FD lifecycle, file offset, EOF, `errno` |
| D2 | `process_demo`, `exec_demo`, `pipe_demo`, `pipe_exec_demo`, `pipeline_demo` | `fork/exec/wait`, pipes, `dup2`, exit status |
| D3 | `producer_consumer_demo`, `race_demo`, `race_unsafe_demo` | condition variables, bounded queues, race detection and repair |
| D4 | `mmap_demo` | shared anonymous mapping and process-shared mutex |

`race_unsafe_demo` is intentionally excluded from normal builds and tests. It
exists only to produce a TSan report in the opt-in `tsan` preset.

## Sanitizer builds

```bash
cd /home/neron/workspace/tencent-cpp

cmake --preset asan
cmake --build --preset asan --parallel 2
ctest --preset asan

cmake --preset tsan
cmake --build --preset tsan --parallel 2
ctest --preset tsan
setarch x86_64 -R ./build/tsan/cpp-linux-labs/race_unsafe_demo
```

The unsafe command is expected to return nonzero when TSan detects the race.
Keep that output as evidence; do not add the unsafe target to CTest as a normal
passing test. The process-local `setarch` option avoids a known WSL address-map
collision with GCC TSan and does not change global kernel configuration.

## Evidence

Original output and system-call traces live in `docs/`. Evidence proves what a
particular build executed in the recorded environment. It does not prove that
all error paths were injected or that the learner can independently explain the
result.

## Environment snapshot

- WSL2, Ubuntu 24.04.4 LTS
- GCC 13.3.0
- CMake 3.28.3
- Ninja 1.11.1
- GDB 15.1
- Clang/clang-format 18.1.3

Recheck versions after the environment changes; this list is not a permanent
guarantee.
