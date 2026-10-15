#!/usr/bin/env bash
# Demo for the csv2insert example: turn CSV on stdin into SQL INSERT statements.
#
#   ./start.sh
#
# Builds via the examples makefile (csv2insert links m_var) and runs a few
# sample records. For the full test matrix use ./test_csv2insert.sh.
set -euo pipefail
cd "$(dirname "$0")"

echo "== build =="
make -s csv2insert.exe

CSV="1, alice, 3, x, y
2, Bob, 5, y, z
3, O'Brien, 7, z, w"

echo
echo "== input =="
printf '%s\n' "$CSV"
echo
echo "== output =="
printf '%s\n' "$CSV" | ./csv2insert.exe
