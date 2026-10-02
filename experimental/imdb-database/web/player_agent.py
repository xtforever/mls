#!/usr/bin/env python3
"""Lokaler Play-Agent fuer movdb: startet mpv auf dem Desktop.

Laeuft AUSSERHALB des Docker-Containers, nur an 127.0.0.1 gebunden. Das
Container-Frontend (web/server.py) schickt bei Klick auf den Play-Button
per fetch() einen POST mit dem absoluten Server-Pfad aus der DB. Der Agent
mappt ihn auf den lokalen sshfs-Mount und startet den Player mit einer
Argumentliste (kein Shell).

Konfiguration per Umgebungsvariablen:

  MOVDB_PLAYER      Player-Programm              (Default mpv)
  MOVDB_LOCAL_ROOT  Mount der Server-Wurzel      (Default ~/mnt/server)
  MOVDB_PATH_MAP    Praefix-Map REMOTE=LOCAL;... (Default leer)
  MOVDB_PLAY_ROOTS  erlaubte lokale Wurzeln      (Default LOCAL_ROOT,
                    os.pathsep-getrennt)
  MOVDB_PLAY_EXT    erlaubte Endungen            (Default s. DEFAULT_EXT)
  MOVDB_PLAY_ORIGIN erlaubte UI-Origins          (Default
                    http://localhost:8000, mehrere per Komma)
  MOVDB_PLAY_PORT   HTTP-Port                    (Default 8765)
  MOVDB_MOUNT_CMD   On-Demand-Mount (Argv, z.B. Pfad zu movdb-mount);
                    laeuft, wenn der Zielpfad noch nicht da ist
  MOVDB_MOUNT_WAIT  Wartezeit danach in Sekunden (Default 10)

Sicherheit: Host-Header gegen DNS-Rebinding, Origin-Allowlist (kein
Wildcard, kein Credentials), realpath-Containment + Endungs-Allowlist +
isfile vor jedem Player-Start.
"""
import json
import os
import shlex
import subprocess
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse

DEFAULT_EXT = "mkv,mp4,avi,m4v,webm,ts,mpg,mpeg,mov,wmv,flv"

PLAYER = os.environ.get("MOVDB_PLAYER", "mpv")
LOCAL_ROOT = os.path.expanduser(
    os.environ.get("MOVDB_LOCAL_ROOT", "~/mnt/server")).rstrip("/")
EXTS = {e.strip().lower().lstrip(".")
        for e in os.environ.get("MOVDB_PLAY_EXT", DEFAULT_EXT).split(",")
        if e.strip()}
ORIGINS = {o.strip() for o in os.environ.get(
    "MOVDB_PLAY_ORIGIN", "http://localhost:8000").split(",") if o.strip()}
PORT = int(os.environ.get("MOVDB_PLAY_PORT", "8765"))
MOUNT_CMD = shlex.split(os.environ.get("MOVDB_MOUNT_CMD", ""))
MOUNT_WAIT = float(os.environ.get("MOVDB_MOUNT_WAIT", "10"))
_MOUNT_LOCK = threading.Lock()


def _parse_map(spec):
    """REMOTE=LOCAL;REMOTE2=LOCAL2 -> [(remote, local), ...]."""
    out = []
    for item in spec.split(";"):
        item = item.strip()
        if "=" not in item:
            continue
        remote, local = item.split("=", 1)
        remote = remote.strip().rstrip("/")
        local = os.path.expanduser(local.strip()).rstrip("/")
        if remote and local:
            out.append((remote, local))
    return out


PATH_MAP = _parse_map(os.environ.get("MOVDB_PATH_MAP", ""))


def _roots():
    raw = os.environ.get("MOVDB_PLAY_ROOTS") or LOCAL_ROOT
    return [os.path.expanduser(p.strip()).rstrip("/")
            for p in raw.split(os.pathsep) if p.strip()]


ROOTS = _roots()


def resolve(dbpath):
    """Server-Pfad -> lokaler Pfad. None, wenn nicht mappbar.

    PATH_MAP hat Vorrang (erstes passendes Praefix gewinnt); sonst wird
    verbatim LOCAL_ROOT vorangestellt. Relative/leere Pfade sind ungueltig.
    """
    if not isinstance(dbpath, str) or not dbpath.startswith("/"):
        return None
    for remote, local in PATH_MAP:
        if dbpath == remote:
            return local
        if dbpath.startswith(remote + "/"):
            return local + dbpath[len(remote):]
    if LOCAL_ROOT:
        return LOCAL_ROOT + dbpath
    return None


def allowed(localpath):
    """realpath-Containment + Endungs-Allowlist + isfile."""
    if not isinstance(localpath, str) or not localpath:
        return False
    try:
        real = os.path.realpath(localpath)
    except OSError:
        return False
    ext = os.path.splitext(real)[1].lower().lstrip(".")
    if ext not in EXTS or not os.path.isfile(real):
        return False
    for root in ROOTS:
        try:
            root = os.path.realpath(root)
            if os.path.commonpath([root, real]) == root:
                return True
        except ValueError:
            continue          # z.B. unterschiedliche Laufwerke
    return False


def build_argv(player, localpath):
    """-- verhindert, dass ein Dateiname als Player-Option gilt."""
    return [player, "--", localpath]


def start_player(argv):
    """Startet den Player ohne Shell, abgekoppelt von der Agent-Session."""
    subprocess.Popen(argv, stdout=subprocess.DEVNULL,
                     stderr=subprocess.DEVNULL, start_new_session=True)


def ensure_mounted(localpath):
    """On-Demand-Mount: MOVDB_MOUNT_CMD (Argv, kein Shell) ausfuehren, warten.

    Nur wenn der Pfad noch nicht erlaubt ist. Ohne MOVDB_MOUNT_CMD passiert
    nichts (False). Der Lock verhindert parallele Mount-Versuche.
    """
    if not MOUNT_CMD:
        return False
    with _MOUNT_LOCK:
        if allowed(localpath):
            return True
        try:
            subprocess.run(MOUNT_CMD, stdout=subprocess.DEVNULL,
                           stderr=subprocess.DEVNULL,
                           timeout=MOUNT_WAIT + 5, check=False)
        except (OSError, subprocess.TimeoutExpired):
            return False
        deadline = time.monotonic() + MOUNT_WAIT
        while time.monotonic() < deadline:
            if allowed(localpath):
                return True
            time.sleep(0.2)
    return False


class Handler(BaseHTTPRequestHandler):
    server_version = "movdb-play/1"

    def _json(self, code, obj, origin=None):
        body = json.dumps(obj).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        if origin and origin in ORIGINS:
            self.send_header("Access-Control-Allow-Origin", origin)
            self.send_header("Vary", "Origin")
        self.end_headers()
        self.wfile.write(body)

    def _host_ok(self):
        """DNS-Rebinding-Schutz: nur Loopback-Host mit eigenem Port."""
        port = self.server.server_address[1]
        return self.headers.get("Host", "") in (
            f"127.0.0.1:{port}", f"localhost:{port}")

    def do_GET(self):
        if not self._host_ok():
            self._json(403, {"ok": False, "error": "host"})
        elif urlparse(self.path).path == "/healthz":
            self._json(200, {"ok": True})
        else:
            self._json(404, {"ok": False, "error": "not found"})

    def do_OPTIONS(self):
        origin = self.headers.get("Origin")
        if not self._host_ok():
            self._json(403, {"ok": False, "error": "host"})
        elif urlparse(self.path).path != "/play":
            self._json(404, {"ok": False, "error": "not found"})
        elif origin not in ORIGINS:
            self._json(403, {"ok": False, "error": "origin"})
        else:
            self.send_response(204)
            self.send_header("Access-Control-Allow-Origin", origin)
            self.send_header("Access-Control-Allow-Methods", "POST, OPTIONS")
            self.send_header("Access-Control-Allow-Headers", "Content-Type")
            self.send_header("Access-Control-Max-Age", "600")
            self.send_header("Vary", "Origin")
            self.send_header("Content-Length", "0")
            self.end_headers()

    def do_POST(self):
        origin = self.headers.get("Origin")
        if not self._host_ok():
            self._json(403, {"ok": False, "error": "host"})
            return
        if urlparse(self.path).path != "/play":
            self._json(404, {"ok": False, "error": "not found"})
            return
        if origin and origin not in ORIGINS:
            self._json(403, {"ok": False, "error": "origin"})
            return
        try:
            n = int(self.headers.get("Content-Length", "0") or "0")
            data = json.loads(self.rfile.read(n).decode("utf-8"))
            dbpath = data["path"]
        except (ValueError, KeyError, TypeError):
            self._json(400, {"ok": False, "error": "json"}, origin)
            return
        local = resolve(dbpath)
        if not local or not (allowed(local) or ensure_mounted(local)):
            self._json(403, {"ok": False, "error": "path"}, origin)
            return
        try:
            start_player(build_argv(PLAYER, local))
        except OSError as e:
            self._json(500, {"ok": False, "error": str(e)}, origin)
            return
        self._json(200, {"ok": True, "path": local}, origin)

    def log_message(self, *args):
        pass


def main():
    print(f"movdb Play-Agent auf http://127.0.0.1:{PORT}", flush=True)
    ThreadingHTTPServer(("127.0.0.1", PORT), Handler).serve_forever()


if __name__ == "__main__":
    main()
