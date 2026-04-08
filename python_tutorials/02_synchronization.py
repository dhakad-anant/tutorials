"""
PYTHON CONCURRENCY — 02: Synchronization Primitives
=====================================================
For Java developers:

  threading.Lock        ≈  ReentrantLock (non-reentrant!) / synchronized
  threading.RLock       ≈  ReentrantLock (reentrant)
  threading.Condition   ≈  Object.wait() / notify() / notifyAll()
  threading.Semaphore   ≈  java.util.concurrent.Semaphore
  threading.Event       ≈  CountDownLatch(1) / manual-reset flag
  threading.Barrier     ≈  CyclicBarrier
"""

import threading
import time
import random


# ─────────────────────────────────────────────────────────────────────────────
# 1. LOCK  (mutual exclusion)
# ─────────────────────────────────────────────────────────────────────────────
#
# Java:
#   synchronized(this) { counter++; }
#   // or
#   ReentrantLock lock = new ReentrantLock();
#   lock.lock(); try { counter++; } finally { lock.unlock(); }
#
# Python — ALWAYS use the context manager (with lock:) so unlock is guaranteed:
#   lock = threading.Lock()
#   with lock:
#       counter += 1

print("=== 1. Lock — Race Condition Demo ===")

counter_unsafe = 0
counter_safe   = 0
lock = threading.Lock()

def increment_unsafe() -> None:
    global counter_unsafe
    for _ in range(100_000):
        counter_unsafe += 1       # NOT atomic — read-modify-write can race!

def increment_safe() -> None:
    global counter_safe
    for _ in range(100_000):
        with lock:                # acquire on entry, release on exit (even on exception)
            counter_safe += 1

threads = [threading.Thread(target=increment_unsafe) for _ in range(4)]
for t in threads: t.start()
for t in threads: t.join()
print(f"  Unsafe counter (expected 400000): {counter_unsafe}")  # often wrong!

threads = [threading.Thread(target=increment_safe) for _ in range(4)]
for t in threads: t.start()
for t in threads: t.join()
print(f"  Safe counter   (expected 400000): {counter_safe}\n")  # always correct


# ─────────────────────────────────────────────────────────────────────────────
# 2. RLOCK  (reentrant lock)
# ─────────────────────────────────────────────────────────────────────────────
#
# Java:  ReentrantLock (default behavior)
# Python: threading.RLock — the SAME thread can acquire it multiple times
#         without deadlocking. A plain Lock would deadlock on re-entry.

print("=== 2. RLock — Reentrant Locking ===")

rlock = threading.RLock()

def outer() -> None:
    with rlock:
        print("  outer: acquired rlock")
        inner()          # calls inner which also needs the lock — works with RLock

def inner() -> None:
    with rlock:          # same thread re-acquires — safe with RLock
        print("  inner: re-acquired rlock")

t = threading.Thread(target=outer)
t.start()
t.join()
print()


# ─────────────────────────────────────────────────────────────────────────────
# 3. CONDITION  (wait / notify)
# ─────────────────────────────────────────────────────────────────────────────
#
# Java:
#   synchronized(monitor) { while (!ready) monitor.wait(); doWork(); }
#   synchronized(monitor) { ready = true; monitor.notifyAll(); }
#
# Python pattern is identical in structure:
#   with condition:
#       while not predicate(): condition.wait()
#       do_work()
#
# condition.wait()      releases the lock and blocks until notified
# condition.notify()    wake ONE waiter (= Object.notify())
# condition.notify_all()  wake ALL waiters (= Object.notifyAll())

print("=== 3. Condition — Producer / Consumer ===")

buffer: list[int] = []
MAX_SIZE = 3
condition = threading.Condition()

def producer(n: int) -> None:
    for i in range(n):
        with condition:
            while len(buffer) >= MAX_SIZE:
                print("  [producer] buffer full, waiting…")
                condition.wait()                  # releases lock, blocks
            item = random.randint(1, 99)
            buffer.append(item)
            print(f"  [producer] produced {item:2d}, buffer={buffer}")
            condition.notify_all()                # wake sleeping consumers
        time.sleep(0.05)

def consumer(name: str, n: int) -> None:
    for _ in range(n):
        with condition:
            while not buffer:
                condition.wait()                  # nothing to consume yet
            item = buffer.pop(0)
            print(f"  [{name}] consumed {item:2d}, buffer={buffer}")
            condition.notify_all()                # wake sleeping producer
        time.sleep(0.1)

prod = threading.Thread(target=producer, args=(8,))
cons = [threading.Thread(target=consumer, args=(f"consumer-{i}", 4)) for i in range(2)]
prod.start()
for c in cons: c.start()
prod.join()
for c in cons: c.join()
print()


# ─────────────────────────────────────────────────────────────────────────────
# 4. SEMAPHORE  (limit concurrency)
# ─────────────────────────────────────────────────────────────────────────────
#
# Java:  Semaphore sem = new Semaphore(3);
#        sem.acquire(); try { ... } finally { sem.release(); }
#
# Python: threading.Semaphore(n)  — same acquire/release semantics
# BoundedSemaphore raises ValueError if you release more than n times,
# catching bugs where you release without acquiring (no equivalent check in Java).

print("=== 4. Semaphore — Rate-Limit Concurrent Connections ===")

MAX_CONNECTIONS = 3
semaphore = threading.BoundedSemaphore(MAX_CONNECTIONS)

def connect(worker_id: int) -> None:
    print(f"  [{worker_id}] waiting for connection slot…")
    with semaphore:                          # blocks if all 3 slots are taken
        print(f"  [{worker_id}] connected")
        time.sleep(random.uniform(0.2, 0.5))
        print(f"  [{worker_id}] disconnected")

threads = [threading.Thread(target=connect, args=(i,)) for i in range(7)]
for t in threads: t.start()
for t in threads: t.join()
print()


# ─────────────────────────────────────────────────────────────────────────────
# 5. EVENT  (one-shot signal / flag)
# ─────────────────────────────────────────────────────────────────────────────
#
# Java: CountDownLatch(1) for a one-shot signal, or a volatile boolean flag
#
# threading.Event is a simple boolean flag:
#   event.set()        — set flag to True, wake all waiters
#   event.clear()      — reset flag to False
#   event.wait()       — block until flag is True (or timeout)
#   event.is_set()     — non-blocking check

print("=== 5. Event — Start Signal ===")

ready_event = threading.Event()

def worker_waiting(name: str) -> None:
    print(f"  [{name}] waiting for start signal…")
    ready_event.wait()              # blocks until event is set
    print(f"  [{name}] GO! Starting work.")

workers = [threading.Thread(target=worker_waiting, args=(f"W{i}",)) for i in range(4)]
for w in workers: w.start()

time.sleep(0.3)
print("  [main] firing start signal!")
ready_event.set()                   # unblocks ALL waiting threads at once

for w in workers: w.join()
print()


# ─────────────────────────────────────────────────────────────────────────────
# 6. BARRIER  (rendezvous / synchronization point)
# ─────────────────────────────────────────────────────────────────────────────
#
# Java: CyclicBarrier
# Python: threading.Barrier(n_parties)
#   barrier.wait() — each caller blocks until n_parties threads have called wait()
#   Then ALL are released simultaneously.

print("=== 6. Barrier — Phased Computation ===")

N_WORKERS = 4
barrier = threading.Barrier(N_WORKERS)

def phase_worker(worker_id: int) -> None:
    for phase in range(1, 3):
        duration = random.uniform(0.1, 0.5)
        time.sleep(duration)
        print(f"  [W{worker_id}] phase {phase} done ({duration:.2f}s), waiting at barrier…")
        barrier.wait()              # wait for ALL workers to finish this phase
        if worker_id == 0:
            print(f"  >>> All workers passed barrier for phase {phase} <<<")

threads = [threading.Thread(target=phase_worker, args=(i,)) for i in range(N_WORKERS)]
for t in threads: t.start()
for t in threads: t.join()
print()


# ─────────────────────────────────────────────────────────────────────────────
# 7. DEADLOCK AVOIDANCE  — same rules as Java
# ─────────────────────────────────────────────────────────────────────────────
#
# Golden rules (identical in both Java and Python):
#  1. Always acquire locks in the SAME order across all threads.
#  2. Use try/finally (or 'with') to guarantee unlock.
#  3. Prefer higher-level abstractions (Queue, Executor) over raw locks.
#  4. Lock.acquire(timeout=...) lets you detect and break out of deadlocks.

print("=== 7. Lock with Timeout (deadlock detection) ===")

lock_a = threading.Lock()
lock_b = threading.Lock()

def safe_transfer() -> None:
    # acquire with a timeout instead of blocking forever
    acquired = lock_a.acquire(timeout=1.0)
    if not acquired:
        print("  Could not acquire lock_a in time — aborting to avoid deadlock")
        return
    try:
        time.sleep(0.05)
        acquired_b = lock_b.acquire(timeout=1.0)
        if not acquired_b:
            print("  Could not acquire lock_b in time — aborting")
            return
        try:
            print("  Transfer complete (both locks held)")
        finally:
            lock_b.release()
    finally:
        lock_a.release()

t = threading.Thread(target=safe_transfer)
t.start()
t.join()
