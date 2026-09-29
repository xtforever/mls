# movdb.exed + Web-Frontend.
# Build-Stage: movdb ist standalone (nur POSIX, kein MLS).
FROM gcc:13 AS build
WORKDIR /src
COPY movdb.c imdb_db.c imdb_db.h imdb_bin.h ./
RUN gcc -O2 -std=c11 -D_GNU_SOURCE -Wall -Wextra \
        -o movdb.exed movdb.c imdb_db.c -lm

# Runtime: Python-Stdlib-Frontend (web/server.py).
FROM python:3.12-slim
WORKDIR /app
RUN useradd -r -u 10001 app
COPY --from=build /src/movdb.exed /app/movdb.exed
COPY web/server.py /app/server.py
ENV MOVDB_EXE=/app/movdb.exed \
    MOVDB_BIN=/data/movies.bin \
    MOVDB_META=/data/movies-meta.bin \
    PORT=8000 \
    BIND=0.0.0.0
USER app
EXPOSE 8000
HEALTHCHECK --interval=30s --timeout=3s --start-period=5s \
    CMD python3 -c "import urllib.request;urllib.request.urlopen('http://127.0.0.1:8000/healthz')" || exit 1
CMD ["python3", "/app/server.py"]
