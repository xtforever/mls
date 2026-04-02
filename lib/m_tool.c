#include "m_tool.h"
#include "mls.h"
#include "m_table.h"
#include <errno.h>
#include <fcntl.h>
#include <printf.h>
#include <regex.h>
#include <search.h>
#include <stdarg.h>

/* Internal prototypes */
static int vas_app (int m, va_list ap);
static int field_escape (int s2, char *s, int quotes);
static void repl_char (int buf, char ch);

/* alphabetisch sortierte liste von mstr */
static int CONSTSTR_DATA = 0;
static int CS_ZERO = -1;

/* Relocated Core Utilities */

int m_dub (int m)
{
	int h = m_free_hdl (m);
	int r = m_alloc (m_len (m), m_width (m), h);
	m_write (r, 0, mls (m, 0), m_len (m));
	return r;
}

int m_regex (int m, const char *regex, const char *s)
{
	char *szTemp;
	regex_t regc;
	regmatch_t *pm;
	int i, error;
	int subexp = 1;
	int p = 0;
	error = regcomp (&regc, regex, REG_EXTENDED);
	if (error)
		ERR ("REG_EXPRESSION %s not valid", regex);
	while (regex[p]) {
		if (regex[p] == '(')
			subexp++;
		p++;
	}
	pm = (regmatch_t *)malloc (sizeof (regmatch_t) * subexp);
	if (m > 1)
		m_free_strings (m, 1);
	else
		m = m_alloc (subexp + 1, sizeof (char *), MFREE_STR);
	error = regexec (&regc, s, subexp, pm, 0);
	if (!error) {
		for (i = 0; i < subexp; i++) {
			if (pm[i].rm_so == -1)
				break;
			szTemp = strndup (s + pm[i].rm_so,
					  pm[i].rm_eo - pm[i].rm_so);
			m_put (m, &szTemp);
		}
	}
	free (pm);
	regfree (&regc);
	return m;
}

void m_qsort (int list, int (*compar) (const void *, const void *))
{
	qsort (m_buf (list), m_len (list), m_width (list), compar);
}

int m_bsearch (const void *key, int list,
	       int (*compar) (const void *, const void *))
{
	if (list < 1 || m_len (list) == 0)
		return -1;
	void *res = bsearch (key, m_buf (list), m_len (list), m_width (list),
			     compar);
	if (res)
		return (res - m_buf (list)) / m_width (list);
	return -1;
}

int m_lfind (const void *key, int list,
	     int (*compar) (const void *, const void *))
{
	size_t max;
	if (list < 1 || m_len (list) == 0)
		return -1;
	max = m_len (list);
	void *res = lfind (key, m_buf (list), &max, m_width (list), compar);
	if (res)
		return (res - m_buf (list)) / m_width (list);
	return -1;
}

int m_binsert (int buf, const void *data,
	       int (*cmpf) (const void *data, const void *buf_elem),
	       int with_duplicates)
{
	int left = 0;
	int right = m_len (buf) + 1;
	int cur = 1;
	void *obj;
	int cmp;
	if (m_len (buf) == 0) {
		m_put (buf, data);
		return 0;
	}
	while (1) {
		cur = (left + right) / 2;
		obj = mls (buf, cur - 1);
		cmp = cmpf (data, obj);
		if (cmp == 0) {
			if (!with_duplicates)
				return -cur;
			break;
		}
		if (cmp < 0) {
			right = cur;
			if (left + 1 == right)
				break;
		} else {
			left = cur;
			if (left + 1 == right) {
				cur++;
				break;
			}
		}
	}
	cur--;
	m_ins (buf, cur, 1);
	m_write (buf, cur, data, 1);
	return cur;
}

int ioread_all (int fd, int buffer)
{
	ssize_t total = 0;
	ssize_t n = 0;
	m_clear (buffer);
	do {
		total += n;
		m_setlen (buffer, total + 4096);
		n = read (fd, mls (buffer, total), 4096);
		if (n < 0) {
			if (errno == EINTR) {
				n = 0;
				continue;
			}
			break;
		}
	} while (n > 0);
	m_setlen (buffer, total);
	m_putc (buffer, 0);
	return n;
}

/* String Utilities */

int s_new (void) { return m_alloc (16, 1, MFREE); }

void s_free (int h) { m_free (h); }

int s_strlen (int m)
{
	int p = m_len (m);
	return p && CHAR (m, p - 1) == 0 ? p - 1 : p;
}

int s_app1 (int m, char *s)
{
	int p = s_strlen (m);
	m_write (m, p, s, strlen (s) + 1);
	return m;
}

static int vas_app (int m, va_list ap)
{
	char *name;
	while ((name = va_arg (ap, char *)) != NULL) {
		s_app1 (m, name);
	}
	return m;
}

int s_app (int m, ...)
{
	va_list ap;
	if (m <= 0)
		m = s_new ();
	va_start (ap, m);
	vas_app (m, ap);
	va_end (ap);
	return m;
}

int vas_printf (int m, int p, const char *format, va_list ap)
{
	int len;
	va_list copy1, copy2;
	va_copy (copy1, ap);
	va_copy (copy2, ap);
	len = vsnprintf (0, 0, format, copy1);
	va_end (copy1);
	len++;
	if (m <= 0) {
		m = s_new ();
		p = 0;
	}
	if (p < 0 || p > m_len (m))
		p = s_strlen (m);
	m_setlen (m, p + len);
	void *buf = mls (m, p);
	vsnprintf (buf, len, format, copy2);
	va_end (copy2);
	return m;
}

int s_printf (int m, int p, char *format, ...)
{
	va_list ap;
	va_start (ap, format);
	m = vas_printf (m, p, format, ap);
	va_end (ap);
	return m;
}

int s_dup (const char *s)
{
	if (s == NULL || *s == 0)
		return s_new ();
	int h = s_new ();
	s_app1 (h, (char *)s);
	return h;
}

int s_clone (int h) { return m_dub (h); }

int s_resize (int h, int len)
{
	if (h == 0 || len < 0)
		return h;
	m_setlen (h, len + 1);
	CHAR (h, len) = 0;
	return h;
}

void s_clear (int h) { m_clear (h); }

int s_has_prefix (int h, const char *prefix)
{
	if (h == 0 || prefix == NULL)
		return 0;
	int pre_len = strlen (prefix);
	int h_len = s_strlen (h);
	if (pre_len > h_len)
		return 0;
	return strncmp (m_str (h), prefix, pre_len) == 0;
}

int s_has_suffix (int h, const char *suffix)
{
	if (h == 0 || suffix == NULL)
		return 0;
	int suf_len = strlen (suffix);
	int h_len = s_strlen (h);
	if (suf_len > h_len)
		return 0;
	const char *p = m_str (h) + h_len - suf_len;
	return strcmp (p, suffix) == 0;
}

int s_from_long (long val)
{
	int h = s_new ();
	s_printf (h, 0, "%ld", val);
	return h;
}

uint32_t s_hash (int h)
{
	const char *s = m_str (h);
	uint32_t hash = 5381;
	int c;
	while ((c = *s++))
		hash = ((hash << 5) + hash) + c;
	return hash;
}

int s_join (const char *sep, ...)
{
	int h = s_new ();
	va_list ap;
	char *s;
	int first = 1;

	va_start (ap, sep);
	while ((s = va_arg (ap, char *)) != NULL) {
		if (!first && sep && *sep)
			s_app1 (h, (char *)sep);
		first = 0;
		s_app1 (h, s);
	}
	va_end (ap);
	return h;
}

int s_cmp (int a, int b)
{
	if (a == 0 && b == 0)
		return 0;
	if (a == 0)
		return -1;
	if (b == 0)
		return 1;
	int len_a = s_strlen (a);
	int len_b = s_strlen (b);
	int min_len = len_a < len_b ? len_a : len_b;
	int res = strncmp (m_str (a), m_str (b), min_len);
	if (res != 0)
		return res;
	return len_a - len_b;
}

int s_ncmp (int a, int b, int n)
{
	if (a == 0 || b == 0 || n <= 0)
		return 0;
	const char *s1 = m_str (a);
	const char *s2 = m_str (b);
	return strncmp (s1, s2, n);
}

int s_chr (int h, int c, int off)
{
	if (h == 0 || off < 0)
		return -1;
	const char *s = m_str (h);
	int len = s_strlen (h);
	if (off >= len)
		return -1;
	const char *p = strchr (s + off, c);
	if (p == NULL)
		return -1;
	return p - s;
}

int s_rchr (int h, int c)
{
	if (h == 0)
		return -1;
	const char *s = m_str (h);
	int len = s_strlen (h);
	for (int i = len - 1; i >= 0; i--) {
		if (s[i] == c)
			return i;
	}
	return -1;
}

int s_find (int h, const char *sub)
{
	if (h == 0 || sub == NULL || *sub == 0)
		return -1;
	const char *s = m_str (h);
	const char *p = strstr (s, sub);
	if (p == NULL)
		return -1;
	return p - s;
}

int s_spn (int h, const char *accept)
{
	if (h == 0 || accept == NULL)
		return 0;
	return strspn (m_str (h), accept);
}

int s_cspn (int h, const char *reject)
{
	if (h == 0 || reject == NULL)
		return 0;
	return strcspn (m_str (h), reject);
}

int s_cat (int h, const char *src)
{
	if (h == 0)
		h = s_new ();
	if (src)
		s_app1 (h, (char *)src);
	return h;
}

int s_ncat (int h, const char *src, int n)
{
	if (h == 0)
		h = s_new ();
	if (src && n > 0) {
		size_t src_len = strlen (src);
		int len = n < (int)src_len ? n : (int)src_len;
		m_write (h, s_strlen (h), src, len);
		m_putc (h, 0);
	}
	return h;
}

int s_sub (int h, int pos, int len)
{
	if (h == 0)
		return s_new ();
	int h_len = s_strlen (h);
	if (pos < 0)
		pos = 0;
	if (len < 0 || pos >= h_len)
		return s_new ();
	if (pos + len > h_len)
		len = h_len - pos;

	return s_slice (0, 0, h, pos, pos + len - 1);
}

int s_left (int h, int n) { return s_sub (h, 0, n); }

int s_right (int h, int n)
{
	if (h == 0 || n <= 0)
		return s_new ();
	int h_len = s_strlen (h);
	if (n >= h_len)
		return s_clone (h);
	return s_sub (h, h_len - n, n);
}

int s_replace_c (int h, const char *old, const char *replacement)
{
	if (h == 0 || old == NULL || *old == 0)
		return s_clone (h);
	if (replacement == NULL)
		replacement = "";

	int new_h = s_new ();
	const char *s = m_str (h);
	const char *start = s;
	const char *p;
	int old_len = strlen (old);

	while ((p = strstr (start, old)) != NULL) {
		if (p > start)
			s_ncat (new_h, start, p - start);
		s_cat (new_h, replacement);
		start = p + old_len;
	}
	s_cat (new_h, start);
	return new_h;
}

int s_trim_c (int h, const char *chars)
{
	if (h == 0)
		return s_new ();
	if (chars == NULL || *chars == 0)
		chars = " \t\n\r";

	const char *s = m_str (h);
	int len = s_strlen (h);
	int start = 0;
	int end = len - 1;

	while (start <= end && strchr (chars, s[start]))
		start++;
	while (end >= start && strchr (chars, s[end]))
		end--;

	return s_sub (h, start, end - start + 1);
}

int s_lastchar (int m)
{
	int len = m_len (m);
	if (len == 0)
		return 0;
	do {
		len--;
	} while (len > 0 && CHAR (m, len) == 0);
	return CHAR (m, len);
}

int s_copy (int m, int first_char, int last_char)
{
	if (last_char < 0)
		last_char = m_len (m) - 1;
	if (first_char < 0 || first_char > last_char || first_char >= m_len (m))
		return s_new ();
	int size = last_char - first_char + 1;
	if (first_char + size > m_len (m))
		size = m_len (m) - first_char;
	int ret = s_new ();
	m_setlen (ret, size);
	m_write (ret, 0, mls (m, first_char), size);
	if (CHAR (ret, m_len (ret) - 1) != 0)
		m_putc (ret, 0);
	return ret;
}

int s_index (int buf, int p, int ch)
{
	unsigned char *d;
	while (p < m_len (buf)) {
		d = mls (buf, p);
		if (*d == ch)
			return p;
		p++;
	}
	return -1;
}

int s_split (int m, const char *s, int c, int remove_wspace)
{
	int p = 0, start = 0, end;
	char *szTemp;


	if (m)
		m_free_strings (m, 1);
	else
		m = m_alloc (10, sizeof (char *), MFREE_STR);

	for (;;) {
		if (remove_wspace)
			while (isspace (s[p]) && s[p] != c)
				p++;
		start = p;
		while (s[p] && s[p] != c)
			p++;
		if (remove_wspace) {
			end = p;
			while (end > start && isspace (s[--end]))
				;
			if (end >= start && !isspace (s[end])) {
				szTemp = strndup (s + start, end - start + 1);
			} else
				szTemp = strdup ("");
		} else {
			end = p;
			if (end > start) {
				szTemp = strndup (s + start, end - start);
			} else
				szTemp = strdup ("");
		}
		m_put (m, &szTemp);
		if (s[p])
			p++;
		else
			break;
	}
	return m;
}

int s_slice (int dest, int offs, int m, int a, int b)
{
	int ret = m_slice (dest, offs, m, a, b);
	if (m_len (ret) == 0 || CHAR (ret, m_len (ret) - 1) != 0)
		m_putc (ret, 0);
	return ret;
}

int s_strstr (int m, int offs, int pattern)
{
	if (offs >= m_len (m))
		return -1;
	char *res = strstr (m_str (m) + offs, m_str (pattern));
	if (res)
		return res - m_str (m);
	return -1;
}

int s_replace (int dest, int src, int pattern, int replace, int count)
{
	if (dest == 0)
		dest = s_new ();
	else
		m_clear (dest);
	ASSERT (m_width (dest) == 1);
	if (s_isempty (src) || s_isempty (pattern))
		return dest;
	int offs = 0;
	int replace_last = s_strlen (replace) - 1;
	int pattern_len = s_strlen (pattern);
	for (;;) {
		int pos = s_strstr (src, offs, pattern);
		if (pos < 0)
			break;
		m_slice (dest, m_len (dest), src, offs, pos - 1);
		m_slice (dest, m_len (dest), replace, 0, replace_last);
		offs = pos + pattern_len;
		if (--count == 0)
			break;
	}
	m_slice (dest, m_len (dest), src, offs, -1);
	return dest;
}

int s_strncmp (int m, int offs, int pattern, int n)
{
	if (offs >= m_len (m))
		return 1;
	return strncmp (m_str (m) + offs, m_str (pattern), n);
}

int s_isempty (int m)
{
	if (m <= 0 || m_len (m) == 0)
		return 1;
	return (CHAR (m, 0) == 0);
}

int s_trim (int m)
{
	int p = 0;
	if (s_isempty (m))
		return m;
	while (p < m_len (m) && isspace (CHAR (m, p)))
		p++;
	if (p > 0)
		m_remove (m, 0, p);
	p = m_len (m) - 1;
	while (p >= 0 && (CHAR (m, p) == 0 || isspace (CHAR (m, p))))
		p--;
	m_setlen (m, p + 1);
	m_putc (m, 0);
	m_setlen (m, p + 1);
	return m;
}

int s_lower (int m)
{
	if (s_isempty (m))
		return m;
	int p;
	char *d;
	m_foreach (m, p, d) *d = tolower (*d);
	return m;
}

int s_upper (int m)
{
	if (s_isempty (m))
		return m;
	int p;
	char *d;
	m_foreach (m, p, d) *d = toupper (*d);
	return m;
}

int s_msplit (int dest, int src, int pattern)
{
	if (dest == 0)
		dest = m_create (10, sizeof (int));
	else
		m_clear (dest);
	int offs = 0;
	int plen = s_strlen (pattern);
	for (;;) {
		int pos = s_strstr (src, offs, pattern);
		if (pos < 0)
			break;
		m_puti (dest, m_slice (0, 0, src, offs, pos - 1));
		offs = pos + plen;
	}
	m_puti (dest, m_slice (0, 0, src, offs, -1));
	return dest;
}

int s_implode (int dest, int srcs, int seperator)
{
	if (dest == 0)
		dest = s_new ();
	else
		m_clear (dest);
	if (srcs <= 0 || m_len (srcs) == 0 || s_isempty (seperator))
		goto leave;

	int p, *d;
	m_foreach (srcs, p, d)
	{
		if (p) {
			m_slice (dest, m_len (dest), seperator, 0, -2);
		}
		m_slice (dest, m_len (dest), *d, 0, -2);
	}
leave:
	m_putc (dest, 0);
	return dest;
}

int s_strdup_c (const char *s) { return s_dup (s); }

int s_strcpy_c (int out, const char *s)
{
	if (out <= 0)
		out = s_new ();
	m_clear (out);
	s_app1 (out, (char *)s);
	return out;
}

void s_puts (int m) { printf ("%s\n", m_str (m)); }

void s_write (int m, int n)
{
	if (m_len (m) > n) {
		m_setlen (m, n);
		m_putc (m, 0);
		m_setlen (m, n);
	}
}

/* Custom printf support */

static int mls_printf_handler (FILE *stream, const struct printf_info *info,
			       const void *const *args)
{
	const int p = *((const int *)args[0]);
	return fprintf (stream, "%s", m_str (p));
}

static int mls_printf_arginfo (const struct printf_info *info, size_t n,
			       int *argtypes, int *size)
{
	if (n > 0) {
		argtypes[0] = PA_INT;
	}
	return 1;
}

void m_register_printf ()
{
	if (register_printf_specifier ('M', mls_printf_handler,
				       mls_printf_arginfo) != 0) {
		ERR ("Failed to register printf specifier 'M'\n");
	}
}

/* Conststr System */

static int CS_MAP = 0;

void conststr_init (void)
{
	if (CS_MAP)
		return;
	CS_MAP = m_create (100, sizeof (int));
	CS_ZERO = s_dup ("");
}

void conststr_free (void)
{
	if (!CS_MAP)
		return;
	int p;
	int *d;
	m_foreach (CS_MAP, p, d) { s_free (*d); }
	m_free (CS_MAP);
	CS_MAP = 0;
	s_free (CS_ZERO);
	CS_ZERO = -1;
}

int conststr_lookup_c (const char *s)
{
	if (!s || !*s)
		return CS_ZERO;
	int p, *d;
	m_foreach (CS_MAP, p, d)
	{
		if (strcmp (m_str (*d), s) == 0)
			return *d;
	}
	int h = s_dup (s);
	m_put (CS_MAP, &h);
	return h;
}

int conststr_lookup (int s) { return conststr_lookup_c (m_str (s)); }

int cs_printf (const char *format, ...)
{
	va_list ap;
	va_start (ap, format);
	int h = s_new ();
	vas_printf (h, 0, format, ap);
	va_end (ap);
	int res = conststr_lookup (h);
	s_free (h);
	return res;
}

/* Variable System */

int v_init (void) { return m_create (100, sizeof (int)); }

void v_free (int vl)
{
	int p, *d;
	m_foreach (vl, p, d) m_free_strings (*d, 0);
	m_free (vl);
}

int v_find_key (int vs, const char *name)
{
	char *s;
	int p, *d;
	m_foreach (vs, p, d)
	{
		s = STR (*d, 0);
		if (strcmp (s, name) == 0)
			return p;
	}
	return -1;
}

int v_lookup (int vl, const char *name)
{
	if (is_empty (name))
		return -1;
	int p = v_find_key (vl, name);
	if (p >= 0) {
		return INT (vl, p);
	}
	TRACE (1, "Create on Stack (%d) Var: %s", vl, name);
	int var = m_create (2, sizeof (char *));
	char *s = strdup (name);
	m_put (var, &s);
	m_put (vl, &var);
	return var;
}

void v_kset (int var, const char *v, int row)
{
	char *val = NULL;
	if (v)
		val = strdup (v);
	if (row < 0 || row >= m_len (var)) {
		m_put (var, &val);
	} else {
		char **d = (char **)mls (var, row);
		if (*d)
			free (*d);
		*d = val;
	}
}

int v_set (int vs, const char *name, const char *value, int pos)
{
	int key = v_lookup (vs, name);
	v_kset (key, value, pos);
	return key;
}

void v_vaset (int vs, ...)
{
	va_list argptr;
	char *name, *value;
	va_start (argptr, vs);
	while ((name = va_arg (argptr, char *)) != NULL) {
		value = va_arg (argptr, char *);
		v_set (vs, name, value, VAR_APPEND);
	}
	va_end (argptr);
}

void v_kclr (int var)
{
	int i = 0;
	char **d;
	while (m_next (var, &i, &d))
		if (*d) {
			free (*d);
			*d = NULL;
		}
	m_setlen (var, 1);
}

void v_clr (int vs, const char *name) { v_kclr (v_lookup (vs, name)); }

char *v_kget (int var, int row)
{
	if (var <= 0)
		return "";
	int len = m_len (var);
	if (row >= len)
		return "";
	char *s = STR (var, row);
	if (s == NULL)
		return "";
	return s;
}

char *v_get (int vs, const char *name, int pos)
{
	return v_kget (v_lookup (vs, name), pos);
}

int v_klen (int key) { return m_len (key) - 1; }

int v_len (int vs, const char *name) { return v_klen (v_lookup (vs, name)); }

void v_remove (int vs, const char *name)
{
	int pos = v_find_key (vs, name);
	if (pos >= 0) {
		m_free_strings (INT (vs, pos), 0);
		m_del (vs, pos);
	}
}

/* String Expansion System */

void se_init (str_exp_t *se) { memset (se, 0, sizeof *se); }

void se_free (str_exp_t *se)
{
	m_free_strings (se->splitbuf, 0);
	m_free (se->values);
	m_free (se->indices);
	m_free (se->buf);
	memset (se, 0, sizeof *se);
}

void se_realloc_buffers (str_exp_t *se)
{
	if (!se->buf) {
		se->splitbuf = m_create (10, sizeof (char *));
		se->values = m_create (10, sizeof (char *));
		se->indices = m_create (10, sizeof (int));
		se->buf = m_create (100, 1);
		return;
	}
	m_free_strings (se->splitbuf, 1);
	m_clear (se->values);
	m_clear (se->indices);
	m_clear (se->buf);
	se->max_row = 0;
}

static int parse_index (const char **s)
{
	int val;
	const char *p = *s;
	if (*p != '[')
		return 0;
	p++;
	if (*p == '*' && p[1] == ']') {
		*s = p + 2;
		return 1;
	}
	val = 0;
	while (isdigit (*p)) {
		val *= 10;
		val += *p - '0';
		p++;
	}
	if (*p == ']') {
		*s = p + 1;
		return val + 2;
	}
	return 0;
}

void se_parse (str_exp_t *se, const char *frm)
{
	ASSERT (frm && se);
	se_realloc_buffers (se);
	int b = se->splitbuf;
	int v = se->values;
	int idx = se->indices;
	char *cp;
	const char *s, *s0;
	s = frm;
	s0 = s;
	while (*s) {
		if (*s == '\\' && s[1] == '$') {
			/* Found escaped dollar. We need to collect what we have and then add the $ */
			if (s > s0) {
				cp = strndup (s0, s - s0);
				m_put (b, &cp);
			}
			cp = strdup ("$");
			m_put (b, &cp);
			s += 2;
			s0 = s;
			continue;
		}
		if (*s == '$') {
			if (s > s0) {
				cp = strndup (s0, s - s0);
				m_put (b, &cp);
			}
			s0 = s; /* Point to $ */
			s++;
			int quoted = 0;
			if (*s == '\'') {
				quoted = 1;
				s++;
			}
			while (isalnum (*s) || *s == '_')
				s++;
			
			cp = strndup (s0, s - s0);
			m_put (b, &cp);
			m_put (v, &cp);
			
			int index = parse_index (&s);
			m_put (idx, &index);

			if (quoted && *s == '\'') {
				s++;
			}
			s0 = s;
			continue;
		}
		s++;
	}
	if (s > s0) {
		cp = strndup (s0, s - s0);
		m_put (b, &cp);
	}
}

static void repl_char (int buf, char ch)
{
	char tab[] = {'\\', '\0', '\n', '\r', '\'', '"', '\x1a'};
	char rep[] = {'\\', '0', 'n', 'r', '\'', '"', 'Z'};
	int i;
	for (i = 0; (size_t)i < sizeof tab; i++)
		if (tab[i] == ch) {
			m_putc (buf, '\\');
			m_putc (buf, rep[i]);
			return;
		}
	m_putc (buf, ch);
}

void escape_buf (int buf, char *src)
{
	while (*src)
		repl_char (buf, *src++);
}

int escape_str (int buf, char *src)
{
	if (!buf)
		buf = m_create (100, 1);
	else
		m_clear (buf);
	escape_buf (buf, src);
	m_putc (buf, 0);
	return buf;
}

static int field_escape (int s2, char *s, int quotes)
{
	if (quotes)
		m_putc (s2, '\'');
	escape_buf (s2, s);
	if (quotes)
		m_putc (s2, '\'');
	return s2;
}

static int get_variable_handle (int vl, const char *name)
{
	if (m_is_table (vl)) {
		return m_table_get_cstr (vl, name);
	}
	return v_lookup (vl, name);
}

char *se_expand (str_exp_t *se, int vl, int row)
{
	int var, index;
	int p, vn;
	char **d, *s, **val_ptr;
	int quotes = 0;
	m_clear (se->buf);
	int buf = se->buf;
	vn = 0;
	m_foreach (se->splitbuf, p, d)
	{
		s = *d;
		/* Check if this part in splitbuf matches the next expected variable in se->values */
		if (vn < m_len (se->values) && (val_ptr = (char **)mls (se->values, vn)) && *val_ptr == s) {
			int name_offset = 1;
			if (s[1] == '\'') {
				quotes = 1;
				name_offset = 2;
			} else {
				quotes = 0;
			}
			var = get_variable_handle (vl, s + name_offset);
			index = INT (se->indices, vn);
			vn++;
			if (index == 1) {
				/* [*] expansion - join all if it's a list */
				if (var > 0 && m_width (var) == sizeof (char *)) {
					field_escape (buf, STR (var, 1), quotes);
					for (index = 2; index < m_len (var); index++) {
						m_putc (buf, ',');
						field_escape (buf, STR (var, index),
							      quotes);
					}
				} else if (var > 0) {
					/* Just a single string handle? */
					field_escape (buf, m_str (var), quotes);
				}
			} else {
				if (index == 0)
					index = row;
				else
					index -= 2;

				if (var > 0) {
					if (m_width (var) == sizeof (char *)) {
						/* Variable System style: list of char* */
						if (index < v_klen (var))
							field_escape (buf,
								      STR (var,
									   index +
										   1),
								      quotes);
					} else {
						/* Single string handle (m_table style) */
						field_escape (buf, m_str (var),
							      quotes);
					}
				}
			}
		} else {
			/* Constant part */
			m_write (buf, m_len (buf), s, strlen (s));
		}
	}
	m_putc (buf, 0);
	return mls (buf, 0);
}

char *se_string (int vl, const char *frm)
{
	str_exp_t se;
	se_init (&se);
	se_parse (&se, frm);
	se_expand (&se, vl, 0);

	char *res;
	if (m_is_table (vl)) {
		m_table_set_string_by_cstr (vl, "se_string", mls (se.buf, 0));
		int h = m_table_get_cstr (vl, "se_string");
		res = m_str (h);
	} else {
		int data = v_set (vl, "se_string", mls (se.buf, 0), 1);
		res = STR (data, 1);
	}

	se_free (&se);
	return res;
}

/* Other Utilities */

int m_str_from_file (char *filename)
{
	int m = m_alloc (100, 1, MFREE);
	int fd = open (filename, O_RDONLY);
	if (fd < 0) {
		m_free (m);
		return -1;
	}
	ioread_all (fd, m);
	close (fd);
	return m;
}

static void skip_delim (char **s, char *delim)
{
	while (**s && *delim && **s == *delim) {
		(*s)++;
		delim++;
	}
}

static void skip_unitl_delim (char **s, char *delim)
{
	char *p = strstr (*s, delim);
	if (p) {
		*s = p;
		return;
	}
	while (**s)
		(*s)++;
}

static int cut_word (char **s, char *delim, int trimws, char **a, char **b)
{
	static char *empty = "";
	*a = *s;
	skip_unitl_delim (s, delim);
	*b = *s;
	skip_delim (s, delim);
	if (*a == *b) {
		*a = empty;
		return 0;
	}
	return 1;
}

static char *dup_word (char *a, char *b, int trimws)
{
	if (a == b)
		return strdup ("");
	if (trimws) {
		while (a < b && isspace (*a))
			a++;
		while (b > a && isspace (b[-1]))
			b--;
	}
	return strndup (a, b - a);
}

int m_str_split (int ms, char *s, char *delim, int trimws)
{
	char *a, *b;
	while (cut_word (&s, delim, trimws, &a, &b)) {
		char *w = dup_word (a, b, trimws);
		m_put (ms, &w);
	}
	return ms;
}

int s_strcmp_c (int mstr, const char *s)
{
	if (!s)
		return 1;
	return strcmp (m_str (mstr), s);
}

int m_strncpy (int dst, int src, int max)
{
	if (dst <= 0)
		dst = m_create (max + 1, 1);
	int len = m_len (src);
	if (len > max)
		len = max;
	m_write (dst, 0, mls (src, 0), len);
	m_putc (dst, 0);
	m_setlen (dst, len);
	return dst;
}

static void element_copy (int dest, int destp, int src, int srcp, int src_count,
			  int width)
{
	for (int i = 0; i < src_count; i++) {
		void *from = mls (src, srcp);
		memcpy (mls (dest, destp), from, width);
		srcp++;
		destp++;
	}
}

int m_mcopy (int dest, int destp, int src, int srcp, int src_count)
{
	if (src <= 0)
		return dest;
	if (srcp < 0)
		srcp = 0;
	if (src_count < 0 || srcp + src_count > m_len (src))
		src_count = m_len (src) - srcp;
	if (destp < 0)
		destp = m_len (dest);
	if (dest <= 0 || m_width (dest) == m_width (src)) {
		return m_slice (dest, destp, src, srcp, srcp + src_count - 1);
	}
	int dest_len = destp + src_count;
	if (m_len (dest) < dest_len)
		m_setlen (dest, dest_len);
	if (src_count == 0)
		return dest;
	int width = Min (m_width (src), m_width (dest));
	element_copy (dest, destp, src, srcp, src_count, width);
	return dest;
}

void m_memset (int ln, char c, int w)
{
	if (ln <= 0)
		return;
	m_setlen (ln, w);
	memset (m_buf (ln), c, w);
}

int s_strncmp2 (int s0, int p0, int s1, int p1, int len)
{
	return strncmp (m_str (s0) + p0, m_str (s1) + p1, len);
}

int s_strncmpr (int str, int suffix)
{
	int l0 = s_strlen (str);
	int l1 = s_strlen (suffix);
	if (l1 > l0)
		return 1;
	return strcmp (m_str (str) + l0 - l1, m_str (suffix));
}

int s_readln (int buf, FILE *fp)
{
	int c;
	m_clear (buf);
	while ((c = fgetc (fp)) != EOF) {
		if (c == '\n')
			break;
		m_putc (buf, c);
	}
	if (c == EOF && m_len (buf) == 0)
		return EOF;
	m_putc (buf, 0);
	m_setlen (buf, m_len (buf) - 1);
	return m_len (buf);
}
