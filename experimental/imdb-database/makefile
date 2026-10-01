thread_safe=1
production=0

include ../../rules.mk

TARGET=guess.exed
BUILD=imdb_build.exed
MOVDB=movdb.exed

ALL: $(TARGET) $(BUILD) $(MOVDB)

$(TARGET): guess.od imdb_db.od
$(BUILD): imdb_build.od imdb_db.od
$(MOVDB): movdb.od imdb_db.od

# Header-Aenderungen sollen einen Rebuild ausloesen.
guess.od imdb_db.od imdb_build.od: imdb_db.h imdb_bin.h
movdb.od: imdb_db.h imdb_bin.h

# Selfcheck: Mini-Bin aus testdata/mini-imdb.tsv bauen und guess.exed
# dagegen laufen lassen. Faellt, falls Build, Titel-Extraktion oder Suche
# kaputt gehen.
check: $(TARGET) $(BUILD) $(MOVDB)
	./$(BUILD) testdata/mini-imdb.tsv /tmp/imdb-mini.bin
	./$(TARGET) /tmp/imdb-mini.bin testdata/mini-movies.txt > /tmp/imdb-guess.out 2> /tmp/imdb-guess.err
	grep -q 'This Is Going to Hurt' /tmp/imdb-guess.out
	grep -q 'Kopfgeldjaeger des Teufels\|Kopfgeldjäger des Teufels' /tmp/imdb-guess.out
	grep -q 'Life on Mars' /tmp/imdb-guess.out
	grep -q '^0.50  1883' /tmp/imdb-guess.out
	grep -q 'Das 10te' /tmp/imdb-guess.out
	grep -q 'Call the Midwife' /tmp/imdb-guess.out
	grep -q 'Mord im Mittsommer' /tmp/imdb-guess.out
	grep -q 'SOKO Leipzig' /tmp/imdb-guess.out
	# Ein Sammlungsordner ("...Collection...") darf nicht als Titel
	# durchschlagen; der Dateiname (Cartouche) ist massgeblich.
	! grep -q 'Belmondo (2003)' /tmp/imdb-guess.out
	# movdb: Roundtrip build -> query ueber den Mini-Index (TSV-Ausgabe).
	./$(TARGET) /tmp/imdb-mini.bin testdata/mini-movies.txt --tsv > /tmp/imdb-mini-guess.tsv 2> /dev/null
	./$(MOVDB) build /tmp/imdb-mini-guess.tsv /tmp/imdb-mini-movdb.bin
	./$(MOVDB) query /tmp/imdb-mini-movdb.bin 1883 | grep -q '^1883'
	./$(MOVDB) query /tmp/imdb-mini-movdb.bin mord mittsommer | grep -q 'Mord im Mittsommer'
	# movdb meta: Zusatzinfo per tt-ID joinen (separates mmap-File).
	./$(MOVDB) meta /tmp/imdb-mini-guess.tsv /tmp/imdb-mini-meta.bin --ratings testdata/mini-ratings.tsv --basics testdata/mini-basics.tsv
	./$(MOVDB) get /tmp/imdb-mini-meta.bin tt9000004 | grep -q '7.5'
	./$(MOVDB) get /tmp/imdb-mini-meta.bin tt9000004 | grep -q 'Drama,Western'
	./$(MOVDB) query --meta /tmp/imdb-mini-meta.bin /tmp/imdb-mini-movdb.bin 1883 | grep -q 'Drama,Western'
	# movdb query: Meta-Filter (Genre/Rating) ueber das Meta-File.
	./$(MOVDB) query --meta /tmp/imdb-mini-meta.bin --genre western /tmp/imdb-mini-movdb.bin | grep -q '^1883'
	./$(MOVDB) query --meta /tmp/imdb-mini-meta.bin --min-rating 7.0 /tmp/imdb-mini-movdb.bin | grep -q '^1883'
	! ./$(MOVDB) query --meta /tmp/imdb-mini-meta.bin --genre western /tmp/imdb-mini-movdb.bin | grep -q 'Mord im Mittsommer'
	./$(MOVDB) query --meta /tmp/imdb-mini-meta.bin --year-range 2021-2023 /tmp/imdb-mini-movdb.bin | grep -q '^1883'
	! ./$(MOVDB) query --meta /tmp/imdb-mini-meta.bin --year-range 2021-2023 /tmp/imdb-mini-movdb.bin | grep -q 'Mord im Mittsommer'
	# web/server.py: Mehrfach-Genre + tolerante UTF-8-Dekodierung.
	python3 web/test_server.py

# Vollen Index bauen (laedt Datasets ueber download.sh) und Binaerindex
# erzeugen.
index: $(BUILD)
	./download.sh

clean:
	-${RM} -f *.exed *.od
