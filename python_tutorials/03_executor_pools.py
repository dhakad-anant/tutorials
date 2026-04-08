"""
PYTHON CONCURRENCY — 03: Executor Pools & Futures
==================================================
For Java developers:

  ThreadPoolExecutor   ≈  ExecutorService (Executors.newFixedThreadPool)
  ProcessPoolExecutor  ≈  ForkJoinPool (true parallelism — bypasses GIL)
  Future               ≈  java.util.concurrent.Future
  as_completed()       ≈  CompletionService.take()
  wait()               ≈  invokeAll() / invokeAny()

The `concurrent.futures` module is the high-level, idiomatic way to run
work concurrently without managing threads directly — just like using
ExecutorService instead of raw Thread in Java.
"""

import concurrent.futures
import time
import random
import urllib.request


# ─────────────────────────────────────────────────────────────────────────────
# HELPERS
# ─────────────────────────────────────────────────────────────────────────────

def slow_square(n: int) -> int:
    """Simulate CPU work."""
    time.sleep(random.uniform(0.1, 0.4))
    return n * n

def fetch_url(url: str) -> tuple[str, int]:
    """Simulate I/O work (returns (url, content_length))."""
    time.sleep(random.uniform(0.2, 0.6))   # pretend network delay
    return url, random.randint(1000, 9999)


# ─────────────────────────────────────────────────────────────────────────────
# 1. ThreadPoolExecutor — submit() → Future
# ─────────────────────────────────────────────────────────────────────────────
#
# Java:
#   ExecutorService pool = Executors.newFixedThreadPool(4);
#   Future<Integer> f = pool.submit(() -> slowSquare(5));
#   int result = f.get();
#   pool.shutdown();
#
# Python — the `with` block calls shutdown(wait=True) automatically:

print("=== 1. ThreadPoolExecutor — submit() + Future ===")

with concurrent.futures.ThreadPoolExecutor(max_workers=4) as executor:
    # submit() returns a Future immediately (non-blocking)
    future = executor.submit(slow_square, 7)

    print(f"  Future done? {future.done()}")   # False — still running

    result = future.result()                   # blocks until done (like Future.get())
    print(f"  slow_square(7) = {result}")
    print(f"  Future done? {future.done()}\n") # True now


# ─────────────────────────────────────────────────────────────────────────────
# 2. map() — parallel equivalent of Java's invokeAll()
# ─────────────────────────────────────────────────────────────────────────────
#
# Java:
#   List<Future<Integer>> futures = pool.invokeAll(tasks);
#
# Python: executor.map(fn, iterable) — returns results in SUBMISSION order,
#         blocks only when you iterate. Like a parallel list comprehension.

print("=== 2. map() — Parallel Batch Work ===")

numbers = list(range(1, 9))
start = time.perf_counter()

with concurrent.futures.ThreadPoolExecutor(max_workers=4) as executor:
    results = list(executor.map(slow_square, numbers))  # blocks until all done

elapsed = time.perf_counter() - start
print(f"  Results: {results}")
print(f"  Elapsed: {elapsed:.2f}s  (sequential would be ~{0.25*len(numbers):.1f}s)\n")


# ─────────────────────────────────────────────────────────────────────────────
# 3. as_completed() — process results as they finish
# ─────────────────────────────────────────────────────────────────────────────
#
# Java:  ExecutorCompletionService — .take() returns the next completed Future
#
# Python: concurrent.futures.as_completed(futures) yields each Future
#         as it completes, regardless of submission order.
# Use this when you want to handle results ASAP rather than waiting for all.

print("=== 3. as_completed() — Handle Results as They Arrive ===")

urls = [f"https://example.com/page/{i}" for i in range(6)]

with concurrent.futures.ThreadPoolExecutor(max_workers=6) as executor:
    future_to_url = {executor.submit(fetch_url, url): url for url in urls}

    for future in concurrent.futures.as_completed(future_to_url):
        url = future_to_url[future]
        try:
            _, length = future.result()
            print(f"  {url} → {length} bytes")
        except Exception as exc:
            print(f"  {url} raised: {exc}")
print()


# ─────────────────────────────────────────────────────────────────────────────
# 4. Exception handling in Futures
# ─────────────────────────────────────────────────────────────────────────────
#
# Java: Future.get() throws ExecutionException wrapping the original exception.
# Python: future.result() re-raises the original exception directly. Cleaner!

print("=== 4. Exception Handling ===")

def risky_task(x: int) -> int:
    if x == 3:
        raise ValueError(f"Don't like {x}!")
    return x * 10

with concurrent.futures.ThreadPoolExecutor(max_workers=3) as executor:
    futures = {executor.submit(risky_task, i): i for i in range(5)}

    for f in concurrent.futures.as_completed(futures):
        try:
            print(f"  risky_task({futures[f]}) = {f.result()}")
        except ValueError as e:
            print(f"  risky_task({futures[f]}) FAILED: {e}")
print()


# ─────────────────────────────────────────────────────────────────────────────
# 5. wait() — invokeAll-style with fine-grained control
# ─────────────────────────────────────────────────────────────────────────────
#
# Java:  pool.invokeAll(tasks, timeout, unit)  waits for all tasks
# Python: concurrent.futures.wait(futures, timeout, return_when=...)
#
# return_when options:
#   ALL_COMPLETED   — wait for everything (like invokeAll)
#   FIRST_COMPLETED — return as soon as any one finishes (like invokeAny)
#   FIRST_EXCEPTION — return when any future raises

print("=== 5. wait() with FIRST_COMPLETED ===")

with concurrent.futures.ThreadPoolExecutor(max_workers=4) as executor:
    futures = [executor.submit(slow_square, i) for i in range(6)]

    done, not_done = concurrent.futures.wait(
        futures,
        return_when=concurrent.futures.FIRST_COMPLETED
    )
    print(f"  First finished: {[f.result() for f in done]}")
    print(f"  Still pending : {len(not_done)} futures")
print()


# ─────────────────────────────────────────────────────────────────────────────
# 6. ProcessPoolExecutor — TRUE parallelism (bypasses GIL)
# ─────────────────────────────────────────────────────────────────────────────
#
# Java:  ForkJoinPool / Executors.newWorkStealingPool()
# Python: ProcessPoolExecutor — spawns separate OS processes, each with its
#         own GIL. Use for CPU-bound work. API is IDENTICAL to ThreadPoolExecutor.
#
# GOTCHA: Functions and arguments must be picklable (serialized with pickle).
#         Lambdas, closures, and most local functions are NOT picklable.
#         Define the worker function at module top-level.

def cpu_bound_square(n: int) -> int:
    """Defined at module level so it can be pickled by ProcessPoolExecutor."""
    total = 0
    for _ in range(100_000):   # simulate real CPU work
        total += n
    return total

print("=== 6. ProcessPoolExecutor — CPU-Bound Parallelism ===")

# Guard required on Windows/macOS: spawned processes re-import __main__,
# so worker code must be inside  if __name__ == '__main__':
if __name__ == "__main__":
    nums = list(range(8))

    start = time.perf_counter()
    with concurrent.futures.ProcessPoolExecutor() as executor:   # defaults to cpu_count() workers
        results = list(executor.map(cpu_bound_square, nums))
    elapsed = time.perf_counter() - start

    print(f"  Results  : {results[:4]}…")
    print(f"  Elapsed  : {elapsed:.2f}s (using all CPU cores)\n")
else:
    print("  (Skipped in import context — run this file directly)\n")


# ─────────────────────────────────────────────────────────────────────────────
# 7. Choosing ThreadPoolExecutor vs ProcessPoolExecutor
# ─────────────────────────────────────────────────────────────────────────────
#
#  ┌──────────────────────────────┬──────────────────────┬──────────────────────┐
#  │ Workload                     │ Use                  │ Java equiv           │
#  ├──────────────────────────────┼──────────────────────┼──────────────────────┤
#  │ Network / DB / file I/O      │ ThreadPoolExecutor   │ Executors.newFixed…  │
#  │ CPU: number crunching, image │ ProcessPoolExecutor  │ ForkJoinPool         │
#  │ CPU: needs shared state       │ multiprocessing +   │ ForkJoinPool +       │
#  │                              │ Manager/Queue        │ ConcurrentHashMap    │
#  └──────────────────────────────┴──────────────────────┴──────────────────────┘
print("=== 7. Quick Rule of Thumb ===")
print("  I/O-bound  → ThreadPoolExecutor  (threads share memory, low overhead)")
print("  CPU-bound  → ProcessPoolExecutor (separate processes, true parallelism)")
