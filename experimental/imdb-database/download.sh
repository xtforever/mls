#!/usr/bin/env bash
# Lädt die IMDb-Daily-Datasets (title.basics, title.akas) herunter und baut
# daraus ein schmales Such-Index im Format:
#
#   titleId<TAB>startYear<TAB>type<TAB>Titel
#
#   - basics: primaryTitle + originalTitle, nicht-Adult, gewaehlte titleTypes
#   - akas:   deutschsprachige Titel
#             (region DE/DEAT/AT/CH oder language == de), mit Jahr und
#             Typ des basics-Eintrags (aka-Zeilen tragen beides nicht)
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
    years="$(mktemp)"
    akas="$(mktemp)"
    trap 'rm -f "$tmp" "$years" "$akas"' EXIT

    # titleId -> startYear + Titeltyp aus basics (akas tragen beides nicht).
    zcat "$DIR/title.basics.tsv.gz" | LC_ALL=C awk -F'\t' '
        function tcode(t) {
            if (t == "movie") return 1;
            if (t == "tvMovie") return 2;
            if (t == "tvSeries") return 3;
            if (t == "miniSeries" || t == "tvMiniSeries") return 4;
            if (t == "tvShort") return 5;
            if (t == "tvSpecial") return 6;
            if (t == "short") return 7;
            if (t == "video") return 8;
            if (t == "special") return 9;
            return 0;
        }
        NR > 1 && $1 != "" {
            print $1 "\t" ($6 == "\\N" ? "" : $6) "\t" tcode($2)
        }' | LC_ALL=C sort -u > "$years"

    # deutschsprachige aka-Titel, nach titleId sortiert (Join-Schluessel).
    zcat "$DIR/title.akas.tsv.gz" | LC_ALL=C awk -F'\t' '
        NR > 1 && $3 != "\\N" &&
        ($5 == "de" || $4 == "DE" || $4 == "DEAT" ||
         $4 == "AT" || $4 == "CH") {
            print $1 "\t" $3
        }' | LC_ALL=C sort -u > "$akas"

    {
        zcat "$DIR/title.basics.tsv.gz" | LC_ALL=C awk -F'\t' '
            function tcode(t) {
                if (t == "movie") return 1;
                if (t == "tvMovie") return 2;
                if (t == "tvSeries") return 3;
                if (t == "miniSeries" || t == "tvMiniSeries") return 4;
                if (t == "tvShort") return 5;
                if (t == "tvSpecial") return 6;
                if (t == "short") return 7;
                if (t == "video") return 8;
                if (t == "special") return 9;
                return 0;
            }
            NR > 1 && $5 != "1" &&
            $2 ~ /^(movie|tvMovie|tvSeries|miniSeries|tvMiniSeries|tvShort|tvSpecial|short|video|special)$/ {
                y = ($6 == "\\N" ? "" : $6)
                ty = tcode($2)
                if ($3 != "\\N")                          print $1 "\t" y "\t" ty "\t" $3
                if ($4 != "\\N" && $4 != $3)              print $1 "\t" y "\t" ty "\t" $4
            }'
        # akas mit basics-Jahr und -Typ anreichern (inner join auf titleId).
        LC_ALL=C join -t $'\t' "$years" "$akas"
    } | LC_ALL=C sort -u > "$tmp"

    mv "$tmp" "$index"
    rm -f "$years" "$akas"
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
