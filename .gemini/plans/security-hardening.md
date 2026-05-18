   1 # Plan: Security Hardening of the MLS Library
    2
    3 Transforming `mls` into the most secure C library for array handling by implementing
      architectural safeguards against common vulnerabilities.
    4
    5 ## Objective
    6 - Eliminate integer overflow vulnerabilities in memory allocation and size calculations.
    7 - Harden handle validation and Use-After-Free (UAF) protection.
    8 - Ensure rigorous bounds checking across all API entry points.
    9 - Prevent information leaks via memory sanitization.
   10
   11 ## Proposed Solution
   12 1. **Type Migration (int -> size_t)**: Prevents overflows on 64-bit systems.
   13 2. **Safe Arithmetic & Allocation Wrappers**: `m_safe_mul` and `m_calloc_safe`.
   14 3. **Hardened Bounds Checking**: Strict index validation in `mls()` and others.
   15 4. **UAF and Handle Hardening**: Randomized patterns and incremental protection.
   16 5. **Memory Sanitization**: Zeroing out memory in `m_free()`.
   17
   18 ## Implementation Plan
   19
   20 ### Phase 1: Core Type Update
   21 1. Modify `lib/mls.h` to use `size_t` for `w`, `l`, and `max`.
   22 2. Update function signatures in `lib/mls.c`.
   23
   24 ### Phase 2: Safe Helpers
   25 1. Implement `m_safe_mul` in `lib/mls.c`.
   26 2. Refactor `m_alloc` and `lst_resize`.
   27
   28 ### Phase 3: Validation
   29 1. Harden `mls()` with signed index checks.
   30 2. Update `m_write`/`m_read` bounds checks.
   31
   32 ### Phase 4: Handle Security
   33 1. Add `srand`/`rand` to `m_init()` for UAF protection initialization.
   34 2. Implement zero-fill on free.


