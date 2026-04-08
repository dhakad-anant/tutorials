"""
PYTHON CONCURRENCY — 06: asyncio Advanced Patterns
====================================================
For Java developers — more complex asyncio patterns:

  asyncio.Queue         ≈  LinkedBlockingQueue (async-friendly)
  asyncio.Lock          ≈  ReentrantLock (async-friendly)
  asyncio.Semaphore     ≈  Semaphore (async-friendly)
  asyncio.Event         ≈  CountDownLatch(1) (async-friendly)
  asyncio.TaskGroup     ≈  StructuredTaskScope (Java 21+)
  asyncio.as_completed  ≈  CompletionService (async version)

Note: asyncio has its OWN versions of Lock, Queue, etc. — these are
NOT thread-safe across multiple OS threads, but they're safe within
the single-threaded event loop (and much faster than thread primitives).
"""

import asyncio
import time
import random
from collections.abc import AsyncIterator


# ─────────────────────────────────────────────────────────────────────────────
# 1. asyncio.Queue  (async producer / consumer)
# ─────────────────────────────────────────────────────────────────────────────
#
# Java:  BlockingQueue with take() / put() in separate threads
# Python: asyncio.Queue — put/get are coroutines that yield to the event loop
#         instead of blocking a thread.
# This pattern scales to THOUSANDS of concurrent tasks cheaply.

async def producer(queue: asyncio.Queue, n_items: int, name: str) -> None:
    for i in range(n_items):
        await asyncio.sleep(random.uniform(0.05, 0.15))
        item = f"{name}-item-{i}"
        await queue.put(item)          # blocks if queue is full (backpressure!)
        print(f"  [{name}] produced: {item}")
    # Signal end — put one sentinel per consumer expecting it
    await queue.put(None)

async def consumer(queue: asyncio.Queue, name: str) -> int:
    count = 0
    while True:
        item = await queue.get()       # yields to event loop until item is available
        if item is None:
            queue.task_done()
            break
        print(f"  [{name}] consumed: {item}")
        await asyncio.sleep(0.1)       # simulate processing
        queue.task_done()              # signal item processed (for queue.join())
        count += 1
    return count

async def demo_queue() -> None:
    queue: asyncio.Queue = asyncio.Queue(maxsize=3)   # bounded — applies backpressure

    prod = asyncio.create_task(producer(queue, 5, "prod"))
    cons = asyncio.create_task(consumer(queue, "cons"))

    await asyncio.gather(prod, cons)
    print(f"  All done")

print("=== 1. asyncio.Queue — Producer / Consumer ===")
asyncio.run(demo_queue())
print()


# ─────────────────────────────────────────────────────────────────────────────
# 2. asyncio.Lock, Semaphore, Event
# ─────────────────────────────────────────────────────────────────────────────
#
# Identical semantics to threading counterparts but coroutine-based.
# Use ONLY within an async context (not from regular threads).

async def demo_async_primitives() -> None:
    # Lock — mutual exclusion within async code
    lock = asyncio.Lock()
    shared = {"count": 0}

    async def inc() -> None:
        async with lock:               # async context manager — awaits acquisition
            current = shared["count"]
            await asyncio.sleep(0)     # yield — without the lock this would race!
            shared["count"] = current + 1

    await asyncio.gather(*[inc() for _ in range(100)])
    print(f"  Lock demo — count (expected 100): {shared['count']}")

    # Semaphore — limit concurrent operations (e.g., max 3 DB connections)
    sem = asyncio.Semaphore(3)
    active = [0]

    async def use_connection(i: int) -> None:
        async with sem:
            active[0] += 1
            print(f"    connection {i:2d} active — concurrent: {active[0]}")
            await asyncio.sleep(0.05)
            active[0] -= 1

    await asyncio.gather(*[use_connection(i) for i in range(9)])

    # Event — one-shot signal (like threading.Event but async)
    event = asyncio.Event()

    async def waiter(name: str) -> None:
        print(f"    {name} waiting…")
        await event.wait()
        print(f"    {name} unblocked!")

    waiters = [asyncio.create_task(waiter(f"W{i}")) for i in range(3)]
    await asyncio.sleep(0.1)
    print("  Firing event…")
    event.set()
    await asyncio.gather(*waiters)

print("=== 2. Async Lock, Semaphore, Event ===")
asyncio.run(demo_async_primitives())
print()


# ─────────────────────────────────────────────────────────────────────────────
# 3. TaskGroup  (structured concurrency — Python 3.11+)
# ─────────────────────────────────────────────────────────────────────────────
#
# Java 21+: StructuredTaskScope — a scope that ensures all tasks finish
#           before the scope exits, and propagates exceptions cleanly.
#
# Python 3.11+: asyncio.TaskGroup — the idiomatic replacement for many
#               gather() patterns. All tasks are cancelled if ANY raises.
# This is "structured concurrency" — tasks are scoped to the block.

async def fetch_page(url: str) -> str:
    await asyncio.sleep(random.uniform(0.1, 0.3))
    if "bad" in url:
        raise ConnectionError(f"Cannot reach {url}")
    return f"<page: {url}>"

async def demo_task_group() -> None:
    urls = ["https://a.com", "https://b.com", "https://c.com"]
    results = []

    async with asyncio.TaskGroup() as tg:
        tasks = [tg.create_task(fetch_page(url)) for url in urls]
    # TaskGroup ensures ALL tasks complete before here (or all cancelled on exception)

    results = [t.result() for t in tasks]
    for r in results:
        print(f"  {r}")

print("=== 3. TaskGroup (structured concurrency) ===")
asyncio.run(demo_task_group())
print()


# ─────────────────────────────────────────────────────────────────────────────
# 4. as_completed()  (async)
# ─────────────────────────────────────────────────────────────────────────────
#
# Java:  ExecutorCompletionService — processes futures as they complete
# Python: asyncio.as_completed() — yields futures in completion order

async def demo_as_completed() -> None:
    coros = [fetch_page(f"https://site{i}.com") for i in range(5)]

    for coro in asyncio.as_completed(coros):
        result = await coro
        print(f"  Completed: {result}")

print("=== 4. as_completed() ===")
asyncio.run(demo_as_completed())
print()


# ─────────────────────────────────────────────────────────────────────────────
# 5. ASYNC GENERATORS & ASYNC ITERATORS
# ─────────────────────────────────────────────────────────────────────────────
#
# Java: No direct equivalent. Closest is a reactive stream (Flux in Reactor).
# Python: async def with yield — an async generator.
#         Consumed with `async for`.

async def stream_events(n: int) -> AsyncIterator[str]:
    """Yields events asynchronously — like a reactive stream source."""
    for i in range(n):
        await asyncio.sleep(0.1)       # wait for next event (e.g., from SSE, WebSocket)
        yield f"event-{i}"

async def demo_async_generator() -> None:
    async for event in stream_events(4):
        print(f"  Received: {event}")

print("=== 5. Async Generator / Async Iterator ===")
asyncio.run(demo_async_generator())
print()


# ─────────────────────────────────────────────────────────────────────────────
# 6. ASYNC CONTEXT MANAGERS  (resource management)
# ─────────────────────────────────────────────────────────────────────────────
#
# Java: try-with-resources (AutoCloseable)
# Python: async with —- for resources that require async setup/teardown
#         (e.g., async DB connections, aiohttp sessions)

class AsyncDatabaseConnection:
    """Simulates an async DB connection (think aiopg, asyncpg, etc.)"""

    async def __aenter__(self) -> "AsyncDatabaseConnection":
        print("  [db] connecting…")
        await asyncio.sleep(0.05)      # async connect
        print("  [db] connected")
        return self

    async def __aexit__(self, *_) -> None:
        print("  [db] closing connection")
        await asyncio.sleep(0.02)      # async close

    async def query(self, sql: str) -> list[dict]:
        await asyncio.sleep(0.1)       # async query
        return [{"id": 1, "result": sql}]

async def demo_async_context_manager() -> None:
    async with AsyncDatabaseConnection() as db:
        rows = await db.query("SELECT * FROM users")
        print(f"  Query result: {rows}")
    # connection closed automatically, even on exception

print("=== 6. Async Context Manager ===")
asyncio.run(demo_async_context_manager())
print()


# ─────────────────────────────────────────────────────────────────────────────
# 7. MIXING asyncio WITH THREADS (thread-safe bridge)
# ─────────────────────────────────────────────────────────────────────────────
#
# Sometimes you have regular threads AND an asyncio event loop running together
# (e.g., a Flask app calling into async code, or a background thread posting
#  work to the loop).
#
# Rule: NEVER call coroutines or asyncio primitives from a regular thread —
#       use these bridge functions instead:
#
# asyncio.run_coroutine_threadsafe(coro, loop)  — submit coro from another thread
# loop.call_soon_threadsafe(callback)           — schedule a callback from another thread

import threading

async def async_work(x: int) -> int:
    await asyncio.sleep(0.1)
    return x * x

def run_event_loop(loop: asyncio.AbstractEventLoop) -> None:
    loop.run_forever()

async def demo_thread_bridge() -> None:
    loop = asyncio.new_event_loop()
    t = threading.Thread(target=run_event_loop, args=(loop,), daemon=True)
    t.start()

    # From THIS thread, submit a coroutine to the OTHER thread's event loop
    future = asyncio.run_coroutine_threadsafe(async_work(7), loop)
    result = future.result(timeout=2.0)    # blocks current thread (like Future.get())
    print(f"  Cross-thread result: {result}")

    loop.call_soon_threadsafe(loop.stop)   # ask the loop thread to stop
    t.join(timeout=1.0)

print("=== 7. Thread ↔ asyncio Bridge ===")
asyncio.run(demo_thread_bridge())
print()


# ─────────────────────────────────────────────────────────────────────────────
# 8. CHOOSING THE RIGHT TOOL — Summary
# ─────────────────────────────────────────────────────────────────────────────

print("=== 8. Decision Guide ===")
print("""
  ┌────────────────────────────────────────────────────────────────────────────┐
  │ Scenario                         │ Best Tool                              │
  ├────────────────────────────────────────────────────────────────────────────┤
  │ Many I/O tasks, modern codebase  │ asyncio + async libraries (aiohttp…)   │
  │ I/O tasks, legacy/sync codebase  │ ThreadPoolExecutor or threading        │
  │ CPU-bound work, simple           │ ProcessPoolExecutor                    │
  │ CPU-bound work, shared state     │ multiprocessing + Queue/Manager        │
  │ Single blocking call in async    │ loop.run_in_executor(ThreadPool)       │
  │ CPU call inside async            │ loop.run_in_executor(ProcessPool)      │
  │ Structured parallel subtasks     │ asyncio.TaskGroup (py 3.11+)           │
  │ Fan-out & collect results        │ asyncio.gather()                       │
  │ First result wins                │ asyncio.wait(FIRST_COMPLETED)          │
  │ Stream processing                │ async generators + async for           │
  └────────────────────────────────────────────────────────────────────────────┘
""")
