#!/usr/bin/env bash
# Kompletter Durchlauf: IMDb-Datasets laden, Binaerindex bauen, Titel raten.
#
#   ./start.sh                  # movies-db, sequenziell
#   ./start.sh -j 6             # parallel (globaler Prefix-Schnitt + Chunks)
#   ./start.sh --force          # Datasets neu laden
#   ./start.sh --no-download    # nur bauen + raten (Index bereits vorhanden)
#   ./start.sh DATEI            # andere Eingabeliste
#
# Env: IMDB_INDEX=<pfad>        # abweichender Binaerindex
set -euo pipefail
cd "$(dirname "$0")"

MOVIES=movies-db
JOBS=1
FORCE=""
DOWNLOAD=1
while [[ $# -gt 0 ]]; do
	case "$1" in
	-j | --jobs)
		JOBS="${2:?--jobs braucht eine Zahl}"
		shift 2
		;;
	--force)
		FORCE="--force"
		shift
		;;
	--no-download)
		DOWNLOAD=0
		shift
		;;
	-h | --help)
		sed -n '2,11p' "$0"
		exit 0
		;;
	-*)
		echo "unbekannte Option: $1" >&2
		exit 1
		;;
	*)
		MOVIES="$1"
		shift
		;;
	esac
done

IDX="${IMDB_INDEX:-imdb_index.bin}"

echo "== build =="
make

if [[ "$DOWNLOAD" -eq 1 ]]; then
	echo "== download + index =="
	./download.sh $FORCE
	IDX="${IMDB_INDEX:-imdb_index.bin}"
fi

echo "== guess ($MOVIES) =="
if [[ "$JOBS" -gt 1 ]]; then
	tmp="$(mktemp -d)"
	trap 'rm -rf "$tmp"' EXIT
	# Globaler Prefix-Schnitt einmal, dann Chunks parallel suchen.
	./guess.exed "$IDX" "$MOVIES" --strip-only >"$tmp/paths"
	split -n l/"$JOBS" -d "$tmp/paths" "$tmp/chunk-"
	pids=()
	for f in "$tmp"/chunk-*; do
		./guess.exed "$IDX" "$f" --strip-prefix 0 --tsv >"$f.tsv" &
		pids+=("$!")
	done
	for p in "${pids[@]}"; do wait "$p"; done
	{
		head -1 "$tmp"/chunk-00.tsv
		for f in "$tmp"/chunk-*.tsv; do tail -n +2 "$f"; done
	} >movies-guess.tsv
else
	./guess.exed "$IDX" "$MOVIES" --tsv >movies-guess.tsv
fi
echo "-> movies-guess.tsv ($(wc -l <movies-guess.tsv) Zeilen)"

# Zusatzinfo je Film (Rating, Stimmen, Genres, Laufzeit, Typ) aus den
# IMDb-Datasets, per tt-ID verknuepft, in ein separates mmap-File.
if [[ -x movdb.exed ]]; then
	meta_args=()
	[[ -s title.ratings.tsv.gz ]] && meta_args+=(--ratings title.ratings.tsv.gz)
	[[ -s title.basics.tsv.gz ]] && meta_args+=(--basics title.basics.tsv.gz)
	if ((${#meta_args[@]})); then
		echo "== meta =="
		./movdb.exed meta movies-guess.tsv movies-meta.bin "${meta_args[@]}"
		echo "-> movies-meta.bin ($(wc -c <movies-meta.bin) Bytes)"
	else
		echo "[warn] title.ratings/basics fehlen, ueberspringe Meta" >&2
	fi
fi
