/* Tests for s_read_fields: read one line, split into separator-delimited
 * string handles (MFREE_EACH list). */
#include "../lib/m_tool.h"
#include "../lib/mls.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static FILE *mem (const char *s)
{
	FILE *fp = tmpfile ();
	assert (fp);
	fputs (s, fp);
	rewind (fp);
	return fp;
}

static const char *fv (int list, int i) { return m_str (INT (list, i)); }

int main (void)
{
	m_init ();
	int f = 0;

	/* multi-char separator, one line, then EOF */
	FILE *fp = mem ("a::b::c\n");
	f = s_read_fields (f, fp, "::", 0);
	assert (f && m_len (f) == 3);
	assert (strcmp (fv (f, 0), "a") == 0);
	assert (strcmp (fv (f, 1), "b") == 0);
	assert (strcmp (fv (f, 2), "c") == 0);
	assert (m_len (s_read_fields (f, fp, "::", 0)) == 0);
	/* repeated EOF reuses the same handle, still empty (no leak) */
	assert (s_read_fields (f, fp, "::", 0) == f);
	fclose (fp);

	/* dest reuse + trim, two lines */
	fp = mem (" 1 , alice , 3 \n 4 , bob , 6 \n");
	f = s_read_fields (f, fp, ",", 1);
	assert (m_len (f) == 3);
	assert (strcmp (fv (f, 0), "1") == 0);
	assert (strcmp (fv (f, 1), "alice") == 0);
	assert (strcmp (fv (f, 2), "3") == 0);
	f = s_read_fields (f, fp, ",", 1);
	assert (m_len (f) == 3);
	assert (strcmp (fv (f, 0), "4") == 0);
	assert (strcmp (fv (f, 1), "bob") == 0);
	assert (strcmp (fv (f, 2), "6") == 0);
	assert (m_len (s_read_fields (f, fp, ",", 1)) == 0);
	fclose (fp);

	/* no trim keeps spaces; trailing separator -> empty last field */
	fp = mem ("x; y;\n");
	f = s_read_fields (f, fp, ";", 0);
	assert (m_len (f) == 3);
	assert (strcmp (fv (f, 0), "x") == 0);
	assert (strcmp (fv (f, 1), " y") == 0);
	assert (strcmp (fv (f, 2), "") == 0);
	fclose (fp);

	/* empty separator -> whole line is a single field */
	fp = mem ("  hello, world  \n");
	f = s_read_fields (f, fp, "", 1);
	assert (m_len (f) == 1);
	assert (strcmp (fv (f, 0), "hello, world") == 0);
	fclose (fp);

	/* NULL separator behaves the same */
	fp = mem ("raw\n");
	f = s_read_fields (f, fp, NULL, 0);
	assert (m_len (f) == 1);
	assert (strcmp (fv (f, 0), "raw") == 0);
	fclose (fp);

	/* /etc/passwd style: 7 colon fields, empty GECOS preserved */
	fp = mem ("root:x:0:0:root:/root:/bin/bash\n"
		  "daemon:x:1:1::/usr/sbin:/usr/sbin/nologin\n");
	f = s_read_fields (f, fp, ":", 0);
	assert (m_len (f) == 7);
	assert (strcmp (fv (f, 0), "root") == 0);
	assert (strcmp (fv (f, 4), "root") == 0);
	assert (strcmp (fv (f, 6), "/bin/bash") == 0);
	f = s_read_fields (f, fp, ":", 0);
	assert (m_len (f) == 7);
	assert (strcmp (fv (f, 4), "") == 0); /* empty GECOS kept */
	assert (strcmp (fv (f, 6), "/usr/sbin/nologin") == 0);
	fclose (fp);

	/* /etc/group style: 4 colon fields, trailing empty member list */
	fp = mem ("wheel:x:10:\n");
	f = s_read_fields (f, fp, ":", 0);
	assert (m_len (f) == 4);
	assert (strcmp (fv (f, 0), "wheel") == 0);
	assert (strcmp (fv (f, 3), "") == 0);
	fclose (fp);

	/* NULL fp -> empty list, still a valid handle */
	int none = s_read_fields (0, NULL, ",", 0);
	assert (none > 0 && m_len (none) == 0);
	m_free (none);

	m_free (f);
	m_destruct ();
	puts ("read_fields ok");
	return 0;
}
