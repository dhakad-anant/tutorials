# Module 03: Templates & Metaprogramming

Templates are the backbone of generic, high-performance C++ libraries. This module
takes you from "I know `template<typename T>`" to writing real template infrastructure.

## Topics
1. [Variadic Templates & Fold Expressions](01_variadic_templates.cpp)
2. [SFINAE, Type Traits & `if constexpr`](02_sfinae_type_traits.cpp)
3. [CRTP & Policy-Based Design](03_crtp_policies.cpp)

## Key Insight
Templates are a **compile-time code generation** system. The compiler writes
specialized code for every type you use. This means zero runtime overhead —
but you pay with compile time and error message complexity.

---
*After completing all files, move to [Module 04: Concurrency](../04_concurrency/README.md) →*
