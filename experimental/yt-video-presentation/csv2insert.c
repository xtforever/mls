/* csv2insert — CSV von stdin in SQL-INSERT-Zeilen.
 *
 *     printf '1, alice, 3, x, y\n' | ./csv2insert
 *     insert into datax ( id,name,cnt,ref,verify ) values
 * ('1','alice','3','x','y' );
 *
 * Bauen (Single-Header/Amalgam, kein Threading):
 *     cp ../../mls_full.h .
 *     gcc -std=c11 -DMLS_THREAD_SAFE=0 csv2insert.c -o csv2insert -lm -ldl
 *
 * ponytail: naives Split auf ',' — keine Quoted-Felder / Escaped-Kommas.
 * Leere Zeilen fallen über die Feldzahl-Prüfung (NCOLS != 1) weg. Für
 * Ein-Spalten-Tabellen bräuchte es einen zusätzlichen Blank-Guard.
 */
#define MLS_FULL_IMPLEMENTATION
#include "mls_full.h"

#include <stdio.h>

/* Spaltenliste: single source of truth für Header und Feldprüfung. */
static const char *cols[] = {"id", "name", "cnt", "ref", "verify"};
#define NCOLS ((int)ALEN (cols))

void csv2insert (void)
{
	tmls vset = vs_add (0, "col_names", cols, NCOLS);

	/* Header einmal bauen: $col_names[*] expandiert die unquotierte Liste.
	 */
	tmls buf = vs_string_builder (
		0, vset, "insert into datax ( $col_names[*] ) values (");

	int len = s_strlen (buf); /* Checkpoint: nur der Header */

	for (;;) {
		/* eine Zeile stdin -> Variable "fields"; 0 Werte == EOF */
		int fields = vs_read_fields (vset, "fields", stdin, ",");
		if (vs_klen (fields) == 0)
			break;

		if (vs_klen (fields) != NCOLS)
			continue; /* Formatfehler: still überspringen */

		m_setlen (buf, len); /* zurück auf den Header */
		vs_sql_string_builder (buf, vset, "$'fields[*]' );");
		s_puts (buf);
	}
}

int main (void)
{
	m_init ();
	csv2insert ();
	m_destruct ();
	return 0;
}
