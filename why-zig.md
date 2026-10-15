# Why Zig

Zig is a small, explicit systems language that aims to be a better C: no hidden behavior, no mandatory runtime.

## No hidden control flow
- No operator overloading, no `@property` getters, no exceptions: what looks like a function call is the only thing that runs.
- No macros; the language itself is expressive enough to avoid them.

## No hidden allocations
- Every function that allocates takes an explicit `Allocator` parameter.
- No `new` keyword and no string-concatenation operator that silently hits the heap.
- Custom allocators make arena, debug (leak- and use-after-free-detecting), and specialized allocation strategies easy.

## Freestanding & bare-metal
- The standard library is optional; only what you use is compiled in.
- Equal support for linking libc or not, making freestanding and bare-metal targets first-class.
- Tiny binaries, including for WebAssembly.

## Toolchain & portability
- Ships a build system and package manager that can replace CMake/Make even for existing C/C++ projects.
- Out-of-the-box cross-compilation and `zig cc` as a drop-in GCC/Clang replacement.
- Straightforward C ABI interoperability makes Zig a portable language for libraries.

## Trade-offs / Limitations
- The ecosystem and third-party libraries are still young compared to Rust or Go.
- The language is not yet 1.0; syntax and tooling can still change between releases.

**Sources:** ziglang.org/learn/why_zig_rust_d_cpp/
