"""
PYTHON CONCURRENCY — 04: Multiprocessing
=========================================
For Java developers:

The GIL means Python threads can't run Python bytecode in parallel.
Multiprocessing spawns SEPARATE OS processes — each has its own interpreter
and GIL, giving you true CPU parallelism.

Think of it like running multiple JVMs instead of multiple threads.

  multiprocessing.Process   ≈  ProcessBuilder launching a separate JVM
  multiprocessing.Pool      ≈  ForkJoinPool
  multiprocessing.Queue     ≈  LinkedBlockingQueue (cross-process safe)
  multiprocessing.Pipe      ≈  PipedInputStream / PipedOutputStream
  multiprocessing.Value     ≈  AtomicInteger (cross-process)
  multiprocessing.Array     ≈  shared int[] (cross-process)
  multiprocessing.Manager   ≈  RMI / distributed shared objects

NOTE: On Windows/macOS, new processes are SPAWNED (re-import __main__),
so all worker functions must be defined at module top-level, and the
main entry point must be guarded with:  if __name__ == '__main__':
"""

import multiprocessing
import multiprocessing.shared_memory
import time
import os
import random


# ─────────────────────────────────────────────────────────────────────────────
# Module-level worker functions (required for pickling on Windows/macOS)
# ─────────────────────────────────────────────────────────────────────────────

def cpu_worker(n: int) -> int:
    """Burns CPU — benefits from running in a separate process."""
    total = sum(i * i for i in range(n * 100_000))
    print(f"  [PID {os.getpid()}] cpu_worker({n}) done")
    return total % 1_000_000   # just return something manageable

def producer_proc(queue: multiprocessing.Queue, n: int) -> None:
    for i in range(n):
        queue.put(i)
        time.sleep(0.05)
    queue.put(None)                    # sentinel — signals end of stream

def consumer_proc(queue: multiprocessing.Queue, name: str) -> None:
    while True:
        item = queue.get()
        if item is None:
            queue.put(None)            # re-post sentinel for other consumers
            break
        print(f"  [{name}] consumed {item}")
        time.sleep(0.1)

def pipe_worker(conn: multiprocessing.connection.Connection) -> None:
    for msg in iter(conn.recv, "STOP"):
        conn.send(f"echo: {msg}")
    conn.close()

def shared_value_worker(val: multiprocessing.Value,
                        lock: multiprocessing.Lock,
                        amount: int) -> None:
    for _ in range(amount):
        with lock:                     # cross-process lock — like synchronized
            val.value += 1


# ─────────────────────────────────────────────────────────────────────────────
# MAIN — all multiprocessing code MUST be inside this guard
# ─────────────────────────────────────────────────────────────────────────────

if __name__ == "__main__":

    # ————————————————————————————————————————————————————————————————————————
    # 1. Process  (like spawning a new JVM process)
    # ————————————————————————————————————————————————————————————————————————
    print("=== 1. multiprocessing.Process ===")

    p = multiprocessing.Process(
        target=cpu_worker,
        args=(5,),
        name="MyProcess"
    )
    p.start()
    print(f"  Spawned PID={p.pid}, parent PID={os.getpid()}")
    p.join()              # wait for it (like Thread.join / Process.waitFor)
    print(f"  Process exited with code {p.exitcode}\n")


    # ————————————————————————————————————————————————————————————————————————
    # 2. Pool  (ForkJoinPool — map work across processes)
    # ————————————————————————————————————————————————————————————————————————
    #
    # Java: ForkJoinPool.commonPool().submit(() -> { ... })
    #       or stream().parallel().map(...)
    #
    # Pool.map()        — like parallel stream map, blocks until all done
    # Pool.imap()       — lazy iterator version (memory-efficient for large input)
    # Pool.map_async()  — non-blocking, returns AsyncResult (like Future)
    # Pool.starmap()    — like map() but args are tuples (unpacked as *args)

    print("=== 2. Pool.map() — Parallel CPU Work ===")
    start = time.perf_counter()

    with multiprocessing.Pool(processes=4) as pool:
        results = pool.map(cpu_worker, range(1, 5))

    elapsed = time.perf_counter() - start
    print(f"  Results : {results}")
    print(f"  Elapsed : {elapsed:.2f}s\n")


    # ————————————————————————————————————————————————————————————————————————
    # 3. Queue  (cross-PROCESS producer / consumer)
    # ————————————————————————————————————————————————————————————————————————
    #
    # Java: LinkedBlockingQueue shared between threads.
    # Python: multiprocessing.Queue — passed explicitly to child processes;
    #         uses a pipe + background thread to serialize data across processes.
    #
    # Within a single process, use queue.Queue (from the queue module) — it's
    # faster because it doesn't need serialization.

    print("=== 3. multiprocessing.Queue — Cross-Process Producer/Consumer ===")
    q: multiprocessing.Queue = multiprocessing.Queue()

    prod = multiprocessing.Process(target=producer_proc, args=(q, 5))
    cons = multiprocessing.Process(target=consumer_proc, args=(q, "consumer"))

    prod.start()
    cons.start()
    prod.join()
    cons.join()
    print()


    # ————————————————————————————————————————————————————————————————————————
    # 4. Pipe  (bidirectional channel between two processes)
    # ————————————————————————————————————————————————————————————————————————
    #
    # Java:  PipedInputStream / PipedOutputStream
    # Python: multiprocessing.Pipe() returns (conn1, conn2).
    #         duplex=True (default) → both ends can send and receive.
    #         duplex=False → conn1=read-only, conn2=write-only.

    print("=== 4. Pipe — Bidirectional Channel ===")
    parent_conn, child_conn = multiprocessing.Pipe(duplex=True)

    worker_proc = multiprocessing.Process(target=pipe_worker, args=(child_conn,))
    worker_proc.start()

    for message in ["hello", "world", "STOP"]:
        parent_conn.send(message)
        if message != "STOP":
            reply = parent_conn.recv()
            print(f"  parent sent '{message}', got '{reply}'")

    worker_proc.join()
    parent_conn.close()
    print()


    # ————————————————————————————————————————————————————————————————————————
    # 5. Shared Memory — Value and Array
    # ————————————————————————————————————————————————————————————————————————
    #
    # Java:  AtomicInteger / int[] shared between threads — trivial.
    # Python: Shared state between PROCESSES requires explicit setup.
    #
    # multiprocessing.Value(typecode, value) — single typed value
    # multiprocessing.Array(typecode, size)  — typed array
    # typecodes: 'i'=int, 'd'=double, 'b'=byte, etc. (same as ctypes)
    #
    # CRITICAL: Always use a Lock — .value access is NOT atomic!

    print("=== 5. Shared Value + Lock ===")
    shared_counter = multiprocessing.Value('i', 0)   # 'i' = signed int
    proc_lock = multiprocessing.Lock()

    procs = [
        multiprocessing.Process(
            target=shared_value_worker,
            args=(shared_counter, proc_lock, 1000)
        )
        for _ in range(4)
    ]
    for p in procs: p.start()
    for p in procs: p.join()

    print(f"  Shared counter (expected 4000): {shared_counter.value}\n")


    # ————————————————————————————————————————————————————————————————————————
    # 6. Manager  (proxy objects for complex shared state)
    # ————————————————————————————————————————————————————————————————————————
    #
    # Value/Array work for simple scalars. For dict/list/custom objects across
    # processes, use a Manager — it runs a server process and gives each
    # worker a proxy that communicates via RPC under the hood.
    #
    # This is slower than Value/Array but more flexible.

    print("=== 6. Manager — Shared dict / list ===")

    def worker_with_dict(d: dict, key: str) -> None:
        d[key] = os.getpid()           # mutate shared dict from child process

    with multiprocessing.Manager() as manager:
        shared_dict = manager.dict()   # proxy dict — changes visible to all processes

        procs = [
            multiprocessing.Process(target=worker_with_dict, args=(shared_dict, f"key{i}"))
            for i in range(4)
        ]
        for p in procs: p.start()
        for p in procs: p.join()

        print(f"  Shared dict: {dict(shared_dict)}\n")


    # ————————————————————————————————————————————————————————————————————————
    # 7. Summary — When to use what
    # ————————————————————————————————————————————————————────────────────────
    print("=== 7. Shared State Cheat Sheet ===")
    print("""
  ┌─────────────────────────────────────────────────────────────────────────┐
  │ Need                          │ Use                                     │
  ├─────────────────────────────────────────────────────────────────────────┤
  │ No shared state (pure output) │ Pool.map() — simplest, fastest          │
  │ Pass results back             │ Pool.map() return value OR Queue        │
  │ Single int/float counter      │ Value('i') + Lock                       │
  │ Shared array of numbers       │ Array('d', size)                        │
  │ Shared dict/list              │ Manager().dict() / Manager().list()     │
  │ Stream of work items          │ Queue (producer/consumer)               │
  │ Two-process protocol          │ Pipe                                    │
  └─────────────────────────────────────────────────────────────────────────┘
    """)
