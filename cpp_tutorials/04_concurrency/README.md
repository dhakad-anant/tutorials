# Module 04: Concurrency & Multithreading

Critical for storage systems where I/O parallelism drives throughput.

## Topics
1. [Threads, Mutexes & Condition Variables](01_threads_mutexes.cpp)
2. [Atomics & Lock-Free Programming](02_atomics_lockfree.cpp)
3. [Thread Pool & Async Patterns](03_thread_pool.cpp)

## Key Rule
If two threads can access the same data and at least one writes, you MUST synchronize.
No exceptions. Data races are undefined behavior.

---
*After completing, move to [Module 05: Systems Programming](../05_systems_programming/README.md) →*
