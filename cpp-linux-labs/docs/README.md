# Evidence index

These files are original outputs from the local WSL environment. They prove the
recorded commands ran with the recorded result; they do not prove every error
path or personal understanding.

| Day | Explanation | Raw evidence |
|---|---|---|
| D1 | `d1-file-io.md` | `d1-file-io-output.txt`, `d1-file-io.strace` |
| D2 | `d2-process-pipe.md` | `d2-process-pipe-output.txt`, `d2-pipeline.strace` |
| D3 | `d3-threading.md` | `d3-thread-output.txt`, `d3-tsan-unsafe.txt` |
| D4 | `d4-mmap.md`, `d4-signal-learning.md` | `d4-mmap-output.txt`, `d4-signal-output.txt`, `d4-signal.strace` |

Refresh D2-D4 evidence only after building the root `debug` and `tsan` presets:

```bash
./cpp-linux-labs/scripts/capture-week1-evidence.sh
```
