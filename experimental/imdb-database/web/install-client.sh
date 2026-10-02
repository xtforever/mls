#!/usr/bin/env bash
# Installiert den lokalen Play-Agenten auf einem Linux-Client, der das
# movdb-Such-Frontend (Container auf dem Server) benutzt. Kein sudo,
# idempotent. Die UI-Origin wird ohne --ui-origin per SSH ermittelt
# (Hostname, .local, IPs) und zusammen mit localhost eingetragen; als
# Ziel dient --ui-host, sonst der Host aus --server/--remote. Der Agent
# selbst bleibt web/player_agent.py.
#
#   ./install-client.sh --server user@server
#   ./install-client.sh --server user@media --ui-host user@ui
#   ./install-client.sh --server user@server --ui-origin http://server:8000
#   ./install-client.sh --help
#
# Installiert:
#   ~/.local/lib/movdb/player_agent.py      Agent (Kopie)
#   ~/.config/movdb/player.env              Konfiguration (0600, Token)
#   ~/.local/bin/movdb-play-agent           Launcher
#   ~/.local/bin/movdb-mount                sshfs-Helfer (up/down/print)
#   ~/.config/systemd/user/movdb-play-agent.service   (abschaltbar)
set -euo pipefail

die() { echo "Fehler: $*" >&2; exit 1; }
warn() { echo "Warnung: $*" >&2; }
info() { echo "$*"; }

usage() {
  cat <<'EOF'
Installiert den movdb Play-Agenten auf dem Client (kein sudo).

Optionen:
  --server HOST       SSH-Ziel, z. B. user@server (Remote wird HOST:/)
  --remote REMOTE     vollstaendiges sshfs-Remote (Default: <server>:/)
  --local-root PATH   lokaler Mount (Default: ~/mnt/server)
  --player NAME       Player-Programm (Default: mpv)
  --ui-origin ORIGIN  erlaubte UI-Origin(s), Komma-getrennt (explizit,
                      ueberschreibt die Erkennung)
  --ui-host HOST      SSH-Ziel des UI-Servers fuer die Origin-Ermittlung,
                      falls dieser nicht --server/--remote ist
                      (Default: Host aus --server/--remote)
  --ui-port N         Port der Such-UI (Default: 8000); Basis der
                      automatischen Origin-Ermittlung
  --no-detect         keine SSH-Origin-Ermittlung (nur localhost + Host)
  --port N            Agent-Port (Default: 8765)
  --agent-src PATH    Pfad zu player_agent.py (Default: neben diesem Skript)
  --ssh-key PATH      IdentityFile fuer sshfs/systemd
  --no-service        keinen systemd-User-Dienst einrichten
  --mount-service     zusaetzlich systemd-User-Dienst fuer den Mount
  --mount-on-play     bei Play automatisch mounten (Default, sobald
                      --server/--remote gesetzt ist)
  --no-mount-on-play  kein automatischer Mount bei Play
  --new-token         neues Token erzeugen (sonst bleibt das vorhandene)
  --uninstall         Agent/Dienste entfernen (Config nur mit --purge)
  --purge             mit --uninstall: auch ~/.config/movdb entfernen
  -h, --help          diese Hilfe

Nach der Installation: Token aus der Ausgabe ins Feld "Play-Token" der
Such-UI eintragen. Mount: `movdb-mount up` (oder Dienst mit --mount-service).
EOF
}

SERVER=""
REMOTE=""
LOCAL_ROOT="$HOME/mnt/server"
PLAYER="mpv"
UI_ORIGIN=""          # leer = automatisch ermitteln
UI_HOST=""            # SSH-Ziel UI-Server (leer = Host aus --server/--remote)
UI_PORT="8000"
NO_DETECT=0
PORT="8765"
AGENT_SRC=""
SSH_KEY=""
NO_SERVICE=0
MOUNT_SERVICE=0
MOUNT_ON_PLAY=auto
NEW_TOKEN=0
UNINSTALL=0
PURGE=0

while [ $# -gt 0 ]; do
  case "$1" in
    --server) SERVER="${2:?Wert fuer $1 fehlt}"; shift 2 ;;
    --remote) REMOTE="${2:?Wert fuer $1 fehlt}"; shift 2 ;;
    --local-root) LOCAL_ROOT="${2:?Wert fuer $1 fehlt}"; shift 2 ;;
    --player) PLAYER="${2:?Wert fuer $1 fehlt}"; shift 2 ;;
    --ui-origin) UI_ORIGIN="${2:?Wert fuer $1 fehlt}"; shift 2 ;;
    --ui-host) UI_HOST="${2:?Wert fuer $1 fehlt}"; shift 2 ;;
    --ui-port) UI_PORT="${2:?Wert fuer $1 fehlt}"; shift 2 ;;
    --no-detect) NO_DETECT=1; shift ;;
    --port) PORT="${2:?Wert fuer $1 fehlt}"; shift 2 ;;
    --agent-src) AGENT_SRC="${2:?Wert fuer $1 fehlt}"; shift 2 ;;
    --ssh-key) SSH_KEY="${2:?Wert fuer $1 fehlt}"; shift 2 ;;
    --no-service) NO_SERVICE=1; shift ;;
    --mount-service) MOUNT_SERVICE=1; shift ;;
    --mount-on-play) MOUNT_ON_PLAY=1; shift ;;
    --no-mount-on-play) MOUNT_ON_PLAY=0; shift ;;
    --new-token) NEW_TOKEN=1; shift ;;
    --uninstall) UNINSTALL=1; shift ;;
    --purge) PURGE=1; shift ;;
    -h|--help) usage; exit 0 ;;
    *) die "unbekannte Option '$1' (--help fuer Hilfe)" ;;
  esac
done

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
[ -n "$AGENT_SRC" ] || AGENT_SRC="$HERE/player_agent.py"

LIB_DIR="$HOME/.local/lib/movdb"
BIN_DIR="$HOME/.local/bin"
CONF_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/movdb"
UNIT_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/systemd/user"
ENV_FILE="$CONF_DIR/player.env"
AGENT_DST="$LIB_DIR/player_agent.py"
LAUNCHER="$BIN_DIR/movdb-play-agent"
MOUNT="$BIN_DIR/movdb-mount"
AGENT_UNIT="$UNIT_DIR/movdb-play-agent.service"
MOUNT_UNIT="$UNIT_DIR/movdb-mount.service"

# ---------------------------------------------------------------- Uninstall
if [ "$UNINSTALL" = 1 ]; then
  if command -v systemctl >/dev/null 2>&1; then
    systemctl --user disable --now movdb-play-agent.service movdb-mount.service \
      >/dev/null 2>&1 || true
    rm -f "$AGENT_UNIT" "$MOUNT_UNIT"
    systemctl --user daemon-reload >/dev/null 2>&1 || true
  fi
  rm -f "$AGENT_DST" "$LAUNCHER" "$MOUNT"
  [ "$PURGE" = 1 ] && rm -rf "$CONF_DIR"
  info "movdb Play-Agent entfernt."
  exit 0
fi

# ------------------------------------------------------------------- Checks
command -v python3 >/dev/null 2>&1 || die "python3 fehlt im PATH"
[ -f "$AGENT_SRC" ] || die "player_agent.py nicht gefunden: $AGENT_SRC (--agent-src)"
case "$PORT" in ''|*[!0-9]*) die "--port muss eine Zahl sein: $PORT" ;; esac
command -v sshfs >/dev/null 2>&1 || warn "sshfs fehlt -> Mount nicht moeglich (z. B. apt install sshfs)"
command -v "$PLAYER" >/dev/null 2>&1 || warn "Player '$PLAYER' nicht im PATH"
if [ -z "$REMOTE" ]; then
  [ -n "$SERVER" ] && REMOTE="$SERVER:/" || \
    warn "kein --server/--remote: movdb-mount braucht dann MOVDB_REMOTE"
fi
if [ "$MOUNT_ON_PLAY" = auto ]; then
  if [ -n "$REMOTE" ]; then MOUNT_ON_PLAY=1; else MOUNT_ON_PLAY=0; fi
fi
case "$UI_PORT" in ''|*[!0-9]*) die "--ui-port muss eine Zahl sein: $UI_PORT" ;; esac

# ---------------------------------------------------- UI-Origin ermitteln
# Ohne --ui-origin: localhost + Host aus --ui-host (sonst --server/--remote)
# + (per SSH) dessen Hostname/.local-Namen/IPs, damit der Browser die UI
# unter jeder dieser Adressen oeffnen darf. SSH-Fehler sind nicht fatal.
detect_ui_origins() {
  local port="$1" target="$2" key="$3" n host
  local -a origins=("http://localhost:$port")
  host="${target##*@}"
  [ -n "$host" ] && origins+=("http://$host:$port")
  if [ -n "$target" ] && [ "$NO_DETECT" = 0 ] && command -v ssh >/dev/null 2>&1; then
    local -a opts=(-o BatchMode=yes -o ConnectTimeout=5
                   -o StrictHostKeyChecking=accept-new)
    [ -n "$key" ] && opts+=(-i "$key")
    local names
    names="$(ssh "${opts[@]}" "$target" '
      h=$(hostname 2>/dev/null) || exit 0
      printf "%s\n%s.local\n" "$h" "$h"
      hf=$(hostname -f 2>/dev/null)
      [ -n "$hf" ] && [ "$hf" != "$h" ] && printf "%s\n" "$hf"
      hostname -I 2>/dev/null | tr " " "\n"
    ' 2>/dev/null || true)"
    while IFS= read -r n; do
      [ -n "$n" ] || continue
      n="$(printf '%s' "$n" | tr '[:upper:]' '[:lower:]')"
      case "$n" in *[!a-z0-9._-]*) continue ;; esac
      origins+=("http://$n:$port")
    done <<< "$names"
  fi
  printf '%s\n' "${origins[@]}" | awk '!seen[$0]++' | paste -sd, -
}

if [ -z "$UI_ORIGIN" ]; then
  SSH_TARGET="$UI_HOST"
  if [ -z "$SSH_TARGET" ]; then
    SSH_TARGET="$SERVER"
    if [ -z "$SSH_TARGET" ] && [ -n "$REMOTE" ]; then
      SSH_TARGET="${REMOTE%%:*}"
    fi
  fi
  UI_ORIGIN="$(detect_ui_origins "$UI_PORT" "$SSH_TARGET" "$SSH_KEY")"
fi

# --------------------------------------------------------------- Dateien
mkdir -p "$LIB_DIR" "$BIN_DIR" "$CONF_DIR" "$LOCAL_ROOT"
install -m 0644 "$AGENT_SRC" "$AGENT_DST"

TOKEN=""
if [ "$NEW_TOKEN" = 0 ] && [ -f "$ENV_FILE" ]; then
  TOKEN="$(grep -m1 '^MOVDB_PLAY_TOKEN=' "$ENV_FILE" 2>/dev/null \
    | cut -d= -f2- | tr -d '"' || true)"
fi
[ -n "$TOKEN" ] || TOKEN="$(python3 -c 'import secrets;print(secrets.token_urlsafe(32))')"

umask 077
cat > "$ENV_FILE" <<EOF
# movdb Play-Agent (Client) - erzeugt von install-client.sh
MOVDB_PLAYER="$PLAYER"
MOVDB_LOCAL_ROOT="$LOCAL_ROOT"
MOVDB_PLAY_ORIGIN="$UI_ORIGIN"
MOVDB_PLAY_PORT="$PORT"
MOVDB_PLAY_TOKEN="$TOKEN"
EOF
if [ "$MOUNT_ON_PLAY" = 1 ]; then
  echo "MOVDB_MOUNT_CMD=\"$MOUNT\"" >> "$ENV_FILE"
fi
chmod 600 "$ENV_FILE"
umask 022

cat > "$LAUNCHER" <<'EOF'
#!/usr/bin/env bash
# Startet den movdb Play-Agenten mit der Client-Konfiguration.
set -euo pipefail
CONF="${XDG_CONFIG_HOME:-$HOME/.config}/movdb/player.env"
[ -r "$CONF" ] || { echo "Konfiguration fehlt: $CONF" >&2; exit 1; }
set -a; . "$CONF"; set +a
exec python3 "$HOME/.local/lib/movdb/player_agent.py"
EOF
chmod 0755 "$LAUNCHER"

cat > "$MOUNT" <<EOF
#!/usr/bin/env bash
# sshfs-Mount fuer movdb (Server-Wurzel, read-only). up|down|print
set -euo pipefail
REMOTE="\${MOVDB_REMOTE:-$REMOTE}"
LOCAL_ROOT="\${MOVDB_LOCAL_ROOT:-$LOCAL_ROOT}"
SSH_KEY="$SSH_KEY"
OPTS=(-o ro -o reconnect -o ServerAliveInterval=15 -o ServerAliveCountMax=3
      -o ConnectTimeout=10 -o cache=yes -o kernel_cache)
[ -n "\$SSH_KEY" ] && OPTS+=(-o "IdentityFile=\$SSH_KEY")
case "\${1:-up}" in
  print) printf '%q ' sshfs "\$REMOTE" "\$LOCAL_ROOT" "\${OPTS[@]}"; echo ;;
  up)
    [ -n "\$REMOTE" ] || { echo "Kein REMOTE (--server/--remote oder MOVDB_REMOTE)" >&2; exit 1; }
    mkdir -p "\$LOCAL_ROOT"
    if mountpoint -q "\$LOCAL_ROOT"; then echo "bereits gemountet: \$LOCAL_ROOT"; exit 0; fi
    sshfs "\$REMOTE" "\$LOCAL_ROOT" "\${OPTS[@]}" ;;
  down)
    fusermount3 -u "\$LOCAL_ROOT" 2>/dev/null || fusermount -u "\$LOCAL_ROOT" ;;
  *) echo "usage: movdb-mount {up|down|print}" >&2; exit 2 ;;
esac
EOF
chmod 0755 "$MOUNT"

# --------------------------------------------------------------- systemd
if [ "$NO_SERVICE" = 0 ] && command -v systemctl >/dev/null 2>&1; then
  mkdir -p "$UNIT_DIR"
  cat > "$AGENT_UNIT" <<EOF
[Unit]
Description=movdb Play-Agent (lokaler Video-Player)
After=network-online.target

[Service]
Type=simple
ExecStart=$LAUNCHER
Restart=on-failure
RestartSec=2

[Install]
WantedBy=default.target
EOF
  if [ "$MOUNT_SERVICE" = 1 ]; then
    cat > "$MOUNT_UNIT" <<EOF
[Unit]
Description=movdb sshfs-Mount
After=network-online.target

[Service]
Type=oneshot
RemainAfterExit=yes
ExecStart=$MOUNT up
ExecStop=$MOUNT down

[Install]
WantedBy=default.target
EOF
  fi
  systemctl --user daemon-reload >/dev/null 2>&1 || true
  systemctl --user enable --now movdb-play-agent.service >/dev/null 2>&1 \
    || warn "Agent-Dienst nicht gestartet (systemctl --user verfuegbar?)"
  if [ "$MOUNT_SERVICE" = 1 ]; then
    systemctl --user enable --now movdb-mount.service >/dev/null 2>&1 \
      || warn "Mount-Dienst nicht gestartet (SSH-Key/Netz pruefen)"
  fi
fi

# --------------------------------------------------------------- Ausgabe
info "movdb Play-Agent installiert."
info "  Agent:    $LAUNCHER  (127.0.0.1:$PORT)"
info "  Config:   $ENV_FILE"
info "  Mount:    $MOUNT up   (lokal: $LOCAL_ROOT)"
if [ "$MOUNT_ON_PLAY" = 1 ]; then
  info "  On-Play:  automatischer Mount an (MOVDB_MOUNT_CMD)"
else
  info "  On-Play:  automatischer Mount aus"
fi
info "  Player:   $PLAYER"
info "  UI-Origin: $UI_ORIGIN"
info "  Token:    $TOKEN"
info ""
info "Token in der Such-UI ins Feld \"Play-Token\" eintragen, dann ▶ klicken."
