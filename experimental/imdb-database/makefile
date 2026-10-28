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
	# "1883" ist ein 1800er-Jahr: raus aus den Query-Tokens, Titel-Treffer.
	grep -q '^1.00  1883' /tmp/imdb-guess.out
	# Umlautfrei an der ID festgemacht, damit der Treffer nicht bloss
	# ueber den Pfad ("Das 10te ...") durchrutscht.
	grep -q '10te.*tt9000009' /tmp/imdb-guess.out
	grep -q 'Call the Midwife' /tmp/imdb-guess.out
	grep -q 'Mord im Mittsommer' /tmp/imdb-guess.out
	grep -q 'SOKO Leipzig' /tmp/imdb-guess.out
	# Ordnungszahl "6th" bleibt erhalten (nicht zu "th" zerlegt); der
	# Film (Typ 1, 2000) schlaegt die gleichnamigen Kurzfilme (Typ 7).
	grep -q 'The 6th Day (2000) tt9000020' /tmp/imdb-guess.out
	# Serienname im Dateinamen "<Name>.<SxxExx>": Serientitel gewinnt
	# gegen gleichnamigen Kurztitel, Abkuerzungsordner wird ignoriert.
	grep -q 'Buffy the Vampire Slayer (1997)' /tmp/imdb-guess.out
	# Schreibfehler im letzten Namensteil ("World" vs "Worlds") in den
	# Verzeichnissen: Einfuegung findet den Serientitel.
	grep -q 'Star Trek: Strange New Worlds (2022)' /tmp/imdb-guess.out
	# Ein Sammlungsordner ("...Collection...") darf nicht als Titel
	# durchschlagen; der Dateiname (Cartouche) ist massgeblich.
	! grep -q 'Belmondo (2003)' /tmp/imdb-guess.out
	# Ein reiner Jahres-Ordner (".../2002/...") ist kein Titel: der
	# gleichnamige Film "2002" darf den echten Filmtitel nicht
	# verdraengen.
	! grep -q '(2001) tt9000022' /tmp/imdb-guess.out
	grep -q 'No Mans Land (2014) tt9000023' /tmp/imdb-guess.out
	# "German" ist Release-Tag, kann aber Teil des Titels sein:
	# "The German S01E04" -> "The German", nicht der Release-Group-Name.
	grep -q 'The German (2025) tt9000024' /tmp/imdb-guess.out
	# Zwei Titelvarianten "A - B": B wird als Fallback-Kandidat gesucht,
	# nicht nur der Prefix A ("Ein Tolpatsch ... - Der Hornochse ...").
	grep -q 'Der Hornochse und sein Zugpferd (1981) tt9000025' /tmp/imdb-guess.out
	# Gleiche Tokens, andere Wortstellung: exakte Reihenfolge gewinnt
	# "Bright Star" (2009) gegen "Star Bright" (1944).
	grep -q 'Bright Star (2009) tt9000028' /tmp/imdb-guess.out
	# "serien"-Ordner: Episodentitel im Dateinamen verliert gegen die
	# Serie im Elternverzeichnis ("Die Märchenbraut", nicht Pulp Fiction).
	grep -q 'Die Märchenbraut (1980) tt9000029' /tmp/imdb-guess.out
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
