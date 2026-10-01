#!/usr/bin/env python3
"""Selbsttest fuer web/player_agent.py: kein Player, kein Display, kein Mount.

Laedt das Modul mehrfach mit verschiedenen Env-Werten per importlib und
ersetzt subprocess durch einen Fake, der die argv sammelt. Prueft Mapping,
Allowlist (inkl. Symlink-Eskalation), Argv-Bau, HTTP-Token/Origin und dass
bei Ablehnung nie ein Player gestartet wird.
"""
import http.client
import importlib.util
import json
import os
import subprocess
import tempfile
import threading
import types
import urllib.error
import urllib.request
from http.server import ThreadingHTTPServer

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "player_agent.py")
ENV_KEYS = ["MOVDB_PLAYER", "MOVDB_LOCAL_ROOT", "MOVDB_PATH_MAP",
            "MOVDB_PLAY_ROOTS", "MOVDB_PLAY_EXT", "MOVDB_PLAY_ORIGIN",
            "MOVDB_PLAY_PORT", "MOVDB_PLAY_TOKEN", "MOVDB_MOUNT_CMD",
            "MOVDB_MOUNT_WAIT"]

calls = []
run_calls = []
on_run = None            # optionaler Hook fuer den Fake von subprocess.run


class FakePopen:
    def __init__(self, argv, **kw):
        calls.append((argv, kw))


class FakeRun:
    def __init__(self, argv, **kw):
        run_calls.append((argv, kw))
        if on_run:
            on_run(argv)


def load(env):
    """Modul frisch laden (Env isoliert) und subprocess durch Fake ersetzen."""
    saved = {k: os.environ.get(k) for k in ENV_KEYS}
    for k in ENV_KEYS:
        os.environ.pop(k, None)
    os.environ.update(env)
    try:
        spec = importlib.util.spec_from_file_location("agent", SRC)
        m = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(m)
    finally:
        for k in ENV_KEYS:
            if saved[k] is None:
                os.environ.pop(k, None)
            else:
                os.environ[k] = saved[k]
    m.subprocess = types.SimpleNamespace(
        Popen=FakePopen, run=FakeRun, DEVNULL=subprocess.DEVNULL,
        TimeoutExpired=subprocess.TimeoutExpired)
    return m


# --------------------------------------------------------------- Mapping
m = load({"MOVDB_LOCAL_ROOT": "/tmp/mnt", "MOVDB_PATH_MAP": ""})
assert m.resolve("/jensbk/a.mkv") == "/tmp/mnt/jensbk/a.mkv", m.resolve("/jensbk/a.mkv")
assert m.resolve("rel/a.mkv") is None
assert m.resolve("") is None
assert m.resolve(123) is None

m = load({"MOVDB_LOCAL_ROOT": "/tmp/mnt", "MOVDB_PATH_MAP": "/jensbk=/tmp/mnt"})
assert m.resolve("/jensbk/a.mkv") == "/tmp/mnt/a.mkv", m.resolve("/jensbk/a.mkv")
assert m.resolve("/jensbk") == "/tmp/mnt"          # exaktes Praefix
assert m.resolve("/other/a.mkv") == "/tmp/mnt/other/a.mkv"  # Fallback Root

# Praefixgrenze: /srv/movies darf nicht /srv/moviesX/... matchen
m = load({"MOVDB_LOCAL_ROOT": "/tmp/mnt",
          "MOVDB_PATH_MAP": "/srv/movies=/tmp/mnt"})
assert m.resolve("/srv/moviesX/a.mkv") == "/tmp/mnt/srv/moviesX/a.mkv", \
    m.resolve("/srv/moviesX/a.mkv")
m = load({"MOVDB_LOCAL_ROOT": "", "MOVDB_PATH_MAP": "/srv/movies=/tmp/mnt"})
assert m.resolve("/srv/moviesX/a.mkv") is None

m = load({"MOVDB_LOCAL_ROOT": "", "MOVDB_PATH_MAP": "/jensbk=/tmp/mnt"})
assert m.resolve("/other/a.mkv") is None            # nicht gemappt -> abgelehnt
assert m.resolve("/jensbk/a.mkv") == "/tmp/mnt/a.mkv"

# ------------------------------------------------------------- Allowlist
n0 = len(calls)
with tempfile.TemporaryDirectory() as d:
    ok = os.path.join(d, "ok.mkv")
    txt = os.path.join(d, "note.txt")
    open(ok, "wb").close()
    open(txt, "wb").close()
    evil = os.path.join(d, "evil.mkv")
    os.symlink("/etc/passwd", evil)                 # Symlink-Eskalation

    m = load({"MOVDB_LOCAL_ROOT": d, "MOVDB_PLAY_ROOTS": d,
              "MOVDB_PATH_MAP": ""})
    assert m.allowed(ok) is True
    assert m.allowed(txt) is False
    assert m.allowed(d) is False                    # Verzeichnis
    assert m.allowed(os.path.join(d, "missing.mkv")) is False
    assert m.allowed("../../etc/passwd") is False   # relativ/ausserhalb
    assert m.allowed(evil) is False                 # realpath ausserhalb Root
    assert m.allowed("") is False

# commonpath-Sibling: /.../rootX liegt nicht in /.../root
with tempfile.TemporaryDirectory() as parent:
    r = os.path.join(parent, "root")
    sib = os.path.join(parent, "rootX")
    os.mkdir(r)
    os.mkdir(sib)
    f = os.path.join(sib, "x.mkv")
    open(f, "wb").close()
    m = load({"MOVDB_LOCAL_ROOT": r, "MOVDB_PLAY_ROOTS": r,
              "MOVDB_PATH_MAP": ""})
    assert m.allowed(f) is False, "commonpath-Sibling darf nicht erlaubt sein"
assert len(calls) == n0, "Allowlist-Pruefung darf keinen Player starten"

# ------------------------------------------------------------------ Argv
m = load({})
argv = m.build_argv("mpv", "/tmp/mnt/ok.mkv")
assert argv == ["mpv", "--", "/tmp/mnt/ok.mkv"], argv
argv = m.build_argv("mpv", "/tmp/mnt/-weird.mkv")
assert argv == ["mpv", "--", "/tmp/mnt/-weird.mkv"], argv
assert argv[1] == "--"

calls.clear()
m.start_player(argv)
assert len(calls) == 1
_argv, kw = calls[0]
assert isinstance(_argv, list) and _argv[0] == "mpv", _argv
assert "shell" not in kw and kw.get("start_new_session") is True, kw

src = open(SRC, encoding="utf-8").read()
assert "shell=True" not in src, "kein shell=True im Agenten"

# ------------------------------------------------------- On-Demand-Mount
run_calls.clear()
with tempfile.TemporaryDirectory() as d:
    late = os.path.join(d, "late.mkv")
    # ohne MOVDB_MOUNT_CMD: nichts tun, kein run
    m = load({"MOVDB_LOCAL_ROOT": d, "MOVDB_PLAY_ROOTS": d,
              "MOVDB_PATH_MAP": ""})
    assert m.ensure_mounted(late) is False
    assert run_calls == [], run_calls
    # mit MOVDB_MOUNT_CMD: Befehl laufen lassen, danach erneut pruefen
    on_run = lambda argv: open(late, "wb").close()
    m = load({"MOVDB_LOCAL_ROOT": d, "MOVDB_PLAY_ROOTS": d,
              "MOVDB_PATH_MAP": "", "MOVDB_MOUNT_CMD": "movdb-mount",
              "MOVDB_MOUNT_WAIT": "1"})
    assert m.ensure_mounted(late) is True
    assert run_calls and run_calls[-1][0] == ["movdb-mount"], run_calls
    assert "shell" not in run_calls[-1][1], run_calls[-1][1]
    # bereits vorhanden -> kein zweiter Mount-Aufruf
    n = len(run_calls)
    assert m.ensure_mounted(late) is True
    assert len(run_calls) == n, run_calls
on_run = None

# ------------------------------------------------------------------ HTTP
with tempfile.TemporaryDirectory() as d:
    root = os.path.dirname(d)
    rel = "/" + os.path.basename(d)
    ok_rel = rel + "/ok.mkv"
    open(os.path.join(d, "ok.mkv"), "wb").close()
    open(os.path.join(d, "note.txt"), "wb").close()
    m = load({"MOVDB_LOCAL_ROOT": root, "MOVDB_PLAY_ROOTS": d,
              "MOVDB_PATH_MAP": "", "MOVDB_PLAY_TOKEN": "testtoken",
              "MOVDB_PLAY_ORIGIN": "http://localhost:8000"})
    calls.clear()
    httpd = ThreadingHTTPServer(("127.0.0.1", 0), m.Handler)
    port = httpd.server_address[1]
    threading.Thread(target=httpd.serve_forever, daemon=True).start()
    base = f"http://127.0.0.1:{port}"

    def post(token, dbpath, origin=None):
        req = urllib.request.Request(
            base + "/play", data=json.dumps({"path": dbpath}).encode(),
            method="POST")
        req.add_header("Content-Type", "application/json")
        if token is not None:
            req.add_header("X-Movdb-Play", token)
        if origin:
            req.add_header("Origin", origin)
        try:
            with urllib.request.urlopen(req) as r:
                return r.status, json.load(r)
        except urllib.error.HTTPError as e:
            return e.code, json.load(e)

    def raw(method, path, headers=None, body=None):
        """http.client, damit der Host-Header frei gesetzt werden kann."""
        c = http.client.HTTPConnection("127.0.0.1", port, timeout=5)
        c.request(method, path, body=body, headers=headers or {})
        r = c.getresponse()
        st = r.status
        r.read()
        c.close()
        return st

    try:
        with urllib.request.urlopen(base + "/healthz") as r:
            assert r.status == 200 and json.load(r) == {"ok": True}

        # Preflight nur fuer erlaubte Origin
        req = urllib.request.Request(base + "/play", method="OPTIONS")
        req.add_header("Origin", "http://localhost:8000")
        with urllib.request.urlopen(req) as r:
            assert r.status == 204, r.status
            assert r.headers["Access-Control-Allow-Origin"] == \
                "http://localhost:8000"
            assert "X-Movdb-Play" in r.headers["Access-Control-Allow-Headers"]

        st, _ = post(None, ok_rel)              # ohne Token
        assert st == 403, st
        st, _ = post("falsch", ok_rel)          # falscher Token
        assert st == 403, st
        assert len(calls) == 0, "ohne Token darf kein Player starten"

        st, j = post("testtoken", ok_rel)       # korrekt
        assert st == 200 and j["ok"] is True, (st, j)
        assert len(calls) == 1, calls
        _argv, _kw = calls[0]
        assert _argv == ["mpv", "--", os.path.join(d, "ok.mkv")], _argv

        st, _ = post("testtoken", rel + "/note.txt")   # falsche Endung
        assert st == 403, st
        st, _ = post("testtoken", "/nix/gibts.mkv")    # nicht gemappt/fehlt
        assert st == 403, st
        st, _ = post("testtoken", ok_rel, origin="http://evil.example")
        assert st == 403, st

        # Host-Header (DNS-Rebinding) -> 403, auch mit gueltigem Token
        assert raw("GET", "/healthz", {"Host": "evil.example"}) == 403
        body = json.dumps({"path": ok_rel}).encode()
        assert raw("POST", "/play",
                   {"Host": "evil.example", "X-Movdb-Play": "testtoken",
                    "Content-Type": "application/json"}, body) == 403

        # OPTIONS mit fremder Origin -> 403
        assert raw("OPTIONS", "/play",
                   {"Origin": "http://evil.example"}) == 403
        # POST auf unbekannten Pfad -> 404 (vor Token)
        assert raw("POST", "/nope", {"X-Movdb-Play": "testtoken"}) == 404
        # Kaputtes JSON mit gueltigem Token -> 400
        assert raw("POST", "/play",
                   {"X-Movdb-Play": "testtoken",
                    "Content-Type": "application/json"}, b"{not json") == 400

        assert len(calls) == 1, "abgelehnte Anfragen duerfen nicht starten"
    finally:
        httpd.shutdown()
        httpd.server_close()

# On-Demand-Mount im HTTP-Pfad: Datei fehlt -> Mount -> Player
with tempfile.TemporaryDirectory() as d:
    root = os.path.dirname(d)
    late_rel = "/" + os.path.basename(d) + "/late.mkv"
    late_abs = os.path.join(d, "late.mkv")
    on_run = lambda argv: open(late_abs, "wb").close()
    m = load({"MOVDB_LOCAL_ROOT": root, "MOVDB_PLAY_ROOTS": d,
              "MOVDB_PATH_MAP": "", "MOVDB_PLAY_TOKEN": "t2",
              "MOVDB_PLAY_ORIGIN": "http://localhost:8000",
              "MOVDB_MOUNT_CMD": "movdb-mount", "MOVDB_MOUNT_WAIT": "1"})
    calls.clear()
    run_calls.clear()
    httpd = ThreadingHTTPServer(("127.0.0.1", 0), m.Handler)
    port = httpd.server_address[1]
    threading.Thread(target=httpd.serve_forever, daemon=True).start()
    try:
        req = urllib.request.Request(
            f"http://127.0.0.1:{port}/play",
            data=json.dumps({"path": late_rel}).encode(), method="POST")
        req.add_header("Content-Type", "application/json")
        req.add_header("X-Movdb-Play", "t2")
        with urllib.request.urlopen(req) as r:
            assert r.status == 200 and json.load(r)["ok"] is True
        assert len(calls) == 1, calls
        assert run_calls and run_calls[-1][0] == ["movdb-mount"], run_calls
    finally:
        httpd.shutdown()
        httpd.server_close()
    on_run = None

print("web/player_agent.py: ok")
