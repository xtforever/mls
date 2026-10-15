# Why Rust

Rust gives you C-level performance with memory safety enforced at compile time.

## Performance
- No garbage collector and no mandatory runtime: predictable, low-latency behavior.
- Memory-efficient and fast, suitable for performance-critical services and embedded devices.
- Zero-cost abstractions let high-level code compile down to the same machine code you would write by hand.

## Reliability
- The ownership and borrowing model guarantees memory safety without a GC.
- Data-race freedom is checked at compile time through `Send`/`Sync`; this does not prevent deadlocks or logical races, but it removes whole bug classes.
- No null pointer dereferences, and errors are values (`Result`) rather than exceptions.

## Productivity & tooling
- `cargo` bundles build, dependency management, tests, and documentation.
- `rustfmt`, `clippy`, and editor tooling (rust-analyzer) are part of the standard setup.
- The compiler's error messages are unusually helpful for a systems language.

## Ecosystem & interop
- crates.io provides a large, actively maintained package ecosystem.
- C FFI is first-class; Rust itself has no stable ABI, so cross-language boundaries go through C.

## Trade-offs / Limitations
- The borrow checker has a steep learning curve; some valid designs need restructuring.
- Compile times are slower than Go's or Zig's, especially for large projects heavy on generics.

**Sources:** rust-lang.org "Why Rust?"; crates.io
