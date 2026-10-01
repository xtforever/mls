#!/usr/bin/env python3
"""Selbsttest fuer web/server.py ohne echten movdb (subprocess wird ersetzt).

Prueft: mehrere Genres -> je ein --genre, Reihenfolge (Flags vor DB-Datei
vor Termen) und tolerante UTF-8-Dekodierung von Pfaden.
"""
import importlib.util
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location("srv", os.path.join(HERE, "server.py"))
srv = importlib.util.module_from_spec(spec)
spec.loader.exec_module(srv)

calls = []


class R:
    def __init__(self, out):
        self.stdout = out
        self.stderr = b""
        self.returncode = 0


def fake_run(argv, **kw):
    calls.append(argv)
    out = (b"Goldfinger\t1964\ttt0058150\t7.7\t220046\t110\t"
           b"Action,Adventure,Thriller\t/p/\x81bad.mkv\n")
    out += (b"Shogun\t2024\ttt2788316\t8.6\t253174\t60\t"
            b"Action,Adventure,Drama\t/p/shogun.mkv\n")
    return R(out)


srv.subprocess.run = fake_run

r = srv.search({"genre": ["Action", "Adventure"], "min_rating": ["8.0"]})
argv = calls[-1]
assert argv.count("--genre") == 2, argv
assert argv[argv.index("--genre") + 1] == "Action", argv
assert argv[argv.index("--min-rating") + 1] == "8.0", argv
assert argv[-1] == srv.BIN, argv            # keine Terme -> Datei zuletzt
assert r["count"] == 2, r
assert r["rows"][0]["title"] == "Goldfinger", r
assert "bad.mkv" in r["rows"][0]["path"], r  # ungueltiges UTF-8 -> kein Crash

# leere Anfrage liefert Hinweis, kein Subprozess
n = len(calls)
r = srv.search({})
assert r["count"] == 0 and "note" in r, r
assert len(calls) == n, "leere Anfrage darf movdb nicht aufrufen"

# Wartung: feste Allowlist, force nur beim Download, unbekannt -> Fehler
assert srv._steps("update-imdb", False)[0][0] == srv.DOWNLOAD
assert srv._steps("rebuild", True)[0] == [srv.DOWNLOAD, "--force"]


def _argv(step):
    return step[0] if isinstance(step, tuple) else step


assert all("download.sh" not in " ".join(_argv(s))
           for s in srv._steps("reindex", False))
assert srv._steps("nope", False) is None
ok, err = srv.JOB.start("nope", False)
assert not ok and "unbekannt" in err, (ok, err)

print("web/server.py: ok")
