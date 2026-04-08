"""
PYTHON CONCURRENCY — 01: Threading Basics
==========================================
For Java developers: threading.Thread ≈ java.lang.Thread

KEY DIFFERENCE: Due to the GIL, Python threads are best for I/O-bound
work (file I/O, network, DB). For CPU-bound parallelism use multiprocessing
(see 04_multiprocessing.py).
"""

import threading
import time
import random


# ─────────────────────────────────────────────────────────────────────────────
# 1. CREATING THREADS
# ─────────────────────────────────────────────────────────────────────────────
#
# Java:
#   Thread t = new Thread(() -> System.out.println("hello"));
#   t.start();
#
# Python: pass a callable (function or lambda) as `target`

def worker(name: str, delay: float) -> None:
    print(f"[{name}] starting, will sleep {delay:.1f}s")
    time.sleep(delay)           # I/O-like wait — GIL is RELEASED here
    print(f"[{name}] done")


print("=== 1. Basic Thread Creation ===")

t1 = threading.Thread(target=worker, args=("Alice", 1.0))
t2 = threading.Thread(target=worker, args=("Bob",   0.5))

t1.start()
t2.start()

# Java: t1.join();  t2.join();
t1.join()
t2.join()
print("Both threads finished\n")


# ─────────────────────────────────────────────────────────────────────────────
# 2. SUBCLASSING Thread  (like implements Runnable / extends Thread in Java)
# ─────────────────────────────────────────────────────────────────────────────
#
# Java:
#   class MyTask extends Thread {
#       public void run() { ... }
#   }
#
# Python: override run()

class DownloadTask(threading.Thread):
    def __init__(self, url: str) -> None:
        super().__init__(name=f"Downloader-{url}")
        self.url = url
        self.result: str | None = None         # store output in an attribute

    def run(self) -> None:
        print(f"[{self.name}] Downloading {self.url}")
        time.sleep(random.uniform(0.3, 0.8))  # simulate network I/O
        self.result = f"<html from {self.url}>"
        print(f"[{self.name}] Finished")

print("=== 2. Subclassing Thread ===")
tasks = [DownloadTask(f"https://example.com/page/{i}") for i in range(3)]
for t in tasks:
    t.start()
for t in tasks:
    t.join()
    print(f"  Got: {t.result}")
print()


# ─────────────────────────────────────────────────────────────────────────────
# 3. DAEMON THREADS
# ─────────────────────────────────────────────────────────────────────────────
#
# Java:  thread.setDaemon(true);
# Python: thread.daemon = True   (must be set BEFORE start())
#
# Daemon threads are killed automatically when the main thread exits.
# Useful for background housekeeping tasks (heartbeat, logging flush, etc.)

def heartbeat() -> None:
    while True:
        print("[heartbeat] ♥")
        time.sleep(0.4)

print("=== 3. Daemon Threads ===")
hb = threading.Thread(target=heartbeat, daemon=True)
hb.start()
time.sleep(1.0)           # let it beat a few times
print("Main thread exiting — daemon will die with it\n")


# ─────────────────────────────────────────────────────────────────────────────
# 4. THREAD-LOCAL STORAGE
# ─────────────────────────────────────────────────────────────────────────────
#
# Java:  ThreadLocal<Integer> tl = new ThreadLocal<>();
# Python: threading.local()
#
# Each thread gets its own independent copy of the data stored here.
# Very useful for e.g. per-thread DB connections or request context.

_local = threading.local()

def process_request(request_id: int) -> None:
    _local.request_id = request_id          # stored per-thread
    time.sleep(random.uniform(0.05, 0.15))  # simulate some work
    # Each thread sees its OWN request_id, never another thread's
    print(f"  Thread {threading.current_thread().name}: "
          f"request_id = {_local.request_id}")

print("=== 4. Thread-Local Storage ===")
threads = [
    threading.Thread(target=process_request, args=(i,), name=f"Worker-{i}")
    for i in range(5)
]
for t in threads:
    t.start()
for t in threads:
    t.join()
print()


# ─────────────────────────────────────────────────────────────────────────────
# 5. USEFUL THREAD INTROSPECTION
# ─────────────────────────────────────────────────────────────────────────────

print("=== 5. Thread Introspection ===")
print(f"Current thread : {threading.current_thread().name}")
print(f"Main thread    : {threading.main_thread().name}")
print(f"Active threads : {threading.active_count()}")
print(f"All threads    : {[t.name for t in threading.enumerate()]}")
