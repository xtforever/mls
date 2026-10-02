#!/usr/bin/env bash
# Prueft die automatische UI-Origin-Ermittlung in install-client.sh.
# Isoliertes HOME, Fake-ssh, kein Netz, kein root.
#
#   bash web/test_install_origin.sh    # Exit 0 und "PASS: ..." bei Erfolg
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
T="$(mktemp -d /tmp/movdb-origin.XXXXXX)"
trap 'rm -rf "$T"' EXIT
fail() { echo "FAIL: $*" >&2; exit 1; }

mkdir -p "$T/bin" "$T/home"
cat > "$T/bin/ssh" <<'EOF'
#!/usr/bin/env bash
case "$*" in
  *uihost*|*UIHOST*) printf 'UIHOST\nUIHOST.local\n10.9.9.9\n' ;;
  *)                 printf 'NB-11572\nNB-11572.local\n192.168.178.97\n192.168.178.238\n' ;;
esac
EOF
chmod +x "$T/bin/ssh"

run()   { HOME="$T/home" PATH="$T/bin:$PATH" "$HERE/install-client.sh" "$@" --no-service >/dev/null; }
fresh() { rm -rf "$T/home"; mkdir -p "$T/home"; }
origin(){ grep '^MOVDB_PLAY_ORIGIN=' "$T/home/.config/movdb/player.env" | cut -d= -f2- | tr -d '"'; }
want()  { case ",$(origin)," in *",$1,"*) ;; *) fail "Origin fehlt: $1 (ist: $(origin))" ;; esac; }

# 1) Erkennung: localhost + Host + Hostname/.local/IPs des Servers
fresh; run --server user@server
for o in http://localhost:8000 http://server:8000 http://nb-11572:8000 \
         http://nb-11572.local:8000 http://192.168.178.97:8000 \
         http://192.168.178.238:8000; do want "$o"; done

# 2) Explizites --ui-origin schlaegt die Erkennung
fresh; run --server user@server --ui-origin http://nur-das:9000
[ "$(origin)" = "http://nur-das:9000" ] || fail "explizite Origin nicht respektiert: $(origin)"

# 3) --no-detect: nur localhost + Host
fresh; run --server user@server --no-detect
[ "$(origin)" = "http://localhost:8000,http://server:8000" ] || fail "--no-detect: $(origin)"

# 4) --ui-port wirkt auf alle ermittelten Origins
fresh; run --server user@server --ui-port 9000
want http://localhost:9000; want http://nb-11572.local:9000

# 5) --ui-host: Erkennung gegen den UI-Server, nicht den Medien-Server;
#    Host wird kleingeschrieben (Browsers senden lowercase-Origins)
fresh; run --server user@server --ui-host user@UIHOST
want http://uihost:8000
want http://uihost.local:8000
want http://10.9.9.9:8000
case ",$(origin)," in
  *",http://server:8000,"*) fail "--server faelschlich als Origin: $(origin)" ;;
esac
case "$(origin)" in *[A-Z]*) fail "Uppercase in Origins: $(origin)" ;; esac

# 6) --ui-host + --no-detect: nur localhost + UI-Host
fresh; run --server user@server --ui-host user@UIHOST --no-detect
[ "$(origin)" = "http://localhost:8000,http://uihost:8000" ] || fail "--ui-host/--no-detect: $(origin)"

# 7) SSH-Fehler ist nicht fatal -> Fallback localhost + Host
printf '#!/usr/bin/env bash\nexit 255\n' > "$T/bin/ssh"; chmod +x "$T/bin/ssh"
fresh; run --server user@server
[ "$(origin)" = "http://localhost:8000,http://server:8000" ] || fail "Fallback: $(origin)"

echo "PASS: install-client.sh UI-Origin-Autoerkennung"
