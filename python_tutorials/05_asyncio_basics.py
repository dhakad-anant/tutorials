"""
PYTHON CONCURRENCY — 05: asyncio Basics
=========================================
For Java developers:

asyncio is Python's answer to reactive/async programming.
The closest Java equivalents are:
  - CompletableFuture chain (.thenApply, .thenCompose)
  - Project Reactor / RxJava (Mono, Flux)
  - Java 21+ virtual threads (closest mental model)

KEY CONCEPT: asyncio is SINGLE-THREADED cooperative concurrency.
There is ONE thread running an EVENT LOOP. Tasks voluntarily yield
control at `await` points — when one task is waiting for I/O, the
event loop runs another task.

  threading / multiprocessing = PREEMPTIVE (OS decides switching)
  asyncio                     = COOPERATIVE (code decides switching via await)

This means:
  ✅ Great for I/O-bound work (many concurrent network/DB calls)
  ✅ No race conditions on pure Python data (single-threaded!)
  ❌ A CPU-blocking call (tight loop, sleep()) freezes the ENTIRE event loop
  ❌ You must use await-friendly libraries (aiohttp, asyncpg, etc.)

Timeline comparison for 3 network calls (200ms each):
  Sequential threading:  600ms
  threading.Thread x3:   ~200ms  (GIL released during I/O)
  asyncio (3 tasks):     ~200ms  (single thread, cooperative)
"""

import asyncio
import time
import random


# ─────────────────────────────────────────────────────────────────────────────
# 1. BASIC COROUTINE
# ─────────────────────────────────────────────────────────────────────────────
#
# Python: `async def` defines a coroutine (like a Mono<T> factory in Reactor,
#         or a method returning CompletableFuture).
#         Calling it does NOT run it — it returns a coroutine object.
#         You must await it (or schedule it as a Task).
#
# Java analogy:
#   CompletableFuture<String> greetAsync(String name) {
#       return CompletableFuture.supplyAsync(() -> {
#           Thread.sleep(100); return "Hello " + name;
#       });
#   }
#   greetAsync("Alice").join();  // = await greet("Alice")

async def greet(name: str) -> str:
    print(f"  [greet] starting for {name}")
    await asyncio.sleep(0.1)       # yields control back to event loop (like Thread.sleep but non-blocking)
    message = f"Hello, {name}!"
    print(f"  [greet] done for {name}")
    return message

# asyncio.run() creates the event loop, runs the coroutine, then closes the loop.
# Java equivalent: CompletableFuture.join() on the outermost call.
print("=== 1. Basic Coroutine ===")
result = asyncio.run(greet("Alice"))
print(f"  Result: {result}\n")


# ─────────────────────────────────────────────────────────────────────────────
# 2. SEQUENTIAL vs CONCURRENT AWAIT
# ─────────────────────────────────────────────────────────────────────────────
#
# CRITICAL MISTAKE beginners make: awaiting one by one = sequential!
#
# Java analogy:
#   // BAD (sequential):
#   String a = fetchA().join();
#   String b = fetchB().join();
#
#   // GOOD (concurrent):
#   CompletableFuture<String> fa = fetchA();
#   CompletableFuture<String> fb = fetchB();
#   String a = fa.join();  String b = fb.join();

async def fetch(name: str, delay: float) -> str:
    print(f"    [fetch] {name} started")
    await asyncio.sleep(delay)          # simulate network I/O
    print(f"    [fetch] {name} done")
    return f"data-{name}"

async def demo_sequential() -> None:
    start = time.perf_counter()
    a = await fetch("A", 0.3)    # waits 0.3s, THEN starts B
    b = await fetch("B", 0.3)    # waits another 0.3s
    print(f"  Sequential: {a}, {b} — took {time.perf_counter()-start:.2f}s")

async def demo_concurrent() -> None:
    start = time.perf_counter()
    # asyncio.gather() runs all coroutines CONCURRENTLY — like CompletableFuture.allOf()
    a, b = await asyncio.gather(fetch("A", 0.3), fetch("B", 0.3))
    print(f"  Concurrent: {a}, {b} — took {time.perf_counter()-start:.2f}s")

print("=== 2. Sequential vs Concurrent await ===")
asyncio.run(demo_sequential())   # ~0.6s
asyncio.run(demo_concurrent())   # ~0.3s
print()


# ─────────────────────────────────────────────────────────────────────────────
# 3. TASKS  (fire-and-forget / background work)
# ─────────────────────────────────────────────────────────────────────────────
#
# Java: CompletableFuture.supplyAsync(task)  — scheduled, runs in background
# Python: asyncio.create_task(coro)          — schedules coroutine as a Task
#
# Unlike gather(), create_task() starts the task immediately and lets you
# do other work before awaiting it.
# Tasks also show up in the event loop's task list (like thread introspection).

async def background_job(job_id: int) -> str:
    await asyncio.sleep(random.uniform(0.1, 0.5))
    return f"job-{job_id}-result"

async def demo_tasks() -> None:
    print("  Creating tasks…")
    task1 = asyncio.create_task(background_job(1), name="job-1")
    task2 = asyncio.create_task(background_job(2), name="job-2")
    task3 = asyncio.create_task(background_job(3), name="job-3")

    # do some other work here while tasks run concurrently
    await asyncio.sleep(0.05)
    print(f"  task1 done? {task1.done()}")

    results = await asyncio.gather(task1, task2, task3)
    print(f"  All results: {results}")

print("=== 3. Tasks ===")
asyncio.run(demo_tasks())
print()


# ─────────────────────────────────────────────────────────────────────────────
# 4. TIMEOUTS
# ─────────────────────────────────────────────────────────────────────────────
#
# Java: future.get(timeout, TimeUnit.SECONDS)  → throws TimeoutException
# Python: asyncio.wait_for(coro, timeout)      → raises asyncio.TimeoutError

async def slow_operation() -> str:
    await asyncio.sleep(2.0)
    return "done"

async def demo_timeout() -> None:
    try:
        result = await asyncio.wait_for(slow_operation(), timeout=0.5)
        print(f"  Result: {result}")
    except asyncio.TimeoutError:
        print("  Timed out after 0.5s (operation needed 2.0s)")

print("=== 4. Timeouts ===")
asyncio.run(demo_timeout())
print()


# ─────────────────────────────────────────────────────────────────────────────
# 5. TASK CANCELLATION
# ─────────────────────────────────────────────────────────────────────────────
#
# Java: Future.cancel(true) /  Thread.interrupt()
# Python: task.cancel() injects a CancelledError at the next await point.
#         The coroutine can catch it for cleanup (like InterruptedException).

async def cancellable_work(name: str) -> None:
    try:
        print(f"  [{name}] working…")
        await asyncio.sleep(5.0)
        print(f"  [{name}] finished")
    except asyncio.CancelledError:
        print(f"  [{name}] was cancelled — cleaning up")
        raise   # IMPORTANT: always re-raise CancelledError (like InterruptedException)

async def demo_cancellation() -> None:
    task = asyncio.create_task(cancellable_work("worker"))
    await asyncio.sleep(0.2)     # let task start
    task.cancel()
    try:
        await task
    except asyncio.CancelledError:
        print("  Task cancellation confirmed")

print("=== 5. Cancellation ===")
asyncio.run(demo_cancellation())
print()


# ─────────────────────────────────────────────────────────────────────────────
# 6. EXCEPTION HANDLING WITH gather()
# ─────────────────────────────────────────────────────────────────────────────
#
# Java: CompletableFuture.allOf() doesn't propagate exceptions automatically.
#
# asyncio.gather():
#   return_exceptions=False (default) — first exception cancels others & re-raises
#   return_exceptions=True            — collects exceptions as return values (like allSettled)

async def maybe_fail(n: int) -> int:
    await asyncio.sleep(0.1)
    if n == 2:
        raise ValueError(f"Task {n} failed!")
    return n * 10

async def demo_gather_exceptions() -> None:
    # return_exceptions=True: like Promise.allSettled() in JS
    results = await asyncio.gather(
        *[maybe_fail(i) for i in range(5)],
        return_exceptions=True
    )
    for i, r in enumerate(results):
        if isinstance(r, Exception):
            print(f"  Task {i} raised: {r}")
        else:
            print(f"  Task {i} returned: {r}")

print("=== 6. gather() with return_exceptions=True ===")
asyncio.run(demo_gather_exceptions())
print()


# ─────────────────────────────────────────────────────────────────────────────
# 7. THE GOLDEN RULE: Don't Block the Event Loop
# ─────────────────────────────────────────────────────────────────────────────
#
# This is the #1 mistake when coming from Java's threaded model.
#
# NEVER call blocking code directly in a coroutine:
#   time.sleep(1)       ← blocks the ENTIRE event loop for 1s!
#   requests.get(url)   ← same problem
#   heavy computation   ← same problem
#
# Solutions:
#   - Use async-native libs:  asyncio.sleep, aiohttp, asyncpg, aiofiles
#   - Offload blocking I/O to a thread pool:   loop.run_in_executor()
#   - Offload CPU work to a process pool:      loop.run_in_executor(ProcessPoolExecutor)

import concurrent.futures

def blocking_io_call(url: str) -> str:
    """Old-style blocking function — can't be made async."""
    time.sleep(0.2)                         # simulates blocking I/O
    return f"response from {url}"

async def demo_run_in_executor() -> None:
    loop = asyncio.get_event_loop()
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        # Offload blocking call to thread pool — event loop stays responsive
        result = await loop.run_in_executor(pool, blocking_io_call, "https://api.example.com")
        print(f"  Got: {result}")

print("=== 7. run_in_executor() — Bridging Blocking Code ===")
asyncio.run(demo_run_in_executor())
