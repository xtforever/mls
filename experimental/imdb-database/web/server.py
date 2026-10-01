#!/usr/bin/env python3
"""Kleines Web-Frontend fuer movdb.exed: Titelsuche + Wartungs-Tab.

Nur Python-Standardbibliothek. Ruft die C-Tools als Subprozess mit
Argumentliste auf (kein Shell). Konfiguration per Umgebungsvariablen:

  MOVDB_EXE    Pfad zu movdb.exed        (Default ./movdb.exed)
  MOVDB_BIN    Pfad zur movies.bin       (Default movies.bin)
  MOVDB_META   Pfad zur movies-meta.bin  (Default movies-meta.bin)
  MOVDB_DATA   Datenverzeichnis          (Default: Verzeichnis von MOVDB_BIN)
  MOVDB_GUESS_EXE      Pfad zu guess.exed       (Default /app/guess.exed)
  MOVDB_IMDB_BUILD_EXE Pfad zu imdb_build.exed  (Default /app/imdb_build.exed)
  MOVDB_MAINTENANCE    1/0, Wartungs-Tab     (Default 1)
  PORT         HTTP-Port                 (Default 8000)
  BIND         Bind-Adresse              (Default 0.0.0.0)

Wartungs-Jobs sind eine feste Allowlist (kein Nutzer-Input in den
Kommandos), laufen einzeln (globales Lock) in einem Hintergrund-Thread und
schreiben ihre Ausgabe in den Job-Log. Binaerdateien werden atomar per
tmp+rename ersetzt, damit laufende Suchanfragen (mmap) nicht stoeren.
"""
import json
import os
import subprocess
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse

EXE = os.environ.get("MOVDB_EXE", "./movdb.exed")
BIN = os.environ.get("MOVDB_BIN", "movies.bin")
META = os.environ.get("MOVDB_META", "movies-meta.bin")
DATA = os.environ.get("MOVDB_DATA") or os.path.dirname(os.path.abspath(BIN))
GUESS_EXE = os.environ.get("MOVDB_GUESS_EXE", "/app/guess.exed")
BUILD_EXE = os.environ.get("MOVDB_IMDB_BUILD_EXE", "/app/imdb_build.exed")
MAINT = os.environ.get("MOVDB_MAINTENANCE", "1").lower() not in (
    "0", "false", "no", "off", "")
PORT = int(os.environ.get("PORT", "8000"))
BIND = os.environ.get("BIND", "0.0.0.0")

# Pfade im Datenverzeichnis (vom Container read-write gemountet).
DOWNLOAD = os.path.join(DATA, "download.sh")
IMDB_INDEX = os.path.join(DATA, "imdb_index.bin")
MOVIES_DB = os.path.join(DATA, "movies-db")
MOVIES_GUESS = os.path.join(DATA, "movies-guess.tsv")
MOVIES_BIN = os.path.join(DATA, "movies.bin")
MOVIES_META = os.path.join(DATA, "movies-meta.bin")
RATINGS = os.path.join(DATA, "title.ratings.tsv.gz")
BASICS = os.path.join(DATA, "title.basics.tsv.gz")

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


# ---------------------------------------------------------------- Suche
def _one(params, key, default=""):
    v = params.get(key)
    return v[0] if v else default


def search(params):
    """Baut die movdb-Argumentliste und liefert das Ergebnis als dict."""
    argv = [EXE, "query", "--meta", META]
    genres = [g.strip() for g in params.get("genre", []) if g.strip()]
    min_rating = _one(params, "min_rating").strip()
    year_range = _one(params, "year_range").strip()
    typ = _one(params, "type").strip()
    terms = _one(params, "q").split()
    for g in genres:               # mehrere --genre = alle muessen passen
        argv += ["--genre", g]
    if min_rating:
        argv += ["--min-rating", min_rating]
    if year_range:
        argv += ["--year-range", year_range]
    if typ:
        argv += ["--type", typ]
    if not terms and not (genres or min_rating or year_range or typ):
        return {"count": 0, "rows": [],
                "note": "Bitte Suchbegriff oder Filter angeben."}
    argv.append(BIN)          # Datei vor den Termen -> Terme nie als Flag
    argv += terms
    try:
        p = subprocess.run(argv, capture_output=True, timeout=60)
    except (OSError, subprocess.TimeoutExpired) as e:
        return {"error": str(e)}
    # Pfade koennen ungueltiges UTF-8 enthalten -> tolerant dekodieren.
    out = p.stdout.decode("utf-8", "replace")
    err = p.stderr.decode("utf-8", "replace")
    if p.returncode not in (0, 1):
        return {"error": (err or f"rc={p.returncode}").strip()}
    rows = []
    for line in out.splitlines():
        f = line.split("\t")
        if len(f) >= len(COLS):
            row = dict(zip(COLS, f[:len(COLS)]))
            tid = row["id"]
            row["imdb"] = (f"https://www.imdb.com/de/title/{tid}/"
                           if tid.startswith("tt") and tid[2:].isdigit()
                           else "")
            rows.append(row)
    return {"count": len(rows), "rows": rows}


# ------------------------------------------------------------ Wartung
def _steps(action, force):
    """Feste Kommandolisten je Aktion. None = unbekannt.

    Ein Schritt ist [argv...] (Ausgabe -> Log) oder
    ([argv...], datei) (stdout -> datei). Binaerdateien werden nach .tmp
    geschrieben und per mv ersetzt (atomar fuer laufende mmap-Leser).
    """
    dl = [DOWNLOAD] + (["--force"] if force else [])
    guess = ([GUESS_EXE, IMDB_INDEX, MOVIES_DB, "--tsv"], MOVIES_GUESS + ".tmp")
    mv_guess = ["mv", "-f", MOVIES_GUESS + ".tmp", MOVIES_GUESS]
    build = [EXE, "build", MOVIES_GUESS, MOVIES_BIN + ".tmp"]
    mv_build = ["mv", "-f", MOVIES_BIN + ".tmp", MOVIES_BIN]
    meta = [EXE, "meta", MOVIES_GUESS, MOVIES_META + ".tmp",
            "--ratings", RATINGS, "--basics", BASICS]
    mv_meta = ["mv", "-f", MOVIES_META + ".tmp", MOVIES_META]
    db = [build, mv_build, meta, mv_meta]
    if action == "update-imdb":
        return [dl]
    if action == "rebuild-db":
        return db
    if action == "reindex":
        return [guess, mv_guess] + db
    if action == "rebuild":
        return [dl, guess, mv_guess] + db
    return None


class Job:
    def __init__(self):
        self.lock = threading.Lock()
        self.status = "idle"       # idle|running|done|error
        self.name = None
        self.rc = None
        self.started = None
        self.ended = None
        self.log = ""

    def start(self, action, force):
        steps = _steps(action, force)
        if steps is None:
            return False, f"unbekannte Aktion '{action}'"
        with self.lock:
            if self.status == "running":
                return False, "es laeuft bereits ein Job"
            self.status = "running"
            self.name = action
            self.rc = None
            self.log = ""
            self.started = time.time()
            self.ended = None
            threading.Thread(target=self._run, args=(steps,),
                             daemon=True).start()
        return True, None

    def _log(self, text):
        with self.lock:
            self.log += text
            if len(self.log) > 65536:      # Log begrenzen
                self.log = self.log[-65536:]

    def _run(self, steps):
        rc = 0
        for step in steps:
            outpath = None
            if isinstance(step, tuple):
                argv, outpath = step
            else:
                argv = step
            self._log("$ " + " ".join(argv)
                      + (f" > {outpath}" if outpath else "") + "\n")
            try:
                if outpath:
                    with open(outpath, "wb") as fo:
                        p = subprocess.run(argv, stdout=fo,
                                           stderr=subprocess.PIPE,
                                           timeout=7200)
                else:
                    p = subprocess.run(argv, stdout=subprocess.PIPE,
                                       stderr=subprocess.STDOUT, timeout=7200)
            except (OSError, subprocess.TimeoutExpired) as e:
                self._log(f"FEHLER: {e}\n")
                rc = 1
                break
            if outpath:
                if p.stderr:
                    self._log(p.stderr.decode("utf-8", "replace"))
            else:
                self._log(p.stdout.decode("utf-8", "replace"))
            if p.returncode != 0:
                rc = p.returncode
                self._log(f"-> rc={rc}\n")
                break
        with self.lock:
            self.status = "done" if rc == 0 else "error"
            self.rc = rc
            self.ended = time.time()

    def snapshot(self):
        with self.lock:
            return {"status": self.status, "name": self.name, "rc": self.rc,
                    "log": self.log, "started": self.started,
                    "ended": self.ended}


JOB = Job()


# -------------------------------------------------------------- Seite
def page():
    genres = "".join(
        f'<label class="cb"><input type="checkbox" name="genre" '
        f'value="{g}">{g}</label>' for g in GENRES)
    types = "".join(f'<option value="{t}">{t}</option>' for t in TYPES)
    tab = '<button data-tab="maint">Wartung</button>' if MAINT else ""
    pane = MAINT_PANE if MAINT else ""
    return (PAGE.replace("{{GENRES}}", genres)
                .replace("{{TYPES}}", types)
                .replace("{{MAINT_TAB}}", tab)
                .replace("{{MAINT_PANE}}", pane))


MAINT_PANE = """
<section id="tab-maint" hidden>
 <div class="actions">
  <button data-action="update-imdb">IMDb aktualisieren</button>
  <button data-action="reindex">Teil-Rebuild (raten)</button>
  <button data-action="rebuild-db">DB-Rebuild (ohne raten)</button>
  <button data-action="rebuild">Komplett-Rebuild</button>
  <label class="cb"><input type="checkbox" id="force">Datasets neu laden</label>
 </div>
 <div id="mstatus">bereit</div>
 <pre id="mlog"></pre>
</section>"""


PAGE = """<!doctype html>
<html lang="de">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>movdb Suche</title>
<style>
 body{font:14px/1.45 system-ui,sans-serif;margin:0;background:#111;color:#eee}
 header{padding:12px 20px;background:#1b1b1b;border-bottom:1px solid #333}
 h1{font-size:16px;margin:0 0 10px}
 nav.tabs{display:flex;gap:6px;margin-bottom:10px}
 nav.tabs button{background:#222;border:1px solid #444}
 nav.tabs button.active{background:#2d6cdf;border-color:#2d6cdf}
 form{display:flex;flex-wrap:wrap;gap:10px;align-items:flex-end}
 label{display:flex;flex-direction:column;font-size:12px;color:#aaa;gap:4px}
 .field{display:flex;flex-direction:column;gap:4px}
 .cap{font-size:12px;color:#aaa}
 .genrebox{display:grid;grid-template-columns:repeat(auto-fill,minmax(105px,1fr));
   gap:2px 12px;background:#1b1b1b;border:1px solid #444;border-radius:4px;
   padding:6px 8px;max-height:130px;overflow:auto;min-width:260px}
 .cb{flex-direction:row;align-items:center;gap:6px;color:#ccc;font-size:12px}
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
 a.imdb{display:inline-block;margin-left:6px;padding:0 4px;border-radius:3px;
   background:#f5c518;color:#000;font-size:11px;font-weight:700;
   text-decoration:none;vertical-align:middle}
 a.imdb:hover{background:#ffd94a}
 .actions{display:flex;flex-wrap:wrap;gap:8px;align-items:center;margin-bottom:12px}
 #mstatus{color:#aaa;margin-bottom:8px}
 #mlog{background:#0c0c0c;border:1px solid #333;border-radius:4px;padding:10px;
   max-height:55vh;overflow:auto;white-space:pre-wrap;font:12px/1.4 ui-monospace,monospace}
</style>
</head>
<body>
<header>
 <h1>movdb &mdash; Filme suchen</h1>
 <nav class="tabs">
  <button data-tab="search" class="active">Suche</button>{{MAINT_TAB}}
 </nav>
 <form id="f">
  <label>Suche <input name="q" placeholder="mord mittsommer" autofocus></label>
  <div class="field"><span class="cap">Genres (alle gew&auml;hlten)</span>
   <div class="genrebox">{{GENRES}}</div></div>
  <label>Rating ab <input name="min_rating" placeholder="8.0" size="4"></label>
  <label>Jahre <input name="year_range" placeholder="1930-1933" size="10"></label>
  <label>Typ <select name="type"><option value="">(egal)</option>{{TYPES}}</select></label>
  <button>suchen</button>
 </form>
</header>
<main>
 <section id="tab-search">
  <div id="status"></div>
  <table>
   <thead><tr><th>Titel</th><th>Jahr</th><th>ID</th><th>Rating</th>
    <th>Stimmen</th><th>Min</th><th>Genres</th><th>Pfad</th></tr></thead>
   <tbody id="tb"></tbody>
  </table>
 </section>
 {{MAINT_PANE}}
</main>
<script>
const esc=s=>{const d=document.createElement('div');d.textContent=s;return d.innerHTML};
const tabs=[...document.querySelectorAll('nav.tabs button')];
function showTab(id){
 tabs.forEach(b=>b.classList.toggle('active',b.dataset.tab===id));
 document.getElementById('tab-search').hidden = id!=='search';
 const m=document.getElementById('tab-maint'); if(m) m.hidden = id!=='maint';
}
tabs.forEach(b=>b.onclick=()=>showTab(b.dataset.tab));

const f=document.getElementById('f'),tb=document.getElementById('tb'),
      st=document.getElementById('status');
f.addEventListener('submit',async e=>{
 e.preventDefault();
 st.textContent='suche\\u2026'; tb.innerHTML='';
 try{
  const r=await fetch('/api/search?'+new URLSearchParams(new FormData(f)));
  const j=await r.json();
  if(j.error){st.textContent='Fehler: '+j.error;return}
  st.textContent=j.note||(j.count+' Treffer');
  tb.innerHTML=j.rows.map(x=>'<tr>'+
    '<td>'+esc(x.title)+(x.imdb?' <a class="imdb" href="'+esc(x.imdb)+
      '" target="_blank" rel="noopener noreferrer">IMDb</a>':'')+'</td>'+
    ['year','id','rating','votes','runtime','genres']
      .map(k=>'<td>'+esc(x[k])+'</td>').join('')+
    '<td class="path">'+esc(x.path)+'</td></tr>').join('');
 }catch(err){st.textContent='Fehler: '+err}
});

const ms=document.getElementById('mstatus'),ml=document.getElementById('mlog');
async function poll(){
 if(!ms) return;
 try{
  const j=await (await fetch('/api/maintenance')).json();
  ms.textContent = j.status==='running' ? ('l\\u00e4uft: '+j.name+' \\u2026')
    : j.status==='idle' ? 'bereit'
    : j.status+(j.rc!=null?(' (rc='+j.rc+')'):'');
  ml.textContent=j.log||'';
  ml.scrollTop=ml.scrollHeight;
 }catch(e){}
}
if(ms){ poll(); setInterval(poll,1500); }
document.querySelectorAll('[data-action]').forEach(b=>b.onclick=async()=>{
 if(!confirm('Wartungsjob \\u201e'+b.textContent+'\\u201c starten?')) return;
 const force=document.getElementById('force').checked?'?force=1':'';
 const r=await fetch('/api/maintenance/'+b.dataset.action+force,
   {method:'POST',headers:{'X-Movdb':'1'}});
 const j=await r.json();
 if(!j.ok) ms.textContent='Fehler: '+(j.error||r.status);
 poll();
});
</script>
</body>
</html>
"""


# ----------------------------------------------------------- HTTP
class Handler(BaseHTTPRequestHandler):
    def _send(self, body, ctype, code=200):
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        u = urlparse(self.path)
        if u.path == "/api/search":
            self._send(json.dumps(search(parse_qs(u.query)),
                                  ensure_ascii=False).encode(),
                       "application/json; charset=utf-8")
        elif u.path == "/api/maintenance":
            snap = JOB.snapshot()
            snap["enabled"] = MAINT
            self._send(json.dumps(snap).encode(), "application/json")
        elif u.path == "/healthz":
            self._send(b'{"ok":true}', "application/json")
        elif u.path in ("/", "/index.html"):
            self._send(page().encode(), "text/html; charset=utf-8")
        else:
            self.send_error(404)

    def do_POST(self):
        u = urlparse(self.path)
        if not MAINT:
            self._send(b'{"ok":false,"error":"Wartung deaktiviert"}',
                       "application/json", 403)
            return
        # CSRF-Schutz: Browser senden den Custom-Header nicht cross-origin.
        if self.headers.get("X-Movdb") != "1":
            self._send(b'{"ok":false,"error":"fehlender Header"}',
                       "application/json", 403)
            return
        if not u.path.startswith("/api/maintenance/"):
            self.send_error(404)
            return
        action = u.path[len("/api/maintenance/"):]
        force = parse_qs(u.query).get("force", ["0"])[0] not in (
            "0", "", "false", "no")
        ok, err = JOB.start(action, force)
        self._send(json.dumps({"ok": ok, "error": err}).encode(),
                   "application/json", 202 if ok else 409)

    def log_message(self, *args):
        pass


if __name__ == "__main__":
    print(f"movdb-Frontend auf http://{BIND}:{PORT} "
          f"(exe={EXE}, bin={BIN}, meta={META}, data={DATA}, "
          f"wartung={'an' if MAINT else 'aus'})", flush=True)
    ThreadingHTTPServer((BIND, PORT), Handler).serve_forever()
