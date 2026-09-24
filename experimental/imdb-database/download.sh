#!/usr/bin/env bash
# Lädt die IMDb-Daily-Datasets (title.basics, title.akas) herunter und baut
# daraus ein schmales Such-Index im Format:
#
#   titleId<TAB>startYear<TAB>Titel
#
#   - basics: primaryTitle + originalTitle, nicht-Adult, gewaehlte titleTypes
#   - akas:   deutschsprachige Titel
#             (region DE/DEAT/AT/CH oder language == de)
#
# Aufruf:
#   ./download.sh            # laedt (ueberspringt vorhandene Dateien) + baut Index
#   ./download.sh --force    # neu laden
#   IMDB_BASE_URL=...        # Mirror/lokale Basis-URL (z.B. fuer Tests)
set -euo pipefail

BASE_URL="${IMDB_BASE_URL:-https://datasets.imdbws.com}"
DIR="$(cd "$(dirname "$0")" && pwd)"
FORCE=0
[[ "${1:-}" == "--force" ]] && FORCE=1

FILES=(title.basics.tsv.gz title.akas.tsv.gz)

for f in "${FILES[@]}"; do
    out="$DIR/$f"
    if [[ -s "$out" && $FORCE -eq 0 ]]; then
        echo "[skip] $f existiert bereits"
        continue
    fi
    echo "[get ] $BASE_URL/$f"
    curl -fsSL --retry 3 --connect-timeout 30 -o "$out.part" "$BASE_URL/$f"
    mv "$out.part" "$out"
done

# Idempotenz: Index nur neu bauen, wenn er aelterer als die Quelldaten ist.
index="$DIR/imdb_index.tsv"
if [[ -s "$index" && "$index" -nt "$DIR/title.basics.tsv.gz" \
      && "$index" -nt "$DIR/title.akas.tsv.gz" ]]; then
    echo "[skip] $index ist aktuell"
else
    tmp="$(mktemp)"
    trap 'rm -f "$tmp"' EXIT
    {
        zcat "$DIR/title.basics.tsv.gz" | awk -F'\t' '
            NR > 1 && $5 != "1" &&
            $2 ~ /^(movie|tvMovie|tvSeries|miniSeries|tvMiniSeries|tvShort|tvSpecial|short|video|special)$/ {
                y = ($6 == "\\N" ? "" : $6)
                if ($3 != "\\N")                          print $1 "\t" y "\t" $3
                if ($4 != "\\N" && $4 != $3)              print $1 "\t" y "\t" $4
            }'
        zcat "$DIR/title.akas.tsv.gz" | awk -F'\t' '
            NR > 1 && $3 != "\\N" &&
            ($5 == "de" || $4 == "DE" || $4 == "DEAT" ||
             $4 == "AT" || $4 == "CH") {
                print $1 "\t\t" $3
            }'
    } | sort -u > "$tmp"

    mv "$tmp" "$index"
    trap - EXIT
    echo "[done] $index: $(wc -l < "$index") Zeilen"
fi

# Binaerindex nur neu bauen, wenn er fehlt oder aelter als der TSV-Index ist.
bin="$DIR/imdb_index.bin"
build="$DIR/imdb_build.exed"
if [[ -x "$build" ]]; then
    if [[ ! -s "$bin" || "$index" -nt "$bin" ]]; then
        echo "[bin ] $index -> $bin"
        "$build" "$index" "$bin"
    else
        echo "[skip] $bin ist aktuell"
    fi
else
    echo "[warn] $build fehlt (make imdb_build.exed); ueberspringe Binaerindex" >&2
fi
