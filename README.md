# CPlusCoding

This repository contains the implementation and reproducible evidence for a
Tencent-oriented C++/Linux backend learning project.

## Repository layout

| Path | Purpose | Current state |
|---|---|---|
| `cpp-linux-labs/` | Linux system-programming foundations | Active through D6 debugging labs |
| `mininet/` | Reactor-style network library | Planned; no implementation yet |
| `linkcore/` | Short-link and hot-query service | Planned; no implementation yet |
| `edge-gateway/` | Multi-instance gateway and resilience experiments | Planned; no implementation yet |

The Git root is `/home/neron/workspace/tencent-cpp`. Build directories are
created under `build/` and are not committed.

## Source of truth

The current learning plan and progress ledger are maintained in the Windows
workspace:

- `D:/CPlusTwoProjects/docs/tencent-cpp-10-week-master-plan.md`
- `D:/CPlusTwoProjects/docs/learning-progress.md`
- `D:/CPlusTwoProjects/docs/project-memory.md`

This repository stores code, tests, runnable instructions, and original
evidence. A source file or a passing command does not by itself prove personal
understanding; the progress ledger records that separately.

## Build and test

Run all commands from the repository root.

```bash
cmake --preset debug
cmake --build --preset debug --parallel 2
ctest --preset debug
```

Other supported presets are `release`, `asan`, and `tsan`:

```bash
cmake --preset asan
cmake --build --preset asan --parallel 2
ctest --preset asan
```

The `tsan` preset also builds the intentionally unsafe `race_unsafe_demo`. It
is excluded from CTest because a ThreadSanitizer report is its expected
teaching result. The CTest wrapper uses process-local `setarch ... -R` on WSL;
it does not change a global kernel setting. See `cpp-linux-labs/README.md`
before running the unsafe target.

## Repository rules

1. Keep implementation, tests, scripts, and raw reproduction evidence in the
   project that owns them.
2. Do not commit build directories, core dumps, credentials, database data, or
   copied learning-management documents.
3. A test must return a nonzero exit code on failure; printing `FAIL` alone is
   not sufficient.
4. Record environment, command, output, and known limitations for important
   experiments.
5. Commit only a coherent verified change. Do not mark later projects complete
   merely because their directories exist.
