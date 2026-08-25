/*
 * MLS: int handles instead of C pointers.
 *
 * Use-after-free becomes a bounds check.
 * Memory handling prevents leaks.
 * All data is one int handle.
 *
 * Good error messages with backtraces.
 * You can inspect lists at runtime.
 *
 * Small footprint.
 * A thin layer above C. C stays reachable.
 */
