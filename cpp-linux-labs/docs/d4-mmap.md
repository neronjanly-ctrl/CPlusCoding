# D4 shared-memory synchronization experiment

`mmap_demo` creates a shared anonymous mapping containing a counter and a
process-shared pthread mutex. After `fork`, parent and child each perform
100000 protected increments. The parent retries `waitpid` on `EINTR`, checks the
child status, verifies the final value, destroys the mutex, and unmaps memory.

Pthread functions return an error number directly, so their return values are
reported with `strerror`; they are not treated as functions that set `errno`.

The output proves the normal synchronized path in this environment. It does not
cover owner death, robust mutex recovery, file-backed mappings, or crash-time
cleanup.
