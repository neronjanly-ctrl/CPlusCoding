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
| D4 | `mmap_demo`, `signal_demo`, `pipe_duplex_demo`, `socketpair_demo` | shared mapping, process-shared mutex, signals, local IPC comparison |
| D5 | `tcp_echo_server/client`, `tcp_framed_server/client`, `tcp_protocol_test` | blocking TCP, byte-stream framing, partial I/O, half-close, protocol limits |

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

## D4: Linux IPC and Resource Management

### Experiments

| Experiment | Source | Main concepts |
|---|---|---|
| Shared memory | `src/mmap_demo.cpp` | mmap, MAP_SHARED, process-shared mutex |
| Signals | `src/signal_demo.cpp` | sigaction, sigprocmask, sigsuspend |
| Duplex Pipe | `src/pipe_duplex_demo.cpp` | bidirectional IPC, FD lifecycle, EOF |
| Unix Domain Socket | `src/socketpair_demo.cpp` | socketpair, AF_UNIX, SOCK_STREAM |

### Documentation

See:

- [D4 shared memory](docs/d4-mmap.md)
- [D4 signal learning](docs/d4-signal-learning.md)
- [D4 IPC comparison](docs/d4-ipc-comparison.md)

### Build and Test

```bash
cmake --build build-debug --parallel 2

ctest --test-dir build-debug \
    -L D4 --output-on-failure
```

## D5: Blocking TCP and Length-Prefixed Frames

`tcp_echo_server` and `tcp_echo_client` are the initial EOF-delimited blocking
echo experiment. `tcp_framed_server` and `tcp_framed_client` use a four-byte
network-order payload length so one connection can carry multiple messages.
The framing helpers live in `src/tcp_protocol.h`.

Build and run the deterministic protocol tests:

```bash
cmake --preset debug
cmake --build --preset debug --parallel 2
ctest --preset debug -L D5 --output-on-failure
```

Run the TCP integration experiment in two terminals:

```bash
# Terminal A
./build/debug/cpp-linux-labs/tcp_framed_server

# Terminal B
./build/debug/cpp-linux-labs/tcp_framed_client
```

The client verifies 1,000 sequential request/response frames on one connection,
then half-closes its sending direction and waits for the server's EOF. The
protocol test separately covers fragmented input, a combined multi-frame byte
stream, the exact 1 MiB limit, oversized frames, truncated frames, and safe
closed-peer sends. See [the D5 evidence record](docs/d5-tcp-framing-output.txt)
for commands, results, and limitations.
