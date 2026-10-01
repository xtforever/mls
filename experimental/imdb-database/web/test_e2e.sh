#!/usr/bin/env bash
# End-to-End-Simulation der Kette Suche -> Agent -> sshfs(Fake) -> Player(Fake).
# Kein Server, kein Display, kein root. Nur Bash/Python3/curl, temporaere Dateien.
#
#   bash web/test_e2e.sh        # Exit 0 und "PASS: ..." bei Erfolg
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
T="$(mktemp -d /tmp/movdb-e2e.XXXXXX)"
SRV=""; AGT=""
cleanup() {
  [ -n "$SRV" ] && kill "$SRV" 2>/dev/null || true
  [ -n "$AGT" ] && kill "$AGT" 2>/dev/null || true
  rm -rf "$T"
}
trap cleanup EXIT
fail() { echo "FAIL: $*" >&2; exit 1; }

export HOME="$T/home"
export PATH="$T/bin:$PATH"
export FAKE_SERVER_ROOT="$T/serverroot"
export MPV_LOG="$T/mpv.log"
mkdir -p "$HOME" "$T/bin" "$T/serverroot/jensbk"
: > "$MPV_LOG"

printf 'x\n' > "$T/serverroot/jensbk/movie.mkv"
printf 'x\n' > "$T/serverroot/jensbk/notes.txt"

# ---- Fakes: sshfs/mountpoint/mpv/systemctl/fake_movdb ----
cat > "$T/bin/sshfs" <<'EOF'
#!/usr/bin/env bash
mkdir -p "$2"; cp -r "$FAKE_SERVER_ROOT"/. "$2"/; echo x > "$2/.movdb_mounted"
EOF
cat > "$T/bin/mountpoint" <<'EOF'
#!/usr/bin/env bash
[ -e "$1/.movdb_mounted" ]
EOF
cat > "$T/bin/fusermount3" <<'EOF'
#!/usr/bin/env bash
[ "${1:-}" = "-u" ] && rm -rf "$2"
EOF
cat > "$T/bin/mpv" <<'EOF'
#!/usr/bin/env bash
printf '%s\n' "$@" >> "$MPV_LOG"
EOF
cat > "$T/bin/systemctl" <<'EOF'
#!/usr/bin/env bash
exit 0
EOF
cat > "$T/bin/fake_movdb" <<'EOF'
#!/usr/bin/env bash
printf 'Movie\t2020\ttt1234567\t8.0\t100\t90\tDrama\t/jensbk/movie.mkv\n'
EOF
chmod +x "$T/bin/"*

freeport() {
  python3 -c 'import socket;s=socket.socket();s.bind(("127.0.0.1",0));print(s.getsockname()[1]);s.close()'
}
PUI="$(freeport)"; PAGT="$(freeport)"
[ "$PUI" != "$PAGT" ] || PAGT="$(freeport)"

# ---- Client installieren (isoliertes HOME) ----
"$HERE/install-client.sh" --server user@server \
  --ui-origin "http://127.0.0.1:$PUI" --port "$PAGT" --no-service > "$T/install.log"

# ---- Such-Frontend (Container) + Play-Agent starten ----
MOVDB_EXE="$T/bin/fake_movdb" MOVDB_BIN="$T/movies.bin" MOVDB_META="$T/meta.bin" \
MOVDB_DATA="$T" MOVDB_MAINTENANCE=0 PORT="$PUI" BIND=127.0.0.1 \
  python3 "$HERE/server.py" > "$T/server.log" 2>&1 & SRV=$!
"$HOME/.local/bin/movdb-play-agent" > "$T/agent.log" 2>&1 & AGT=$!

for _ in $(seq 1 50); do
  curl -fsS "http://127.0.0.1:$PUI/healthz" >/dev/null 2>&1 && \
  curl -fsS "http://127.0.0.1:$PAGT/healthz" >/dev/null 2>&1 && break
  sleep 0.1
done
curl -fsS "http://127.0.0.1:$PUI/healthz"  >/dev/null || fail "UI nicht erreichbar"
curl -fsS "http://127.0.0.1:$PAGT/healthz" >/dev/null || fail "Agent nicht erreichbar"

TOKEN="$(grep '^MOVDB_PLAY_TOKEN=' "$HOME/.config/movdb/player.env" | cut -d= -f2- | tr -d '"')"

# ---- 1) Browser-Suche ----
BODY="$(curl -fsS "http://127.0.0.1:$PUI/api/search?q=movie")"
DBPATH="$(printf '%s' "$BODY" | python3 -c 'import sys,json;print(json.load(sys.stdin)["rows"][0]["path"])')"
[ "$DBPATH" = "/jensbk/movie.mkv" ] || fail "unerwarteter Suchpfad: $DBPATH"

play() {
  curl -sS -o "$T/r.json" -w '%{http_code}' -X POST "http://127.0.0.1:$PAGT/play" \
    -H "Origin: http://127.0.0.1:$PUI" -H "Content-Type: application/json" \
    -H "X-Movdb-Play: $TOKEN" --data "{\"path\": \"$1\"}"
}

# ---- 2) Play vor Mount: Agent mountet selbst und spielt ----
: > "$MPV_LOG"
CODE="$(play "$DBPATH")"
[ "$CODE" = "200" ] || fail "Play vor Mount: HTTP $CODE ($(cat "$T/r.json"))"
grep -q "$HOME/mnt/server/jensbk/movie.mkv" "$MPV_LOG" \
  || fail "mpv bekam nicht den gemappten Pfad: $(cat "$MPV_LOG")"

# ---- 3) Negativ: falsche Endung wird abgelehnt ----
: > "$MPV_LOG"
CODE="$(play /jensbk/notes.txt)"
[ "$CODE" = "403" ] || fail "falsche Endung nicht abgelehnt: HTTP $CODE"
[ ! -s "$MPV_LOG" ] || fail "Player trotz Ablehnung gestartet"

echo "PASS: Suche -> Agent -> On-Demand-Mount -> Player (simuliert)"
