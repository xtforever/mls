# movdb.exed + Web-Frontend mit Wartungs-Jobs.
# Build-Stage: alle C-Tools standalone (nur POSIX, kein MLS).
FROM gcc:13 AS build
WORKDIR /src
COPY movdb.c guess.c imdb_build.c imdb_db.c imdb_db.h imdb_bin.h ./
RUN set -eux; \
    CFLAGS="-O2 -std=c11 -D_GNU_SOURCE -Wall -Wextra"; \
    gcc $CFLAGS -o movdb.exed movdb.c imdb_db.c -lm; \
    gcc $CFLAGS -o guess.exed guess.c imdb_db.c -lm; \
    gcc $CFLAGS -o imdb_build.exed imdb_build.c imdb_db.c -lm

# Runtime: Python-Stdlib-Frontend; curl/zcat/awk fuer download.sh.
FROM python:3.12-slim
RUN apt-get update \
 && apt-get install -y --no-install-recommends curl gzip gawk \
 && rm -rf /var/lib/apt/lists/* \
 && useradd -r -u 10001 app
WORKDIR /app
COPY --from=build /src/movdb.exed /src/guess.exed /src/imdb_build.exed /app/
COPY web/server.py /app/server.py
ENV PATH=/app:$PATH \
    MOVDB_EXE=/app/movdb.exed \
    MOVDB_GUESS_EXE=/app/guess.exed \
    MOVDB_IMDB_BUILD_EXE=/app/imdb_build.exed \
    MOVDB_BIN=/data/movies.bin \
    MOVDB_META=/data/movies-meta.bin \
    MOVDB_DATA=/data \
    PORT=8000 \
    BIND=0.0.0.0
USER app
EXPOSE 8000
HEALTHCHECK --interval=30s --timeout=3s --start-period=5s \
    CMD python3 -c "import urllib.request;urllib.request.urlopen('http://127.0.0.1:8000/healthz')" || exit 1
CMD ["python3", "/app/server.py"]
