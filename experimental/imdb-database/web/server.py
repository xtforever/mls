#!/usr/bin/env python3
"""Kleines Web-Frontend fuer movdb.exed (Titelsuche + Meta-Filter).

Nur Python-Standardbibliothek. Ruft movdb.exed als Subprozess auf
(Argumentliste, kein Shell). Konfiguration per Umgebungsvariablen:

  MOVDB_EXE   Pfad zu movdb.exed        (Default ./movdb.exed)
  MOVDB_BIN   Pfad zur movies.bin       (Default movies.bin)
  MOVDB_META  Pfad zur movies-meta.bin  (Default movies-meta.bin)
  PORT        HTTP-Port                 (Default 8000)
  BIND        Bind-Adresse              (Default 0.0.0.0)
"""
import json
import os
import subprocess
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse

EXE = os.environ.get("MOVDB_EXE", "./movdb.exed")
BIN = os.environ.get("MOVDB_BIN", "movies.bin")
META = os.environ.get("MOVDB_META", "movies-meta.bin")
PORT = int(os.environ.get("PORT", "8000"))
BIND = os.environ.get("BIND", "0.0.0.0")

GENRES = [
    "Action", "Adult", "Adventure", "Animation", "Biography", "Comedy",
    "Crime", "Documentary", "Drama", "Family", "Fantasy", "Film-Noir",
    "Game-Show", "History", "Horror", "Music", "Musical", "Mystery",
    "News", "Reality-TV", "Romance", "Sci-Fi", "Short", "Sport",
    "Talk-Show", "Thriller", "War", "Western",
]
TYPES = ["movie", "tvSeries", "tvMovie", "miniSeries", "short", "video",
         "tvSpecial", "tvShort", "special", "other"]
COLS = ["title", "year", "id", "rating", "votes", "runtime", "genres", "path"]


def search(params):
    """Baut die movdb-Argumentliste und liefert das Ergebnis als dict."""
    argv = [EXE, "query", "--meta", META]
    genre = params.get("genre", "").strip()
    min_rating = params.get("min_rating", "").strip()
    year_range = params.get("year_range", "").strip()
    typ = params.get("type", "").strip()
    terms = params.get("q", "").split()
    if genre:
        argv += ["--genre", genre]
    if min_rating:
        argv += ["--min-rating", min_rating]
    if year_range:
        argv += ["--year-range", year_range]
    if typ:
        argv += ["--type", typ]
    if not terms and not (genre or min_rating or year_range or typ):
        return {"count": 0, "rows": [],
                "note": "Bitte Suchbegriff oder Filter angeben."}
    argv.append(BIN)          # Datei vor den Termen -> Terme nie als Flag
    argv += terms
    try:
        p = subprocess.run(argv, capture_output=True, text=True, timeout=60)
    except (OSError, subprocess.TimeoutExpired) as e:
        return {"error": str(e)}
    if p.returncode not in (0, 1):
        return {"error": (p.stderr or f"rc={p.returncode}").strip()}
    rows = []
    for line in p.stdout.splitlines():
        f = line.split("\t")
        if len(f) >= len(COLS):
            rows.append(dict(zip(COLS, f[:len(COLS)])))
    return {"count": len(rows), "rows": rows}


def page():
    genres = "".join(f'<option>{g}</option>' for g in GENRES)
    types = "".join(f'<option value="{t}">{t}</option>' for t in TYPES)
    return PAGE.replace("{{GENRES}}", genres).replace("{{TYPES}}", types)


PAGE = """<!doctype html>
<html lang="de">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>movdb Suche</title>
<style>
 body{font:14px/1.45 system-ui,sans-serif;margin:0;background:#111;color:#eee}
 header{padding:16px 20px;background:#1b1b1b;border-bottom:1px solid #333}
 h1{font-size:16px;margin:0 0 12px}
 form{display:flex;flex-wrap:wrap;gap:10px;align-items:flex-end}
 label{display:flex;flex-direction:column;font-size:12px;color:#aaa;gap:4px}
 input,select,button{background:#222;color:#eee;border:1px solid #444;
   border-radius:4px;padding:6px 8px;font:inherit}
 button{background:#2d6cdf;border-color:#2d6cdf;cursor:pointer}
 main{padding:16px 20px}
 #status{color:#aaa;margin-bottom:8px}
 table{border-collapse:collapse;width:100%;font-size:13px}
 th,td{text-align:left;padding:5px 8px;border-bottom:1px solid #2a2a2a;
   vertical-align:top}
 th{color:#aaa;font-weight:600}
 td.path{max-width:520px;word-break:break-all;color:#8ab}
</style>
</head>
<body>
<header>
 <h1>movdb &mdash; Filme suchen</h1>
 <form id="f">
  <label>Suche <input name="q" placeholder="mord mittsommer" autofocus></label>
  <label>Genre <select name="genre"><option value="">(egal)</option>{{GENRES}}</select></label>
  <label>Rating ab <input name="min_rating" placeholder="8.0" size="4"></label>
  <label>Jahre <input name="year_range" placeholder="1930-1933" size="10"></label>
  <label>Typ <select name="type"><option value="">(egal)</option>{{TYPES}}</select></label>
  <button>suchen</button>
 </form>
</header>
<main>
 <div id="status"></div>
 <table>
  <thead><tr><th>Titel</th><th>Jahr</th><th>ID</th><th>Rating</th>
   <th>Stimmen</th><th>Min</th><th>Genres</th><th>Pfad</th></tr></thead>
  <tbody id="tb"></tbody>
 </table>
</main>
<script>
const f=document.getElementById('f'),tb=document.getElementById('tb'),
      st=document.getElementById('status');
const esc=s=>{const d=document.createElement('div');d.textContent=s;return d.innerHTML};
f.addEventListener('submit',async e=>{
 e.preventDefault();
 st.textContent='suche\\u2026'; tb.innerHTML='';
 try{
  const r=await fetch('/api/search?'+new URLSearchParams(new FormData(f)));
  const j=await r.json();
  if(j.error){st.textContent='Fehler: '+j.error;return}
  st.textContent=j.note||(j.count+' Treffer');
  tb.innerHTML=j.rows.map(x=>'<tr>'+
    ['title','year','id','rating','votes','runtime','genres']
      .map(k=>'<td>'+esc(x[k])+'</td>').join('')+
    '<td class="path">'+esc(x.path)+'</td></tr>').join('');
 }catch(err){st.textContent='Fehler: '+err}
});
</script>
</body>
</html>
"""


class Handler(BaseHTTPRequestHandler):
    def _send(self, body, ctype):
        self.send_response(200)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        u = urlparse(self.path)
        if u.path == "/api/search":
            params = {k: v[0] for k, v in parse_qs(u.query).items()}
            body = json.dumps(search(params), ensure_ascii=False).encode()
            self._send(body, "application/json; charset=utf-8")
        elif u.path == "/healthz":
            self._send(b'{"ok":true}', "application/json")
        elif u.path in ("/", "/index.html"):
            self._send(page().encode(), "text/html; charset=utf-8")
        else:
            self.send_error(404)

    def log_message(self, *args):
        pass


if __name__ == "__main__":
    print(f"movdb-Frontend auf http://{BIND}:{PORT} "
          f"(exe={EXE}, bin={BIN}, meta={META})", flush=True)
    ThreadingHTTPServer((BIND, PORT), Handler).serve_forever()
