# Module 02: Memory Management & Ownership

The single most important topic for systems programming. At Pure Storage, you'll work
with large buffers, memory pools, and performance-critical allocation patterns.

## Topics
1. [RAII — Resource Acquisition Is Initialization](01_raii.cpp)
2. [Memory Layout: Stack, Heap, Static, and Cache Lines](02_memory_layout.cpp)
3. [Custom Allocators & Memory Pools](03_custom_allocators.cpp)

## Key Mental Model
In C++, **every resource has an owner**. RAII ensures resources are released when
the owner goes out of scope. If you're ever confused about a bug, ask:
**"Who owns this memory?"**

---
*After completing all files, move to [Module 03: Templates](../03_templates/README.md) →*
