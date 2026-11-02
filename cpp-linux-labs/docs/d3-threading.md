# D3 threading experiment

## Safe path

`producer_consumer_demo` uses a capacity-three queue, two condition variables,
predicate waits, and an explicit producer-complete flag. `race_demo` protects
the shared counter with a mutex and checks for the expected total.

## TSan comparison

`race_unsafe_demo` deliberately increments the same non-atomic global from two
threads. It is opt-in and excluded from normal CTest. In WSL, GCC TSan requires
process-local ASLR disablement with `setarch x86_64 -R` to avoid a shadow-memory
mapping collision. This does not modify a global sysctl.

The unsafe report identifies concurrent accesses in `worker`; the fixed run
completes without a TSan warning. This is a controlled teaching case, not
permission to keep undefined behavior in production targets.
