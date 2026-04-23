#include "sd_strings.h"
#include "sd_thread.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <search.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

static int vas_app (int m, va_list ap);
int sd_next (int h, int *p, void **data);

int sd_new (void) { return sd_alloc (16, 1, SD_FREE); }

void sd_sfree (int h) { sd_free (h); }

int sd_strlen (int h)
{
	if (h <= 0)
		return 0;
	int len = sd_len (h);
	char *buf = sd_buf (h);
	while (len > 0 && buf[len - 1] == 0)
		len--;
	return len;
}

int sd_strdup (const char *s)
{
	if (s == NULL || *s == 0)
		return sd_new ();
	int h = sd_new ();
	sd_app1 (h, s);
	return h;
}

int sd_clone (int h)
{
	if (h <= 0)
		return 0;
	int ret = sd_alloc (sd_len (h), sd_width (h), 0);
	char *src = sd_buf (h);
	int len = sd_len (h);
	char *dst = sd_buf (ret);
	memcpy (dst, src, len);
	sd_setlen (ret, len);
	if (len == 0 || dst[len - 1] != 0)
		sd_put (ret, &(char){0});
	return ret;
}

int sd_strresize (int h, int len)
{
	if (h == 0 || len < 0)
		return h;
	sd_setlen (h, len + 1);
	char *buf = sd_buf (h);
	buf[len] = 0;
	sd_setlen (h, len);
	return h;
}

void sd_sclear (int h)
{
	if (h > 0)
		sd_clear (h);
}

int sd_app1 (int h, const char *s)
{
	if (h <= 0)
		h = sd_new ();
	int p = sd_strlen (h);
	int slen = strlen (s);
	sd_setlen (h, p + slen + 1);
	char *buf = sd_buf (h);
	memcpy (buf + p, s, slen + 1);
	return h;
}

static int vas_app (int m, va_list ap)
{
	const char *name;
	while ((name = va_arg (ap, const char *)) != NULL) {
		sd_app1 (m, name);
	}
	return m;
}

int sd_app (int m, ...)
{
	va_list ap;
	if (m <= 0)
		m = sd_new ();
	va_start (ap, m);
	vas_app (m, ap);
	va_end (ap);
	return m;
}

static int vas_printf (int m, int p, const char *format, va_list ap)
{
	int len;
	va_list copy1, copy2;
	va_copy (copy1, ap);
	va_copy (copy2, ap);
	len = vsnprintf (NULL, 0, format, copy1);
	va_end (copy1);
	len++;
	if (m <= 0) {
		m = sd_new ();
		p = 0;
	}
	if (p < 0 || p > sd_len (m))
		p = sd_strlen (m);
	sd_setlen (m, p + len);
	char *buf = sd_buf (m);
	vsnprintf (buf + p, len, format, copy2);
	va_end (copy2);
	return m;
}

int sd_printf (int m, int p, const char *format, ...)
{
	va_list ap;
	va_start (ap, format);
	m = vas_printf (m, p, format, ap);
	va_end (ap);
	return m;
}

int sd_cat (int h, const char *src)
{
	if (h == 0)
		h = sd_new ();
	if (src)
		sd_app1 (h, src);
	return h;
}

int sd_cmp (int a, int b) { return sd_subcmp (a, 0, -1, b, 0, -1); }

int sd_ncmp (int a, int b, int n)
{
	return sd_subcmp (a, 0, n - 1, b, 0, n - 1);
}

int sd_chr (int h, int c, int off)
{
	if (h <= 0 || off < 0)
		return -1;
	int len = sd_strlen (h);
	if (off >= len)
		return -1;
	char *buf = sd_buf (h);
	void *p = memchr (buf + off, c, len - off);
	if (p == NULL)
		return -1;
	return (char *)p - buf;
}

int sd_rchr (int h, int c)
{
	if (h == 0)
		return -1;
	char *s = sd_buf (h);
	int len = sd_strlen (h);
	for (int i = len - 1; i >= 0; i--) {
		if (s[i] == c)
			return i;
	}
	return -1;
}

int sd_find (int h, const char *sub)
{
	if (h <= 0 || sub == NULL || *sub == 0)
		return -1;
	int h_len = sd_strlen (h);
	int sub_len = strlen (sub);
	char *buf = sd_buf (h);
	void *p = memmem (buf, h_len, sub, sub_len);
	if (p == NULL)
		return -1;
	return (char *)p - buf;
}

int sd_has_prefix (int h, const char *prefix)
{
	if (h == 0 || prefix == NULL)
		return 0;
	int pre_len = strlen (prefix);
	int h_len = sd_strlen (h);
	if (pre_len > h_len)
		return 0;
	return strncmp (sd_buf (h), prefix, pre_len) == 0;
}

int sd_has_suffix (int h, const char *suffix)
{
	if (h <= 0 || suffix == NULL)
		return 0;
	int suf_len = strlen (suffix);
	int h_len = sd_strlen (h);
	if (suf_len > h_len)
		return 0;
	return memcmp ((char *)sd_buf (h) + h_len - suf_len, suffix, suf_len) ==
	       0;
}

int sd_subcmp (int s0, int s0a, int s0b, int s1, int s1a, int s1b)
{
	if (s0 <= 0 && s1 <= 0)
		return 0;
	if (s0 <= 0)
		return -1;
	if (s1 <= 0)
		return 1;

	int len0 = sd_strlen (s0);
	int len1 = sd_strlen (s1);

	if (s0b < 0)
		s0b += len0;
	if (s0a < 0)
		s0a += len0;
	if (s1b < 0)
		s1b += len1;
	if (s1a < 0)
		s1a += len1;

	if (s0a < 0)
		s0a = 0;
	if (s0b >= len0)
		s0b = len0 - 1;
	if (s1a < 0)
		s1a = 0;
	if (s1b >= len1)
		s1b = len1 - 1;

	int sub_len0 = (s0b >= s0a) ? (s0b - s0a + 1) : 0;
	int sub_len1 = (s1b >= s1a) ? (s1b - s1a + 1) : 0;

	int min_len = (sub_len0 < sub_len1) ? sub_len0 : sub_len1;
	int res = 0;
	if (min_len > 0) {
		res = memcmp ((char *)sd_buf (s0) + s0a,
			      (char *)sd_buf (s1) + s1a, min_len);
	}

	if (res != 0)
		return res;
	return sub_len0 - sub_len1;
}

int sd_casecmp (int a, int b)
{
	if (a <= 0 && b <= 0)
		return 0;
	if (a <= 0)
		return -1;
	if (b <= 0)
		return 1;

	int len1 = sd_strlen (a);
	int len2 = sd_strlen (b);
	int min_len = len1 < len2 ? len1 : len2;
	const unsigned char *s1 = (const unsigned char *)sd_buf (a);
	const unsigned char *s2 = (const unsigned char *)sd_buf (b);

	for (int i = 0; i < min_len; i++) {
		int diff = tolower (s1[i]) - tolower (s2[i]);
		if (diff != 0)
			return diff;
	}

	return len1 - len2;
}

int sd_ncasecmp (int a, int b, int n)
{
	if (n <= 0)
		return 0;
	if (a <= 0 && b <= 0)
		return 0;
	if (a <= 0)
		return -1;
	if (b <= 0)
		return 1;

	int len1 = sd_strlen (a);
	int len2 = sd_strlen (b);
	int min_len = len1 < len2 ? len1 : len2;
	if (min_len > n)
		min_len = n;

	const unsigned char *s1 = (const unsigned char *)sd_buf (a);
	const unsigned char *s2 = (const unsigned char *)sd_buf (b);

	for (int i = 0; i < min_len; i++) {
		int diff = tolower (s1[i]) - tolower (s2[i]);
		if (diff != 0)
			return diff;
	}

	if (min_len == n)
		return 0;
	return len1 - len2;
}

int sd_sub (int h, int pos, int len)
{
	if (h <= 0)
		return sd_new ();

	int h_len = sd_strlen (h);
	if (pos < 0)
		pos = h_len + pos;
	if (pos < 0)
		pos = 0;
	if (pos >= h_len)
		return sd_new ();

	int end_pos;
	if (len < 0) {
		end_pos = h_len + len;
	} else {
		end_pos = pos + len;
	}
	if (end_pos < 0)
		end_pos = 0;
	if (end_pos > h_len)
		end_pos = h_len;

	if (end_pos <= pos)
		return sd_new ();

	return sd_slice (0, 0, h, pos, end_pos - 1);
}

int sd_left (int h, int n) { return sd_sub (h, 0, n); }

int sd_right (int h, int n)
{
	if (h <= 0 || n <= 0)
		return sd_new ();
	int h_len = sd_strlen (h);
	if (n >= h_len)
		return sd_clone (h);
	return sd_slice (0, 0, h, h_len - n, h_len - 1);
}

int sd_copy (int m, int first, int last)
{
	return sd_slice (0, 0, m, first, last);
}

int sd_slice (int dest, int offs, int m, int a, int b)
{
	if (m <= 0)
		return dest > 0 ? dest : sd_new ();

	int m_len = sd_strlen (m);

	if (a < 0)
		a += m_len;
	if (b < 0)
		b += m_len;

	if (a < 0)
		a = 0;
	if (b >= m_len)
		b = m_len - 1;
	if (a > b)
		a = b;

	int count = b - a + 1;
	if (count <= 0)
		return dest > 0 ? dest : sd_new ();

	if (dest <= 0)
		dest = sd_new ();
	else
		sd_setlen (dest, offs);

	if (offs < sd_len (dest))
		sd_setlen (dest, offs);

	int needed = offs + count + 1;
	if (sd_bufsize (dest) < needed)
		sd_resize (dest, needed);

	char *src = sd_buf (m);
	char *dst = sd_buf (dest);
	memcpy (dst + offs, src + a, count);
	offs += count;
	dst[offs] = 0;
	sd_setlen (dest, offs);

	return dest;
}

int sd_trim (int h)
{
	int p = 0;
	if (sd_is_empty (h))
		return h;
	char *buf = sd_buf (h);
	int len = sd_len (h);
	while (p < len && isspace ((unsigned char)buf[p]))
		p++;
	if (p > 0)
		sd_remove (h, 0, p);
	len = sd_len (h) - 1;
	while (len >= 0 && (buf[len] == 0 || isspace ((unsigned char)buf[len])))
		len--;
	sd_setlen (h, len + 1);
	sd_put (h, &(char){0});
	sd_setlen (h, len + 1);
	return h;
}

int sd_trim_c (int h, const char *chars)
{
	if (h == 0)
		return sd_new ();
	if (chars == NULL || *chars == 0)
		chars = " \t\n\r";

	const char *s = sd_buf (h);
	int len = sd_strlen (h);
	int start = 0;
	int end = len - 1;

	while (start <= end && strchr (chars, s[start]))
		start++;
	while (end >= start && strchr (chars, s[end]))
		end--;

	return sd_sub (h, start, end - start + 1);
}

int sd_trim_left_c (int h, const char *chars)
{
	if (h == 0)
		return sd_new ();
	if (chars == NULL || *chars == 0)
		chars = " \t\n\r";

	const char *s = sd_buf (h);
	int len = sd_strlen (h);
	int start = 0;

	while (start < len && strchr (chars, s[start]))
		start++;

	return sd_sub (h, start, len - start);
}

int sd_trim_right_c (int h, const char *chars)
{
	if (h == 0)
		return sd_new ();
	if (chars == NULL || *chars == 0)
		chars = " \t\n\r";

	const char *s = sd_buf (h);
	int len = sd_strlen (h);
	int end = len - 1;

	while (end >= 0 && strchr (chars, s[end]))
		end--;

	return sd_sub (h, 0, end + 1);
}

int sd_lower (int h)
{
	if (sd_is_empty (h))
		return h;
	char *buf = sd_buf (h);
	int len = sd_len (h);
	for (int i = 0; i < len; i++)
		buf[i] = tolower ((unsigned char)buf[i]);
	return h;
}

int sd_upper (int h)
{
	if (sd_is_empty (h))
		return h;
	char *buf = sd_buf (h);
	int len = sd_len (h);
	for (int i = 0; i < len; i++)
		buf[i] = toupper ((unsigned char)buf[i]);
	return h;
}

int sd_reverse (int h)
{
	if (sd_is_empty (h))
		return h;
	char *s = sd_buf (h);
	int len = sd_strlen (h);
	for (int i = 0; i < len / 2; i++) {
		char tmp = s[i];
		s[i] = s[len - 1 - i];
		s[len - 1 - i] = tmp;
	}
	return h;
}

int sd_replace_c (int h, const char *old, const char *replacement)
{
	if (h == 0 || old == NULL || *old == 0)
		return sd_clone (h);
	if (replacement == NULL)
		replacement = "";

	int new_h = sd_new ();
	const char *s = sd_buf (h);
	const char *start = s;
	const char *p;
	int old_len = strlen (old);

	while ((p = strstr (start, old)) != NULL) {
		if (p > start)
			sd_slice (new_h, sd_len (new_h), h, (int)(start - s),
				  (int)(p - s - 1));
		sd_cat (new_h, replacement);
		start = p + old_len;
	}
	sd_cat (new_h, start);
	return new_h;
}

int sd_replace (int dest, int src, int pattern, int replace, int count)
{
	if (dest == 0)
		dest = sd_new ();
	else
		sd_clear (dest);

	if (sd_is_empty (src) || sd_is_empty (pattern))
		return dest;

	int offs = 0;
	int replace_last = sd_strlen (replace) - 1;
	int pattern_len = sd_strlen (pattern);

	for (;;) {
		int pos = sd_strstr (src, offs, pattern);
		if (pos < 0)
			break;
		sd_slice (dest, sd_len (dest), src, offs, pos - 1);
		sd_slice (dest, sd_len (dest), replace, 0, replace_last);
		offs = pos + pattern_len;
		if (--count == 0)
			break;
	}
	sd_slice (dest, sd_len (dest), src, offs, -1);
	if (sd_len (dest) == 0 ||
	    ((char *)sd_buf (dest))[sd_len (dest) - 1] != 0)
		sd_put (dest, &(char){0});
	return dest;
}

int sd_pad_left (int h, int width, char pad)
{
	int len = h ? sd_strlen (h) : 0;
	if (len >= width)
		return h ? sd_clone (h) : sd_new ();

	int new_h = sd_new ();
	int pad_len = width - len;
	for (int i = 0; i < pad_len; i++)
		sd_put (new_h, &pad);
	if (h)
		sd_cat (new_h, sd_buf (h));
	else
		sd_put (new_h, &(char){0});

	return new_h;
}

int sd_pad_right (int h, int width, char pad)
{
	int len = h ? sd_strlen (h) : 0;
	if (len >= width)
		return h ? sd_clone (h) : sd_new ();

	int new_h = h ? sd_clone (h) : sd_new ();
	int pad_len = width - len;
	int cur_len = sd_strlen (new_h);
	sd_setlen (new_h, cur_len);
	for (int i = 0; i < pad_len; i++)
		sd_put (new_h, &pad);
	sd_put (new_h, &(char){0});
	return new_h;
}

static int field_escape (int buf, char *s, int quotes)
{
	if (quotes)
		sd_put (buf, &(char){'\''});
	while (*s) {
		char tab[] = {'\\', '\0', '\n', '\r', '\'', '"', '\x1a'};
		char rep[] = {'0', '0', 'n', 'r', '\'', '"', 'Z'};
		int found = 0;
		for (size_t i = 0; i < sizeof (tab); i++) {
			if (tab[i] == *s) {
				sd_put (buf, &(char){'\\'});
				sd_put (buf, rep + i);
				found = 1;
				break;
			}
		}
		if (!found)
			sd_put (buf, s);
		s++;
	}
	if (quotes)
		sd_put (buf, &(char){'\''});
	return buf;
}

int sd_split (int m, const char *s, int c, int remove_wspace)
{
	int p = 0, start = 0, end;
	char *szTemp;

	if (m)
		sd_free_strings (m, 1);
	else
		m = sd_alloc (10, sizeof (char *), SD_FREE_STRINGS);

	for (;;) {
		if (remove_wspace)
			while (isspace ((unsigned char)s[p]) && s[p] != c)
				p++;
		start = p;
		while (s[p] && s[p] != c)
			p++;
		if (remove_wspace) {
			end = p;
			while (end > start && isspace ((unsigned char)s[--end]))
				;
			if (end >= start && !isspace ((unsigned char)s[end])) {
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
		sd_put (m, &szTemp);
		if (s[p])
			p++;
		else
			break;
	}
	return m;
}

int sd_msplit (int dest, int src, int pattern)
{
	if (dest == 0)
		dest = sd_alloc (10, sizeof (int), SD_FREE_LIST);
	else
		sd_clear (dest);
	int offs = 0;
	int plen = sd_strlen (pattern);
	for (;;) {
		int pos = sd_strstr (src, offs, pattern);
		if (pos < 0)
			break;
		int sub = sd_slice (0, 0, src, offs, pos - 1);
		sd_put (dest, &sub);
		offs = pos + plen;
	}
	int last = sd_slice (0, 0, src, offs, -1);
	sd_put (dest, &last);
	return dest;
}

int sd_implode (int dest, int srcs, int separator)
{
	if (dest == 0)
		dest = sd_new ();
	else
		sd_clear (dest);
	if (srcs <= 0 || sd_len (srcs) == 0 || sd_is_empty (separator))
		goto leave;

	int p;
	int *d;
	for (p = 0; p < sd_len (srcs); p++) {
		d = sd_get (srcs, p);
		if (p > 0) {
			sd_slice (dest, sd_len (dest), separator, 0, -2);
		}
		sd_slice (dest, sd_len (dest), *d, 0, -2);
	}
leave:
	sd_put (dest, &(char){0});
	return dest;
}

int sd_join (const char *sep, ...)
{
	int h = sd_new ();
	va_list ap;
	char *s;
	int first = 1;

	va_start (ap, sep);
	while ((s = va_arg (ap, char *)) != NULL) {
		if (!first && sep && *sep)
			sd_cat (h, sep);
		first = 0;
		sd_cat (h, s);
	}
	va_end (ap);
	return h;
}

int sd_from_int (int val) { return sd_from_long (val); }

int sd_to_int (int h)
{
	if (sd_is_empty (h))
		return 0;
	return atoi (sd_buf (h));
}

int sd_from_long (long val)
{
	int h = sd_new ();
	sd_printf (h, 0, "%ld", val);
	return h;
}

long sd_to_long (int h)
{
	if (sd_is_empty (h))
		return 0;
	return atol (sd_buf (h));
}

int sd_from_ulong (unsigned long val)
{
	int h = sd_new ();
	sd_printf (h, 0, "%lu", val);
	return h;
}

unsigned long sd_to_ulong (int h)
{
	if (sd_is_empty (h))
		return 0;
	return strtoul (sd_buf (h), NULL, 10);
}

int sd_from_double (double val)
{
	int h = sd_new ();
	sd_printf (h, 0, "%g", val);
	return h;
}

double sd_to_double (int h)
{
	if (sd_is_empty (h))
		return 0.0;
	return atof (sd_buf (h));
}

int sd_from_float (float val) { return sd_from_double (val); }

float sd_to_float (int h) { return (float)sd_to_double (h); }

int sd_from_bool (int val) { return sd_strdup (val ? "true" : "false"); }

int sd_to_bool (int h)
{
	if (sd_is_empty (h))
		return 0;
	const char *s = sd_buf (h);
	if (strcmp (s, "true") == 0 || strcmp (s, "1") == 0 ||
	    strcmp (s, "yes") == 0)
		return 1;
	if (strcmp (s, "false") == 0 || strcmp (s, "0") == 0 ||
	    strcmp (s, "no") == 0)
		return 0;
	return -1;
}

int sd_from_hex (int val)
{
	int h = sd_new ();
	sd_printf (h, 0, "%x", val);
	return h;
}

int sd_to_hex (int h)
{
	if (sd_is_empty (h))
		return 0;
	return (int)strtoul (sd_buf (h), NULL, 16);
}

uint32_t sd_hash (int h)
{
	if (h <= 0)
		return 0;
	const char *s = sd_buf (h);
	uint32_t hash = 5381;
	int c;
	while ((c = *s++))
		hash = ((hash << 5) + hash) + c;
	return hash;
}

int sd_is_numeric (int h)
{
	if (sd_is_empty (h))
		return 0;
	const unsigned char *s = (const unsigned char *)sd_buf (h);
	int len = sd_strlen (h);
	for (int i = 0; i < len; i++) {
		if (!isdigit (s[i]))
			return 0;
	}
	return 1;
}

int sd_is_alpha (int h)
{
	if (sd_is_empty (h))
		return 0;
	const unsigned char *s = (const unsigned char *)sd_buf (h);
	int len = sd_strlen (h);
	for (int i = 0; i < len; i++) {
		if (!isalpha (s[i]))
			return 0;
	}
	return 1;
}

int sd_is_empty (int h)
{
	if (h <= 0 || sd_len (h) == 0)
		return 1;
	return ((char *)sd_buf (h))[0] == 0;
}

int sd_regex (int m, const char *regex, const char *s)
{
	char *szTemp;
	regex_t regc;
	regmatch_t *pm;
	int i, error;
	int subexp = 1;
	int p = 0;
	error = regcomp (&regc, regex, REG_EXTENDED);
	if (error)
		SD_ERR ("REG_EXPRESSION %s not valid", regex);
	while (regex[p]) {
		if (regex[p] == '(')
			subexp++;
		p++;
	}
	pm = (regmatch_t *)malloc (sizeof (regmatch_t) * subexp);
	if (m > 1)
		sd_free_strings (m, 1);
	else
		m = sd_alloc (subexp + 1, sizeof (char *), SD_FREE_STRINGS);
	error = regexec (&regc, s, subexp, pm, 0);
	if (!error) {
		for (i = 0; i < subexp; i++) {
			if (pm[i].rm_so == -1)
				break;
			szTemp = strndup (s + pm[i].rm_so,
					  pm[i].rm_eo - pm[i].rm_so);
			sd_put (m, &szTemp);
		}
	}
	free (pm);
	regfree (&regc);
	return m;
}

int sd_strstr (int m, int offs, int pattern)
{
	if (m <= 0 || pattern <= 0)
		return -1;
	int m_len = sd_strlen (m);
	if (offs < 0 || offs >= m_len)
		return -1;
	int p_len = sd_strlen (pattern);
	if (p_len == 0)
		return offs;

	void *res = memmem (sd_buf (m) + offs, m_len - offs, sd_buf (pattern),
			    p_len);
	if (res)
		return (char *)res - (char *)sd_buf (m);
	return -1;
}

int sd_secure_cmp (int a, int b)
{
	if (a == 0 || b == 0)
		return (a != b);

	const unsigned char *s1 = (const unsigned char *)sd_buf (a);
	const unsigned char *s2 = (const unsigned char *)sd_buf (b);
	size_t len1 = sd_strlen (a);
	size_t len2 = sd_strlen (b);
	size_t i;
	unsigned char result = 0;

	if (len1 != len2)
		return 1;

	for (i = 0; i < len1; i++) {
		result |= (s1[i] ^ s2[i]);
	}

	return (result != 0);
}

int sd_base64_decode (int h)
{
	if (sd_is_empty (h))
		return sd_new ();

	static const signed char b64_inv[256] = {
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
		52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
		-1, 0,	1,  2,	3,  4,	5,  6,	7,  8,	9,  10, 11, 12, 13, 14,
		15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
		-1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
		41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1};

	const char *s = sd_buf (h);
	int len = sd_strlen (h);
	int out = sd_alloc (0, 1, SD_FREE);
	uint32_t v = 0;
	int bits = 0;

	for (int i = 0; i < len; i++) {
		if (s[i] == '=')
			break;
		signed char x = b64_inv[(unsigned char)s[i]];
		if (x == -1)
			continue;
		v = (v << 6) | (unsigned char)x;
		bits += 6;
		if (bits >= 8) {
			bits -= 8;
			unsigned char out_byte = (v >> bits) & 0xFF;
			sd_put (out, &out_byte);
		}
	}
	if (sd_len (out) == 0 || ((char *)sd_buf (out))[sd_len (out) - 1] != 0)
		sd_put (out, &(char){0});
	return out;
}

int sd_dup (int h)
{
	int r = sd_alloc (sd_bufsize (h), sd_width (h), 0);
	char *src = sd_buf (h);
	char *dst = sd_buf (r);
	int len = sd_len (h);
	int byte_len = len * sd_width (h);
	memcpy (dst, src, byte_len);
	sd_setlen (r, len);
	return r;
}

void sd_qsort (int list, int (*compar) (const void *, const void *))
{
	if (list <= 0)
		return;
	qsort (sd_buf (list), sd_len (list), sd_width (list), compar);
}

int sd_bsearch (const void *key, int list,
		int (*compar) (const void *, const void *))
{
	if (list < 1 || sd_len (list) == 0)
		return -1;
	void *res = bsearch (key, sd_buf (list), sd_len (list), sd_width (list),
			     compar);
	if (res)
		return (res - sd_buf (list)) / sd_width (list);
	return -1;
}

int sd_lfind (const void *key, int list,
	      int (*compar) (const void *, const void *))
{
	size_t max;
	if (list < 1 || sd_len (list) == 0)
		return -1;
	max = sd_len (list);
	void *res = lfind (key, sd_buf (list), &max, sd_width (list), compar);
	if (res)
		return (res - sd_buf (list)) / sd_width (list);
	return -1;
}

int sd_binsert (int buf, const void *data,
		int (*cmpf) (const void *data, const void *buf_elem),
		int with_duplicates)
{
	int left = 0;
	int right = sd_len (buf) + 1;
	int cur = 1;
	void *obj;
	int cmp;
	if (sd_len (buf) == 0) {
		sd_put (buf, data);
		return 0;
	}
	while (1) {
		cur = (left + right) / 2;
		obj = sd_get (buf, cur - 1);
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
	sd_ins (buf, cur, 1);
	sd_put (buf, data);
	return cur;
}

int sd_iread (int fd, int buffer)
{
	ssize_t total = 0;
	ssize_t n = 0;
	sd_clear (buffer);
	do {
		total += n;
		sd_setlen (buffer, total + 4096);
		n = read (fd, sd_get (buffer, total), 4096);
		if (n < 0) {
			if (errno == EINTR) {
				n = 0;
				continue;
			}
			break;
		}
	} while (n > 0);
	sd_setlen (buffer, total);
	sd_put (buffer, &(char){0});
	return n;
}

int sd_fscan2 (int m, char delim, FILE *fp)
{
	int c;
	if (m <= 0)
		m = sd_new ();
	else
		sd_clear (m);
	while ((c = fgetc (fp)) != EOF) {
		if (c == delim)
			break;
		sd_put (m, &c);
	}
	return c;
}

int sd_fscan (int m, char delim, FILE *fp)
{
	int c = sd_fscan2 (m, delim, fp);
	if (c == EOF && sd_len (m) == 0)
		return EOF;
	return sd_len (m);
}

int sd_lookup_obj (int m, void *obj, int size)
{
	int p;
	void *d;
	for (p = -1; sd_next (m, &p, &d);) {
		if (memcmp (d, obj, size) == 0)
			return p;
	}
	int len = sd_len (m);
	sd_put (m, obj);
	return len;
}

int sd_lookup_str (int m, const char *key, int NOT_INSERT)
{
	int p;
	void *d;
	if (!key || strlen (key) == 0)
		SD_ERR ("key of zero size");
	for (p = -1; sd_next (m, &p, &d);) {
		char **s = (char **)d;
		if (*s == NULL)
			continue;
		if (strcmp (*s, key) == 0)
			return p;
	}
	if (NOT_INSERT)
		return -1;
	char *dup = strdup (key);
	int len = sd_len (m);
	sd_put (m, &dup);
	return len;
}

int cmp_int (const void *a, const void *b)
{
	return (*(const int *)a) - (*(const int *)b);
}

int sd_blookup_int (int buf, int key, void (*new_fn) (void *, void *),
		    void *ctx)
{
	void *obj = calloc (1, sd_width (buf));
	*(int *)obj = key;
	int p = sd_binsert (buf, obj, cmp_int, 0);
	free (obj);
	if (p < 0)
		return (-p) - 1;
	if (new_fn)
		new_fn (sd_get (buf, p), ctx);
	return p;
}

void *sd_blookup_int_p (int buf, int key, void (*new_fn) (void *, void *),
			void *ctx)
{
	return sd_get (buf, sd_blookup_int (buf, key, new_fn, ctx));
}

int sd_binsert_int (int buf, int key)
{
	return sd_blookup_int (buf, key, NULL, NULL);
}

int sd_bsearch_int (int buf, int key)
{
	return sd_bsearch (&key, buf, cmp_int);
}

static int utf8get (int c, int *len, int *ret)
{
	if ((c & 0x80) == 0) {
		*len = 1;
		*ret = c;
		return 0;
	}
	if ((c & 0x40) == 0)
		return -1;
	if ((c & 0x20) == 0) {
		*len = 2;
		*ret = c & 0x1f;
		goto read;
	}
	if ((c & 0x10) == 0) {
		*len = 3;
		*ret = c & 0x0f;
		goto read;
	}
	if ((c & 0x08) == 0) {
		*len = 4;
		*ret = c & 0x07;
		goto read;
	}
	return -1;
read:
	*ret <<= 6;
	*ret |= c & 0x3f;
	return 1;
}

int sd_utf8char (int buf, int *p)
{
	if (buf <= 0 || p == NULL)
		return -1;
	int len, ret;
	if (*p >= sd_len (buf))
		return -1;
	int c = *(unsigned char *)sd_get (buf, *p);
	int rv = utf8get (c, &len, &ret);
	if (rv < 0)
		return -1;
	for (int i = 1; i < len; i++) {
		if (*p + i >= sd_len (buf))
			return -1;
		c = *(unsigned char *)sd_get (buf, *p + i);
		if ((c & 0xc0) != 0x80)
			return -1;
		ret = (ret << 6) | (c & 0x3f);
	}
	*p += len;
	return ret;
}

int utf8char (char **s)
{
	if (!s || !*s)
		return -1;
	int len, ret;
	int c = **(unsigned char **)s;
	int rv = utf8get (c, &len, &ret);
	if (rv < 0)
		return -1;
	for (int i = 1; i < len; i++) {
		(*s)++;
		c = **(unsigned char **)s;
		if ((c & 0xc0) != 0x80)
			return -1;
		ret = (ret << 6) | (c & 0x3f);
	}
	(*s)++;
	return ret;
}

int utf8_getchar (FILE *fp, char buf[6])
{
	int ch = fgetc (fp);
	if (ch == EOF)
		return EOF;
	buf[0] = ch;
	int len;
	if (ch < 0x80) {
		buf[1] = 0;
		return 1;
	}
	if (ch < 0xC0)
		return utf8_getchar (fp, buf);
	if (ch < 0xE0)
		len = 2;
	else if (ch < 0xF0)
		len = 3;
	else if (ch < 0xF8)
		len = 4;
	else if (ch < 0xFC)
		len = 5;
	else
		len = 6;
	for (int i = 1; i < len; i++) {
		int nx = fgetc (fp);
		if (nx == EOF)
			return EOF;
		if ((nx & 0xC0) != 0x80) {
			ungetc (nx, fp);
			break;
		}
		buf[i] = nx;
	}
	return len;
}

void sd_free_strings (int list, int clear_only)
{
	if (list <= 0)
		return;
	int len = sd_len (list);
	char **ptr;
	for (int i = 0; i < len; i++) {
		ptr = sd_get (list, i);
		if (ptr && *ptr) {
			free (*ptr);
			*ptr = NULL;
		}
	}
	if (!clear_only)
		sd_free (list);
}

void sd_free_list (int m)
{
	if (m <= 0)
		return;
	int len = sd_len (m);
	int *ptr;
	for (int i = 0; i < len; i++) {
		ptr = sd_get (m, i);
		if (ptr && *ptr > 0) {
			sd_free (*ptr);
			*ptr = 0;
		}
	}
	sd_free (m);
}

void sd_clear_list (int m)
{
	if (m <= 0)
		return;
	int len = sd_len (m);
	int *ptr;
	for (int i = 0; i < len; i++) {
		ptr = sd_get (m, i);
		if (ptr && *ptr > 0) {
			sd_free (*ptr);
			*ptr = 0;
		}
	}
	sd_clear (m);
}

void sd_free_stringlist (int m)
{
	if (m <= 0)
		return;
	int len = sd_len (m);
	char **ptr;
	for (int i = 0; i < len; i++) {
		ptr = sd_get (m, i);
		if (ptr && *ptr) {
			free (*ptr);
			*ptr = NULL;
		}
	}
	sd_free (m);
}

void sd_clear_stringlist (int m)
{
	if (m <= 0)
		return;
	int len = sd_len (m);
	char **ptr;
	for (int i = 0; i < len; i++) {
		ptr = sd_get (m, i);
		if (ptr && *ptr) {
			free (*ptr);
			*ptr = NULL;
		}
	}
	sd_clear (m);
}

void sd_register_printf (void) {}

#define sd_foreach(lst, index, ptr)                                            \
	for (index = -1; sd_next (lst, &index, &ptr);)

int sd_next (int h, int *p, void **data)
{
	if (h <= 0 || p == NULL)
		return 0;
	(*p)++;
	if (*p >= sd_len (h))
		return 0;
	*data = sd_get (h, *p);
	return 1;
}
