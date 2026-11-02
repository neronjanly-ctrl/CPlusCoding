# D2 process and pipe experiment

## Problem

Observe how `fork`, `exec`, `waitpid`, pipes, and `dup2` change process and file
descriptor state. The programs also make child exit status part of their pass
condition.

## Programs

- `process_demo` creates one child, optionally holds it for `ps`, `pstree`, and
  `lsof`, then verifies exit code 7.
- `exec_demo` demonstrates that a successful `exec` replaces the child program
  and never returns to the old code.
- `pipe_demo` sends bytes from child to parent, closes unused pipe ends, handles
  `EINTR`, and waits for the child.
- `pipe_exec_demo` redirects the child's standard output to a pipe with `dup2`.
- `pipeline_demo` connects `/bin/echo` to `/usr/bin/wc -c` using two children.

## Acceptance boundary

The raw output and trace demonstrate the normal path, exit-state recovery, and
descriptor closure used by these examples. They do not independently inject
fork failure, FD exhaustion, or every signal timing. Zombie/orphan behavior and
the consequences of retaining an extra pipe write end still require personal
explanation.
