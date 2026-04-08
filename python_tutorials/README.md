# Python Concurrency Tutorial (for Java Developers)

## The Big Difference: The GIL

Before anything else, you need to understand the **Global Interpreter Lock (GIL)** — it's the most
important concept that distinguishes Python concurrency from Java's.

```
Java:  True parallelism for CPU-bound work via threads (JVM runs threads on multiple cores)
Python: Threads share ONE interpreter lock — only ONE thread executes Python bytecode at a time
```

This has a profound consequence:

| Scenario          | Java Threads          | Python Threads (`threading`) | Python Processes (`multiprocessing`) |
|-------------------|-----------------------|------------------------------|--------------------------------------|
| CPU-bound work    | Scales with cores     | ❌ No speedup (GIL!)          | ✅ Scales with cores                  |
| I/O-bound work    | Scales well           | ✅ Scales well (GIL released) | ✅ Scales well                        |
| Memory overhead   | Medium (JVM stack)    | Low                          | High (full process copy)             |
| Shared state      | `synchronized`, locks | `threading.Lock`, etc.       | `Queue`, `Manager`, `Value`          |

**Rule of thumb:**
- I/O-bound (network, files, DB)  → `threading` or `asyncio`
- CPU-bound (computation, parsing) → `multiprocessing` or `concurrent.futures.ProcessPoolExecutor`

---

## Tutorial Structure

| File | Topic | Java Equivalent |
|------|-------|-----------------|
| [01_threading_basics.py](01_threading_basics.py) | Creating & managing threads | `Thread`, `Runnable` |
| [02_synchronization.py](02_synchronization.py) | Locks, conditions, semaphores | `synchronized`, `ReentrantLock`, `Semaphore` |
| [03_executor_pools.py](03_executor_pools.py) | Thread & process pools, Futures | `ExecutorService`, `Future`, `CompletableFuture` |
| [04_multiprocessing.py](04_multiprocessing.py) | True parallelism, shared memory | No direct equiv — needed due to GIL |
| [05_asyncio_basics.py](05_asyncio_basics.py) | Cooperative async/await | `CompletableFuture`, virtual threads (Java 21+) |
| [06_asyncio_advanced.py](06_asyncio_advanced.py) | Queues, tasks, timeouts, gather | `ExecutorService`, `CompletableFuture.allOf()` |

---

## Quick Concept Mapping

```
Java                                Python
─────────────────────────────────── ────────────────────────────────────
new Thread(runnable).start()        threading.Thread(target=fn).start()
implements Runnable                 just pass a function (callable)
synchronized (obj) { }             with lock:  (context manager)
ReentrantLock                       threading.Lock / threading.RLock
volatile                            not needed (Python mem model differs)
ExecutorService (thread pool)       concurrent.futures.ThreadPoolExecutor
Future<T>                           concurrent.futures.Future
CompletableFuture                   asyncio.Future / asyncio.Task
async/await (Java 21 virtual thr.)  asyncio coroutines
BlockingQueue                       queue.Queue
CountDownLatch                      threading.Barrier / threading.Event
AtomicInteger                       threading.Lock + int  OR  Value('i')
ForkJoinPool                        multiprocessing.Pool / ProcessPoolExecutor
```

---

## How to Run Examples

```bash
python 01_threading_basics.py
python 02_synchronization.py
# ... etc
```

Requires Python 3.8+. No external dependencies needed.
