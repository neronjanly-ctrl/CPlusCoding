#!/usr/bin/env bash

set -euo pipefail

repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
binary_dir="$repo_root/build/debug/cpp-linux-labs"
tsan_dir="$repo_root/build/tsan/cpp-linux-labs"
docs_dir="$repo_root/cpp-linux-labs/docs"

if [[ ! -x "$binary_dir/process_demo" ]]; then
    echo "Debug binaries are missing. Run the root debug preset first." >&2
    exit 1
fi

if [[ ! -x "$tsan_dir/race_unsafe_demo" ]]; then
    echo "TSan binaries are missing. Run the root tsan preset first." >&2
    exit 1
fi

{
    echo "command: process_demo --hold-seconds 3 plus ps/pstree/lsof"
    echo "kernel: $(uname -srmo)"
    echo

    "$binary_dir/process_demo" --hold-seconds 3 &
    demo_pid=$!
    sleep 0.2
    child_pid=$(pgrep -P "$demo_pid" | head -n 1)

    echo "--- ps ---"
    COLUMNS=160 LINES=40 ps -o pid,ppid,stat,cmd -p "$demo_pid,$child_pid"
    echo "--- pstree ---"
    pstree -p "$demo_pid"
    echo "--- lsof standard descriptors ---"
    if command -v lsof >/dev/null 2>&1; then
        lsof -a -p "$demo_pid,$child_pid" -d 0,1,2
    else
        echo "lsof is not installed"
    fi
    wait "$demo_pid"

    echo
    echo "--- exec_demo ---"
    "$binary_dir/exec_demo"
    echo "--- pipe_demo ---"
    "$binary_dir/pipe_demo"
    echo "--- pipe_exec_demo ---"
    "$binary_dir/pipe_exec_demo"
    echo "--- pipeline_demo ---"
    "$binary_dir/pipeline_demo"
} >"$docs_dir/d2-process-pipe-output.txt" 2>&1

strace -f -yy \
    -e trace=clone,execve,wait4,pipe,pipe2,dup2,close,read,write \
    -o "$docs_dir/d2-pipeline.strace" \
    "$binary_dir/pipeline_demo" >/dev/null

{
    echo "--- producer_consumer_demo ---"
    "$binary_dir/producer_consumer_demo"
    echo "--- race_demo (fixed, TSan) ---"
    setarch x86_64 -R "$tsan_dir/race_demo"
} >"$docs_dir/d3-thread-output.txt" 2>&1

set +e
setarch x86_64 -R "$tsan_dir/race_unsafe_demo" \
    >"$docs_dir/d3-tsan-unsafe.txt" 2>&1
unsafe_status=$?
set -e
echo "exit_status=$unsafe_status" >>"$docs_dir/d3-tsan-unsafe.txt"
if [[ $unsafe_status -eq 0 ]]; then
    echo "Expected the unsafe TSan demo to return nonzero." >&2
    exit 1
fi

{
    echo "--- mmap_demo ---"
    "$binary_dir/mmap_demo"
} >"$docs_dir/d4-mmap-output.txt" 2>&1

echo "Evidence refreshed under $docs_dir"
