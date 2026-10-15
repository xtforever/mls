#!/usr/bin/env bash
#
# Test csv2insert against known-good CSV inputs.
#
#     ./test_csv2insert.sh
#
# Builds csv2insert.exe from the examples makefile (override the binary with
# CSV2INSERT_BIN=/path/to/csv2insert) and compares stdout byte for byte. If
# sqlite3 is installed, the generated statements are executed against an
# in-memory database as an end-to-end check.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TMP="$(mktemp -d "${TMPDIR:-/tmp}/csv2insert.XXXXXX")"
trap 'rm -rf "$TMP"' EXIT

PASS=0
FAIL=0
pass() { PASS=$((PASS + 1)); printf '  ok   %s\n' "$1"; }
fail() { FAIL=$((FAIL + 1)); printf '  FAIL %s\n' "$1"; }

# Expected line for one CSV record; args are already SQL-quoted rows, e.g.
#   "'1','alice','3','x','y'"
want() {
	local row
	for row in "$@"; do
		printf 'insert into datax ( id,name,cnt,ref,verify ) values (%s );\n' \
			"$row"
	done
}

# check NAME EXPECTED   (CSV is read from stdin)
check() {
	local name="$1" exp="$2" got rc
	rc=0
	got="$("$BIN" 2>"$TMP/err")" || rc=$?
	if [ "$rc" -ne 0 ]; then
		fail "$name (exit $rc)"
		return
	fi
	if [ -s "$TMP/err" ]; then
		fail "$name (stderr)"
		sed 's/^/       /' "$TMP/err"
		return
	fi
	if [ "$got" = "$exp" ]; then
		pass "$name"
	else
		fail "$name"
		printf '    want: %q\n    got:  %q\n' "$exp" "$got"
	fi
}

# ---- build ------------------------------------------------------------
if [ -n "${CSV2INSERT_BIN:-}" ]; then
	BIN="$CSV2INSERT_BIN"
else
	BIN="$HERE/csv2insert.exe"
	printf '== build ==\n'
	make -s -C "$HERE" csv2insert.exe
fi
[ -x "$BIN" ] || { printf 'no executable: %s\n' "$BIN" >&2; exit 1; }

# ---- cases ------------------------------------------------------------
printf '== test ==\n'

check "basic two rows" \
	"$(want "'1','alice','3','x','y'" "'2','bob','4','z','w'")" <<'EOF'
1, alice, 3, x, y
2, bob, 4, z, w
EOF

check "sql quote doubling" \
	"$(want "'1','O''Brien','3','x','y'")" <<'EOF'
1, O'Brien, 3, x, y
EOF

check "whitespace is trimmed" \
	"$(want "'1','alice','3','x','y'")" <<'EOF'
  1 ,  alice  , 3 , x , y 
EOF

check "blank line and wrong field count are skipped" \
	"$(want "'1','alice','3','x','y'" "'3','c','5','r','v'")" <<'EOF'
1, alice, 3, x, y

2,3,4
3, c, 5, r, v
EOF

check "crlf line endings" "$(want "'1','alice','3','x','y'")" \
	< <(printf '1, alice, 3, x, y\r\n')

check "no trailing newline" "$(want "'1','a','2','b','c'")" \
	< <(printf '1, a, 2, b, c')

check "empty input" "" < /dev/null

# SQL mode leaves backslashes untouched (SQLite; MySQL NO_BACKSLASH_ESCAPES)
check "backslash stays literal" "$(want "'1','a\\b','3','x','y'")" \
	< <(printf '1, a\\b, 3, x, y\n')

# ---- end-to-end against sqlite3 --------------------------------------
if command -v sqlite3 >/dev/null 2>&1; then
	sql="$(printf "1, O'Brien, 3, x, y\n" | "$BIN")"
	got="$( {
		echo 'create table datax(id,name,cnt,ref,verify);'
		printf '%s\n' "$sql"
		echo 'select id || name || cnt || ref || verify from datax;'
	} | sqlite3 :memory: 2>"$TMP/err" )" || true
	if [ "$got" = "1O'Brien3xy" ]; then
		pass "sqlite3 end-to-end roundtrip"
	else
		fail "sqlite3 end-to-end roundtrip (got: $got)"
	fi
else
	printf '  skip sqlite3 end-to-end (sqlite3 not installed)\n'
fi

# ---- summary ----------------------------------------------------------
printf '\n%d passed, %d failed\n' "$PASS" "$FAIL"
[ "$FAIL" -eq 0 ]
