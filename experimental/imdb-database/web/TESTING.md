# Test-Anleitung: movdb Play-Agent

Kurz, von „ohne alles" bis „echter Server".

## 1. Automatisch (kein Display, kein Server, kein root)

```bash
cd experimental/imdb-database
python3 web/test_player_agent.py   # Mapping, Allowlist, Origin, On-Demand-Mount
python3 web/test_server.py         # Suche-Argv, UI, PLAYER_URL-Escaping
bash    web/test_e2e.sh            # Suche -> Agent -> sshfs(Fake) -> Player(Fake)
bash    web/test_install_origin.sh # UI-Origin-Autoerkennung im Installer
```

Erwartet: `... : ok` bzw. `PASS: ...`, jeweils Exit 0. `test_e2e.sh` nutzt
Fakes und ein temporaeres `HOME`, raeumt selbst auf.

## 2. Client-Installation isoliert pruefen (veraendert das System nicht)

```bash
T=$(mktemp -d)
HOME=$T bash web/install-client.sh --server user@server \
    --ui-origin http://localhost:8000 --no-service
find "$T" -type f                 # player.env 0600, Launcher/Mount 0755
HOME=$T "$T/.local/bin/movdb-mount" print   # sshfs-Argv, inkl. -o ro
HOME=$T bash web/install-client.sh --uninstall --purge
```

## 3. Echter Praxistest (Server + Desktop)

1. Auf dem Server: `docker compose up -d` → UI unter `http://server:8000`.
2. Auf dem Client: `./web/install-client.sh --server user@server`
   (UI-Origin wird automatisch ermittelt, ggf. `--ui-host`).
3. Suchen, `▶` klicken. Beim ersten Klick mountet der Agent per sshfs
   (Default) und startet `mpv`.

Pruefen: `mountpoint ~/mnt/server`, `ls ~/mnt/server/<db-pfad>` und
`journalctl --user -u movdb-play-agent -n 20`.

## 4. Wenn Play `403` liefert

- **Pfad**: DB-Pfad gegen `movdb-mount print` + `ls ~/mnt/server/<pfad>`.
- **Mount**: `movdb-mount up` manuell. Passwort-SSH funktioniert weder im
  Agenten noch im Dienst (kein TTY) → `--ssh-key` nutzen oder vorher mounten.
- **Origin**: `--ui-origin` muss der Browser-URL entsprechen, sonst
  `403 {"error":"origin"}`.
- **HTTPS-UI**: `http://127.0.0.1` ist dann Mixed Content → Agent nicht
  erreichbar.
