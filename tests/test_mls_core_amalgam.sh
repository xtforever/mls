#!/usr/bin/env bash
#
# Build the mls single-header test in four configurations, one per generated
# convenience preset header, and run it with trace_level selected on the
# command line:
#
#   1. release       mls_core.h      (release, single-threaded)
#   2. debug         mls_cored.h     (debug, single-threaded)
#   3. thread        mls_coremt.h    (release, multithread)
#   4. thread-debug  mls_coredmt.h   (debug, multithread)
#
# For each binary, trace_level=0 must produce no "[mls trace" output and
# trace_level=1 must produce at least one. Non-thread builds must not
# reference pthread symbols; thread builds must.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/tests/test_mls_core_amalgam.c"
CC="${CC:-cc}"
NM="${NM:-nm}"
BUILD="$(mktemp -d "${TMPDIR:-/tmp}/mls_amalgam.XXXXXX")"
trap 'rm -rf "$BUILD"' EXIT

PASS=0
FAIL=0
pass() { PASS=$((PASS + 1)); printf '  PASS: %s\n' "$1"; }
fail() { FAIL=$((FAIL + 1)); printf '  FAIL: %s\n' "$1"; }

run_config() {
	local name="$1" header="$2" libs="$3" bin out rc traces has_pthread
	echo "== $name ($header) =="

	# shellcheck disable=SC2086
	if ! "$CC" -std=c11 -I"$ROOT" -DMLS_TEST_HEADER="\"$header\"" \
		"$SRC" -o "$BUILD/$name" $libs; then
		fail "$name (compile)"
		return
	fi
	bin="$BUILD/$name"

	# trace disabled: clean exit, no trace output
	out="$("$bin" 0 2>&1)" && rc=0 || rc=$?
	[ "$rc" -eq 0 ] && pass "$name trace=0 exit=0" \
		|| fail "$name trace=0 exit=$rc"
	# grep in an if-condition so set -e is not tripped by "no match"
	if printf '%s' "$out" | grep -q '\[mls trace'; then
		fail "$name trace=0 emitted trace output"
	else
		pass "$name trace=0 clean"
	fi

	# trace enabled: clean exit, at least one trace line
	out="$("$bin" 1 2>&1)" && rc=0 || rc=$?
	[ "$rc" -eq 0 ] && pass "$name trace=1 exit=0" \
		|| fail "$name trace=1 exit=$rc"
	traces="$(printf '%s' "$out" | grep -c '\[mls trace' || true)"
	if [ "${traces:-0}" -ge 1 ]; then
		pass "$name trace=1 emitted $traces trace lines"
	else
		fail "$name trace=1 emitted no trace output"
	fi

	# threading is really gated: pthread only in the thread builds
	has_pthread=0
	if "$NM" -u "$bin" | grep -qE '(^|[[:space:]])pthread_'; then
		has_pthread=1
	fi
	case "$name" in
	release|debug)
		[ "$has_pthread" -eq 0 ] && pass "$name has no pthread symbols" \
			|| fail "$name unexpectedly references pthread"
		;;
	thread|thread-debug)
		[ "$has_pthread" -eq 1 ] && pass "$name references pthread" \
			|| fail "$name missing pthread symbols"
		;;
	esac
}

run_config release      "../mls_core.h"     "-lm -ldl"
run_config debug        "../mls_cored.h"    "-lm -ldl"
run_config thread       "../mls_coremt.h"   "-lpthread -lm -ldl"
run_config thread-debug "../mls_coredmt.h"  "-lpthread -lm -ldl"

echo
echo "amalgam test: $PASS passed, $FAIL failed"
[ "$FAIL" -eq 0 ]
