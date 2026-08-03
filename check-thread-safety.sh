#!/usr/bin/env bash
set -euo pipefail
sudo sysctl vm.mmap_rnd_bits=28

ROOT="$(cd "$(dirname "$0")" && pwd)"
PASS=0
FAIL=0
RESULTS=""

pass() { ((PASS++)) || true; RESULTS="$RESULTS  PASS: $1\n"; }
fail() { ((FAIL++)) || true; RESULTS="$RESULTS  FAIL: $1\n"; }

section() {
	echo ""
	echo "========================================================================"
	echo "  $1"
	echo "========================================================================"
}

run_test() {
	local name="$1"
	shift
	section "$name"
	if "$@"; then
		pass "$name"
	else
		fail "$name"
	fi
}

cd "$ROOT"

# === 1. ASAN leak check ===
# Compiles all tests with AddressSanitizer + leak detection
run_test "ASAN leak/race detection" \
	make -C tests thread_safe=1 clean ALL check-leaks

# === 2. ThreadSanitizer ===
# Rebuild and run with -fsanitize=thread to catch data races
run_test "ThreadSanitizer (data race detection)" \
	make -C tests thread_safe=1 \
		CFLAGS="-g -fsanitize=thread -DMLS_THREAD_SAFE -D_GNU_SOURCE -I../lib" \
		LDLIBS="-lpthread -fsanitize=thread -ldl -lm" \
		clean ALL run

# Rebuild with ASAN so steps 3-4 don't run leftover TSan binaries
make -C tests thread_safe=1 clean ALL > /dev/null 2>&1

# === 3. Stress fuzzy test ===
# Run the randomised fuzzy test 100 times to surface probabilistic races
section "Fuzzy stress (100 rounds x 50000 iters)"
cd tests
FUZZ_PASS=0
FUZZ_FAIL=0
for i in $(seq 1 100); do
	if FUZZ_ITERS=50000 ./test_thread_safe_fuzzy.exed > /dev/null 2>&1; then
		((FUZZ_PASS++)) || true
	else
		((FUZZ_FAIL++)) || true
		echo "  Fuzzy test failed on round $i"
		FUZZ_ITERS=50000 ./test_thread_safe_fuzzy.exed 2>&1 | tail -5 || true
		break
	fi
done
cd "$ROOT"
if [ "$FUZZ_FAIL" -eq 0 ]; then
	pass "Fuzzy stress ($FUZZ_PASS / 100 rounds passed)"
else
	fail "Fuzzy stress (failed at round $((FUZZ_PASS + 1)))"
fi

# === 4. Thread safety invariant tests ===
# Our dedicated thread-safety test suite (UAF, double-free-safe, concurrent r/w, etc.)
run_test "Thread safety invariants" \
	./tests/test_thread_safety.exed

# === 5. Static analysis ===
# cppcheck + clang-tidy on the lib/ sources
run_test "Static analysis (cppcheck + clang-tidy)" \
	make -f makefile.qa check-static

# === Summary ===
echo ""
echo "========================================================================"
echo "  RESULTS"
echo "========================================================================"
echo -e "$RESULTS"
echo ""
echo "  Total: $((PASS + FAIL)) tests — $PASS passed, $FAIL failed"
[ "$FAIL" -eq 0 ]
