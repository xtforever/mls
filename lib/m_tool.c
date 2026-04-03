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
static int CS_ZERO = -1;

/* Relocated Core Utilities */

/**
 * Creates a duplicate of an existing m-array.
 *
 * @param m The handle of the m-array to duplicate.
 * @return A new handle to the duplicated m-array.
 */
int m_dub (int m)
{
	int h = m_free_hdl (m);
	int r = m_alloc (m_len (m), m_width (m), h);
	m_write (r, 0, mls (m, 0), m_len (m));
	return r;
}

/**
 * Executes a regular expression on a string and stores sub-matches in an m-array.
 *
 * @param m The handle of the m-array to store matches. If <= 1, a new one is allocated.
 * @param regex The regular expression pattern.
 * @param s The string to search.
 * @return The handle of the m-array containing the matches.
 */
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

/**
 * Sorts an m-array using qsort.
 *
 * @param list The handle of the m-array to sort.
 * @param compar Comparison function pointer.
 */
void m_qsort (int list, int (*compar) (const void *, const void *))
{
	qsort (m_buf (list), m_len (list), m_width (list), compar);
}

/**
 * Performs a binary search on a sorted m-array.
 *
 * @param key Pointer to the element to search for.
 * @param list The handle of the sorted m-array.
 * @param compar Comparison function pointer.
 * @return The index of the element if found, or -1.
 */
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

/**
 * Performs a linear search on an m-array.
 *
 * @param key Pointer to the element to search for.
 * @param list The handle of the m-array.
 * @param compar Comparison function pointer.
 * @return The index of the element if found, or -1.
 */
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

/**
 * Inserts an element into a sorted m-array, maintaining order.
 *
 * @param buf The handle of the m-array.
 * @param data Pointer to the element to insert.
 * @param cmpf Comparison function pointer.
 * @param with_duplicates If non-zero, allows duplicate elements.
 * @return The index where the element was inserted, or -index if it exists and duplicates are not allowed.
 */
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

/**
 * Reads all available data from a file descriptor into an m-array.
 *
 * @param fd The file descriptor to read from.
 * @param buffer The handle of the m-array to store the data.
 * @return The number of bytes read, or a negative value on error.
 */
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

/**
 * Allocates a new string buffer.
 *
 * @return The handle of the new string buffer.
 */
int s_new (void) { return m_alloc (16, 1, MFREE); }

/**
 * Frees a string buffer.
 *
 * @param h The handle of the string buffer to free.
 */
void s_free (int h) { m_free (h); }

/**
 * Calculates the length of a string stored in an m-array.
 *
 * @param m The handle of the string buffer.
 * @return The length of the string, excluding the null terminator.
 */
int s_strlen (int m)
{
	if (m <= 0)
		return 0;
	int len = m_len (m);
	while (len > 0 && CHAR (m, len - 1) == 0)
		len--;
	return len;
}

/**
 * Appends a C-style string to an existing string buffer.
 *
 * @param m The handle of the string buffer.
 * @param s The C-style string to append.
 * @return The handle of the string buffer.
 */
int s_app1 (int m, char *s)
{
	int p = s_strlen (m);
	m_write (m, p, s, strlen (s) + 1);
	return m;
}

/**
 * Internal helper to append multiple strings from a va_list.
 */
static int vas_app (int m, va_list ap)
{
	char *name;
	while ((name = va_arg (ap, char *)) != NULL) {
		s_app1 (m, name);
	}
	return m;
}

/**
 * Appends one or more C-style strings to a string buffer.
 *
 * @param m The handle of the string buffer. If <= 0, a new one is allocated.
 * @param ... Variable list of C-style strings, terminated by NULL.
 * @return The handle of the string buffer.
 */
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

/**
 * Formatted append to a string buffer using a va_list.
 *
 * @param m The handle of the string buffer. If <= 0, a new one is allocated.
 * @param p The position to start writing. If < 0 or > length, it appends.
 * @param format The format string.
 * @param ap The va_list of arguments.
 * @return The handle of the string buffer.
 */
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

/**
 * Formatted append to a string buffer.
 *
 * @param m The handle of the string buffer.
 * @param p The position to start writing.
 * @param format The format string.
 * @param ... Variable list of arguments.
 * @return The handle of the string buffer.
 */
int s_printf (int m, int p, char *format, ...)
{
	va_list ap;
	va_start (ap, format);
	m = vas_printf (m, p, format, ap);
	va_end (ap);
	return m;
}

/**
 * Creates a new string buffer from a C-style string.
 *
 * @param s The C-style string to copy.
 * @return The handle of the new string buffer.
 */
int s_dup (const char *s)
{
	if (s == NULL || *s == 0)
		return s_new ();
	int h = s_new ();
	s_app1 (h, (char *)s);
	return h;
}

/**
 * Clones a string buffer.
 *
 * @param h The handle of the string buffer to clone.
 * @return A new handle to the cloned string buffer.
 */
int s_clone (int h)
{
	if (h <= 0)
		return 0;
	int ret = m_dub (h);
	if (m_len (ret) == 0 || CHAR (ret, m_len (ret) - 1) != 0)
		m_putc (ret, 0);
	return ret;
}

/**
 * Resizes a string buffer and ensures it is null-terminated.
 *
 * @param h The handle of the string buffer.
 * @param len The new length.
 * @return The handle of the string buffer.
 */
int s_resize (int h, int len)
{
	if (h == 0 || len < 0)
		return h;
	m_setlen (h, len + 1);
	CHAR (h, len) = 0;
	return h;
}

/**
 * Clears the content of a string buffer.
 *
 * @param h The handle of the string buffer.
 */
void s_clear (int h) { m_clear (h); }

/**
 * Checks if a string buffer starts with a given prefix.
 *
 * @param h The handle of the string buffer.
 * @param prefix The prefix to check.
 * @return 1 if it has the prefix, 0 otherwise.
 */
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

/**
 * Checks if a string buffer ends with a given suffix.
 *
 * @param h The handle of the string buffer.
 * @param suffix The suffix to check.
 * @return 1 if it has the suffix, 0 otherwise.
 */
int s_has_suffix (int h, const char *suffix)
{
	if (h <= 0 || suffix == NULL)
		return 0;
	int suf_len = strlen (suffix);
	int h_len = s_strlen (h);
	if (suf_len > h_len)
		return 0;
	return memcmp ((char *)m_buf (h) + h_len - suf_len, suffix, suf_len) == 0;
}

/**
 * Creates a string buffer from a long value.
 *
 * @param val The long value.
 * @return The handle of the new string buffer.
 */
int s_from_long (long val)
{
	int h = s_new ();
	s_printf (h, 0, "%ld", val);
	return h;
}

/**
 * Calculates a hash value for a string buffer (djb2 algorithm).
 *
 * @param h The handle of the string buffer.
 * @return The 32-bit hash value.
 */
uint32_t s_hash (int h)
{
	const char *s = m_str (h);
	uint32_t hash = 5381;
	int c;
	while ((c = *s++))
		hash = ((hash << 5) + hash) + c;
	return hash;
}

/**
 * Joins multiple C-style strings with a separator into a new string buffer.
 *
 * @param sep The separator string.
 * @param ... Variable list of C-style strings, terminated by NULL.
 * @return The handle of the new string buffer.
 */
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

/**
 * Compares two string buffers lexicographically.
 *
 * @param a Handle of the first string buffer.
 * @param b Handle of the second string buffer.
 * @return 0 if equal, <0 if a < b, >0 if a > b.
 */
int s_cmp (int a, int b)
{
	return s_subcmp (a, 0, -1, b, 0, -1);
}

/**
 * Compares up to n characters of two string buffers.
 *
 * @param a Handle of the first string buffer.
 * @param b Handle of the second string buffer.
 * @param n Maximum number of characters to compare.
 * @return 0 if equal, <0 if a < b, >0 if a > b.
 */
int s_ncmp (int a, int b, int n)
{
	return s_subcmp (a, 0, n - 1, b, 0, n - 1);
}

/**
 * Finds the first occurrence of a character in a string buffer.
 *
 * @param h The handle of the string buffer.
 * @param c The character to find.
 * @param off The offset to start searching from.
 * @return The index of the character, or -1.
 */
int s_chr (int h, int c, int off)
{
	if (h <= 0 || off < 0)
		return -1;
	int len = s_strlen (h);
	if (off >= len)
		return -1;
	void *p = memchr ((char *)m_buf (h) + off, c, len - off);
	if (p == NULL)
		return -1;
	return (char *)p - (char *)m_buf (h);
}

/**
 * Finds the last occurrence of a character in a string buffer.
 *
 * @param h The handle of the string buffer.
 * @param c The character to find.
 * @return The index of the character, or -1.
 */
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

/**
 * Finds the first occurrence of a substring in a string buffer.
 *
 * @param h The handle of the string buffer.
 * @param sub The substring to find.
 * @return The index of the substring, or -1.
 */
int s_find (int h, const char *sub)
{
	if (h <= 0 || sub == NULL || *sub == 0)
		return -1;
	int h_len = s_strlen (h);
	int sub_len = strlen (sub);
	void *p = memmem (m_buf (h), h_len, sub, sub_len);
	if (p == NULL)
		return -1;
	return (char *)p - (char *)m_buf (h);
}

/**
 * Calculates the length of the initial segment of a string buffer which consists entirely of characters in accept.
 *
 * @param h The handle of the string buffer.
 * @param accept C-style string containing characters to accept.
 * @return The number of characters in the initial segment.
 */
int s_spn (int h, const char *accept)
{
	if (h <= 0 || accept == NULL)
		return 0;
	const char *s = (char *)m_buf (h);
	int len = s_strlen (h);
	int i;
	for (i = 0; i < len; i++) {
		if (strchr (accept, s[i]) == NULL)
			break;
	}
	return i;
}

/**
 * Calculates the length of the initial segment of a string buffer which consists entirely of characters not in reject.
 *
 * @param h The handle of the string buffer.
 * @param reject C-style string containing characters to reject.
 * @return The number of characters in the initial segment.
 */
int s_cspn (int h, const char *reject)
{
	if (h <= 0 || reject == NULL)
		return 0;
	const char *s = (char *)m_buf (h);
	int len = s_strlen (h);
	int i;
	for (i = 0; i < len; i++) {
		if (strchr (reject, s[i]) != NULL)
			break;
	}
	return i;
}

/**
 * Concatenates a C-style string to a string buffer.
 *
 * @param h The handle of the string buffer. If <= 0, a new one is allocated.
 * @param src The C-style string to concatenate.
 * @return The handle of the string buffer.
 */
int s_cat (int h, const char *src)
{
	if (h == 0)
		h = s_new ();
	if (src)
		s_app1 (h, (char *)src);
	return h;
}

/**
 * Concatenates up to n characters of a C-style string to a string buffer.
 *
 * @param h The handle of the string buffer. If <= 0, a new one is allocated.
 * @param src The C-style string to concatenate.
 * @param n Maximum number of characters to concatenate.
 * @return The handle of the string buffer.
 */
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

/**
 * Extracts a substring into a new string buffer.
 *
 * @param h The handle of the source string buffer.
 * @param pos Starting position.
 * @param len Length of the substring.
 * @return The handle of the new string buffer.
 */
int s_sub (int h, int pos, int len)
{
	if (h <= 0 || len == 0)
		return s_new ();

	int h_len = s_strlen (h);
	if (pos < 0)
		pos = 0;
	if (pos >= h_len)
		return s_new ();

	if (len < 0 || pos + len > h_len)
		len = h_len - pos;

	return s_slice (0, 0, h, pos, pos + len - 1);
}

/**
 * Returns the leftmost n characters of a string buffer as a new string buffer.
 *
 * @param h The handle of the string buffer.
 * @param n Number of characters to return.
 * @return The handle of the new string buffer.
 */
int s_left (int h, int n) { return s_sub (h, 0, n); }

/**
 * Returns the rightmost n characters of a string buffer as a new string buffer.
 *
 * @param h The handle of the string buffer.
 * @param n Number of characters to return.
 * @return The handle of the new string buffer.
 */
int s_right (int h, int n)
{
	if (h <= 0 || n <= 0)
		return s_new ();
	int h_len = s_strlen (h);
	if (n >= h_len)
		return s_clone (h);
	return s_slice (0, 0, h, h_len - n, h_len - 1);
}

/**
 * Replaces all occurrences of a substring with another string.
 *
 * @param h The handle of the string buffer.
 * @param old Substring to find.
 * @param replacement String to replace with.
 * @return A new handle to the string buffer with replacements.
 */
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

/**
 * Trims specified characters from both ends of a string buffer.
 *
 * @param h The handle of the string buffer.
 * @param chars C-style string containing characters to trim. If NULL, whitespace is trimmed.
 * @return A new handle to the trimmed string buffer.
 */
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

/**
 * Returns the last non-null character of a string buffer.
 *
 * @param m The handle of the string buffer.
 * @return The last character, or 0 if empty.
 */
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

/**
 * Copies a range of characters from one string buffer to a new one.
 *
 * @param m The source handle.
 * @param first_char Index of the first character.
 * @param last_char Index of the last character. If negative, goes to the end.
 * @return The handle of the new string buffer.
 */
int s_copy (int m, int first_char, int last_char)
{
	return s_slice (0, 0, m, first_char, last_char);
}

/**
 * Finds the index of a character in an m-array buffer.
 *
 * @param buf The handle of the buffer.
 * @param p Starting index.
 * @param ch Character to find.
 * @return The index, or -1.
 */
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

/**
 * Splits a string into an m-array of strings based on a delimiter character.
 *
 * @param m The handle to store the resulting strings. If 0, a new one is allocated.
 * @param s The string to split.
 * @param c The delimiter character.
 * @param remove_wspace If non-zero, trims whitespace from the parts.
 * @return The handle of the m-array of strings.
 */
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

/**
 * Creates a slice of a string buffer.
 *
 * @param dest Destination handle. If 0, a new one is allocated.
 * @param offs Destination offset.
 * @param m Source handle.
 * @param a Start index.
 * @param b End index.
 * @return The handle of the resulting string buffer.
 */
int s_slice (int dest, int offs, int m, int a, int b)
{
	int ret = m_slice (dest, offs, m, a, b);
	if (ret <= 0)
		ret = s_new ();
	if (m_len (ret) == 0 || CHAR (ret, m_len (ret) - 1) != 0)
		m_putc (ret, 0);
	return ret;
}

/**
 * Finds a pattern in a string buffer.
 *
 * @param m The handle of the string buffer.
 * @param offs Starting offset.
 * @param pattern The handle of the pattern string.
 * @return The index, or -1.
 */
int s_strstr (int m, int offs, int pattern)
{
	if (m <= 0 || pattern <= 0)
		return -1;
	int m_len_val = s_strlen (m);
	if (offs < 0 || offs >= m_len_val)
		return -1;
	int p_len = s_strlen (pattern);
	if (p_len == 0)
		return offs;

	void *res = memmem (m_buf (m) + offs, m_len_val - offs, m_buf (pattern), p_len);
	if (res)
		return (char *)res - (char *)m_buf (m);
	return -1;
}

/**
 * Replaces occurrences of a pattern in a string buffer.
 *
 * @param dest Destination handle. If 0, a new one is allocated.
 * @param src Source handle.
 * @param pattern Handle of the pattern to find.
 * @param replace Handle of the replacement string.
 * @param count Maximum number of replacements.
 * @return The handle of the destination string buffer.
 */
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
	if (m_len (dest) == 0 || CHAR (dest, m_len (dest) - 1) != 0)
		m_putc (dest, 0);
	return dest;
}

/**
 * Compares a substring of a string buffer with a pattern.
 *
 * @param m Source handle.
 * @param offs Source offset.
 * @param pattern Pattern handle.
 * @param n Number of characters to compare.
 * @return 0 if equal, otherwise like strncmp.
 */
int s_strncmp (int m, int offs, int pattern, int n)
{
	return s_subcmp (m, offs, offs + n - 1, pattern, 0, n - 1);
}

/**
 * Compares two substrings of m-arrays.
 *
 * @param s0 First m-array handle.
 * @param s0a Start index of first substring.
 * @param s0b End index of first substring.
 * @param s1 Second m-array handle.
 * @param s1a Start index of second substring.
 * @param s1b End index of second substring.
 * @return 0 if equal, <0 if s0 < s1, >0 if s0 > s1.
 */
int s_subcmp (int s0, int s0a, int s0b, int s1, int s1a, int s1b)
{
	if (s0 <= 0 && s1 <= 0)
		return 0;
	if (s0 <= 0)
		return -1;
	if (s1 <= 0)
		return 1;

	int len0 = s_strlen (s0);
	int len1 = s_strlen (s1);

	// Normalize indices like m_slice
	if (s0b < 0) s0b += len0;
	if (s0a < 0) s0a += len0;
	if (s1b < 0) s1b += len1;
	if (s1a < 0) s1a += len1;

	// Clamp bounds
	if (s0a < 0) s0a = 0;
	if (s0b >= len0) s0b = len0 - 1;
	if (s1a < 0) s1a = 0;
	if (s1b >= len1) s1b = len1 - 1;

	int sub_len0 = (s0b >= s0a) ? (s0b - s0a + 1) : 0;
	int sub_len1 = (s1b >= s1a) ? (s1b - s1a + 1) : 0;

	int min_len = (sub_len0 < sub_len1) ? sub_len0 : sub_len1;
	int res = 0;
	if (min_len > 0) {
		res = memcmp ((char *)m_buf (s0) + s0a, (char *)m_buf (s1) + s1a,
			      min_len);
	}

	if (res != 0)
		return res;
	return sub_len0 - sub_len1;
}

/**
 * Checks if a string buffer is empty.
 *
 * @param m The handle of the string buffer.
 * @return 1 if empty, 0 otherwise.
 */
int s_isempty (int m)
{
	if (m <= 0 || m_len (m) == 0)
		return 1;
	return (CHAR (m, 0) == 0);
}

/**
 * Trims whitespace from both ends of a string buffer (in-place).
 *
 * @param m The handle of the string buffer.
 * @return The handle of the string buffer.
 */
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

/**
 * Converts a string buffer to lowercase (in-place).
 *
 * @param m The handle of the string buffer.
 * @return The handle of the string buffer.
 */
int s_lower (int m)
{
	if (s_isempty (m))
		return m;
	int p;
	char *d;
	m_foreach (m, p, d) *d = tolower (*d);
	return m;
}

/**
 * Converts a string buffer to uppercase (in-place).
 *
 * @param m The handle of the string buffer.
 * @return The handle of the string buffer.
 */
int s_upper (int m)
{
	if (s_isempty (m))
		return m;
	int p;
	char *d;
	m_foreach (m, p, d) *d = toupper (*d);
	return m;
}

/**
 * Splits a string buffer into an m-array of string handles.
 *
 * @param dest Handle of the resulting m-array. If 0, a new one is allocated.
 * @param src Handle of the source string buffer.
 * @param pattern Handle of the delimiter string buffer.
 * @return The handle of the resulting m-array.
 */
int s_msplit (int dest, int src, int pattern)
{
	if (dest == 0)
		dest = m_alloc (10, sizeof (int), MFREE_EACH);
	else
		m_clear (dest);
	int offs = 0;
	int plen = s_strlen (pattern);
	for (;;) {
		int pos = s_strstr (src, offs, pattern);
		if (pos < 0)
			break;
		m_puti (dest, s_slice (0, 0, src, offs, pos - 1));
		offs = pos + plen;
	}
	m_puti (dest, s_slice (0, 0, src, offs, -1));
	return dest;
}

/**
 * Joins an m-array of string handles into a single string buffer.
 *
 * @param dest Destination handle. If 0, a new one is allocated.
 * @param srcs Handle of the m-array of string handles.
 * @param separator Handle of the separator string buffer.
 * @return The handle of the destination string buffer.
 */
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

/**
 * Duplicates a C-style string into a new string buffer.
 *
 * @param s The C-style string.
 * @return The handle of the new string buffer.
 */
int s_strdup_c (const char *s) { return s_dup (s); }

/**
 * Copies a C-style string into an existing string buffer.
 *
 * @param out Handle of the destination string buffer. If <= 0, a new one is allocated.
 * @param s The C-style string.
 * @return The handle of the string buffer.
 */
int s_strcpy_c (int out, const char *s)
{
	if (out <= 0)
		out = s_new ();
	m_clear (out);
	s_app1 (out, (char *)s);
	return out;
}

/**
 * Prints a string buffer followed by a newline.
 *
 * @param m The handle of the string buffer.
 */
void s_puts (int m) { printf ("%s\n", m_str (m)); }

/**
 * Truncates a string buffer to a given length and ensures null-termination.
 *
 * @param m The handle of the string buffer.
 * @param n The new length.
 */
void s_write (int m, int n)
{
	if (m_len (m) > n) {
		m_setlen (m, n);
		m_putc (m, 0);
		m_setlen (m, n);
	}
}

/* Custom printf support */

/**
 * Internal handler for the %M specifier in printf.
 * Expects an m-array string handle.
 */
static int mls_printf_handler (FILE *stream, const struct printf_info *info,
			       const void *const *args)
{
	const int p = *((const int *)args[0]);
	return fprintf (stream, "%s", m_str (p));
}

/**
 * Internal arginfo handler for the %M specifier in printf.
 */
static int mls_printf_arginfo (const struct printf_info *info, size_t n,
			       int *argtypes, int *size)
{
	if (n > 0) {
		argtypes[0] = PA_INT;
	}
	return 1;
}

/**
 * Registers the %M specifier for printf to handle m-array string handles.
 */
void m_register_printf ()
{
	if (register_printf_specifier ('M', mls_printf_handler,
				       mls_printf_arginfo) != 0) {
		ERR ("Failed to register printf specifier 'M'\n");
	}
}

/* Conststr System */

static int CS_MAP = 0;

/**
 * Initializes the constant string system.
 */
void conststr_init (void)
{
	if (CS_MAP)
		return;
	CS_MAP = m_create (100, sizeof (int));
	CS_ZERO = s_dup ("");
}

/**
 * Frees the constant string system.
 */
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

/**
 * Looks up or creates a constant string from a C-style string.
 *
 * @param s The C-style string.
 * @return The handle of the constant string.
 */
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

/**
 * Looks up or creates a constant string from an existing string buffer.
 *
 * @param s Handle of the source string buffer.
 * @return The handle of the constant string.
 */
int conststr_lookup (int s) { return conststr_lookup_c (m_str (s)); }

/**
 * Formatted creation of a constant string.
 *
 * @param format Format string.
 * @param ... Arguments.
 * @return The handle of the constant string.
 */
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

/**
 * Initializes a variable system.
 *
 * @return The handle of the new variable system.
 */
int v_init (void) { return m_alloc (100, sizeof (int), MFREE_EACH); }

/**
 * Frees a variable system and all its variables.
 *
 * @param vl The handle of the variable system.
 */
void v_free (int vl)
{
	int p, *d;
	m_foreach (vl, p, d)
		m_free_strings (*d, 0);
	m_free (vl);
}

/**
 * Finds the index of a variable key in a variable system.
 *
 * @param vs The variable system handle.
 * @param name The name of the variable.
 * @return The index, or -1.
 */
int v_find_key (int vs, const char *name)
{
	char *s;
	int p, *d;
	if (vs <= 0 || !name)
		return -1;
	m_foreach (vs, p, d)
	{
		s = STR (*d, 0);
		if (s && strcmp (s, name) == 0)
			return p;
	}
	return -1;
}

/**
 * Looks up or creates a variable in a variable system.
 *
 * @param vl The variable system handle.
 * @param name The name of the variable.
 * @return The handle of the variable.
 */
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

/**
 * Sets a value at a specific position in a variable.
 *
 * @param var The variable handle.
 * @param v The string value.
 * @param row The position (index).
 */
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

/**
 * Sets a variable value in a variable system.
 *
 * @param vs The variable system handle.
 * @param name The variable name.
 * @param value The string value.
 * @param pos The position (VAR_APPEND for append).
 * @return The handle of the variable.
 */
int v_set (int vs, const char *name, const char *value, int pos)
{
	int key = v_lookup (vs, name);
	v_kset (key, value, pos);
	return key;
}

/**
 * Variadic set of multiple variable names and values.
 *
 * @param vs The variable system handle.
 * @param ... Pairs of (char* name, char* value), terminated by NULL.
 */
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

/**
 * Clears all values from a variable, keeping only the name.
 *
 * @param var The variable handle.
 */
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

/**
 * Clears a variable by name in a variable system.
 *
 * @param vs The variable system handle.
 * @param name The variable name.
 */
void v_clr (int vs, const char *name) { v_kclr (v_lookup (vs, name)); }

/**
 * Gets a value from a variable at a specific position.
 *
 * @param var The variable handle.
 * @param row The position.
 * @return The string value, or an empty string if not found.
 */
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

/**
 * Gets a variable value from a variable system.
 *
 * @param vs The variable system handle.
 * @param name The variable name.
 * @param pos The position.
 * @return The string value.
 */
char *v_get (int vs, const char *name, int pos)
{
	return v_kget (v_lookup (vs, name), pos);
}

/**
 * Returns the number of values in a variable (excluding the name).
 *
 * @param key The variable handle.
 * @return The number of values.
 */
int v_klen (int key) { return m_len (key) - 1; }

/**
 * Returns the number of values in a variable by name.
 *
 * @param vs The variable system handle.
 * @param name The variable name.
 * @return The number of values.
 */
int v_len (int vs, const char *name) { return v_klen (v_lookup (vs, name)); }

/**
 * Removes a variable from a variable system.
 *
 * @param vs The variable system handle.
 * @param name The variable name.
 */
void v_remove (int vs, const char *name)
{
	int pos = v_find_key (vs, name);
	if (pos >= 0) {
		m_free_strings (INT (vs, pos), 0);
		m_del (vs, pos);
	}
}

/* String Expansion System */

/**
 * Initializes a string expansion structure.
 *
 * @param se Pointer to the string expansion structure.
 */
void se_init (str_exp_t *se) { memset (se, 0, sizeof *se); }

/**
 * Frees resources associated with a string expansion structure.
 *
 * @param se Pointer to the string expansion structure.
 */
void se_free (str_exp_t *se)
{
	m_free_strings (se->splitbuf, 0);
	m_free (se->values);
	m_free (se->indices);
	m_free (se->buf);
	memset (se, 0, sizeof *se);
}

/**
 * Reallocates or initializes buffers for string expansion.
 *
 * @param se Pointer to the string expansion structure.
 */
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

/**
 * Internal helper to parse index in string expansion (e.g., [1], [*]).
 */
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

/**
 * Parses a format string for expansion.
 *
 * @param se Pointer to the string expansion structure.
 * @param frm The format string.
 */
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

/**
 * Internal helper to replace special characters with escaped sequences.
 */
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

/**
 * Escapes a string into a buffer.
 *
 * @param buf Handle of the destination buffer.
 * @param src Source C-style string.
 */
void escape_buf (int buf, char *src)
{
	while (*src)
		repl_char (buf, *src++);
}

/**
 * Creates an escaped version of a string in a new or existing buffer.
 *
 * @param buf Destination handle (if 0, a new one is allocated).
 * @param src Source C-style string.
 * @return The handle of the buffer containing the escaped string.
 */
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

/**
 * Internal helper to escape a field, optionally adding quotes.
 */
static int field_escape (int s2, char *s, int quotes)
{
	if (quotes)
		m_putc (s2, '\'');
	escape_buf (s2, s);
	if (quotes)
		m_putc (s2, '\'');
	return s2;
}

/**
 * Internal helper to get a variable handle from a variable system or table.
 */
static int get_variable_handle (int vl, const char *name)
{
	if (m_is_table (vl)) {
		return m_table_get_cstr (vl, name);
	}
	return v_lookup (vl, name);
}

/**
 * Expands a parsed format string using values from a variable system or table.
 *
 * @param se Pointer to the parsed string expansion structure.
 * @param vl Handle of the variable system or table.
 * @param row The row index to use for expansion.
 * @return Pointer to the expanded string (stored in se->buf).
 */
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

/**
 * Expands a format string in one step and returns the result.
 *
 * @param vl Handle of the variable system or table.
 * @param frm The format string.
 * @return Pointer to the expanded string.
 */
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

/**
 * Reads the entire content of a file into a new string buffer.
 *
 * @param filename Path to the file.
 * @return The handle of the new string buffer, or -1 on error.
 */
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

/**
 * Internal helper to skip a delimiter string.
 */
static void skip_delim (char **s, char *delim)
{
	while (**s && *delim && **s == *delim) {
		(*s)++;
		delim++;
	}
}

/**
 * Internal helper to skip characters until a delimiter string is found.
 */
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

/**
 * Internal helper to cut a word from a string based on a delimiter.
 */
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

/**
 * Internal helper to duplicate a word between two pointers, optionally trimming whitespace.
 */
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

/**
 * Splits a string into an m-array of strings using a delimiter string.
 *
 * @param ms Handle of the m-array to store strings.
 * @param s Source C-style string.
 * @param delim Delimiter string.
 * @param trimws If non-zero, trims whitespace from results.
 * @return The m-array handle.
 */
int m_str_split (int ms, char *s, char *delim, int trimws)
{
	char *a, *b;
	while (cut_word (&s, delim, trimws, &a, &b)) {
		char *w = dup_word (a, b, trimws);
		m_put (ms, &w);
	}
	return ms;
}

/**
 * Compares an m-array string handle with a C-style string.
 *
 * @param mstr Handle of the m-array string.
 * @param s C-style string.
 * @return 0 if equal, otherwise like strcmp.
 */
int s_strcmp_c (int mstr, const char *s)
{
	if (!s)
		return 1;
	return strcmp (m_str (mstr), s);
}

/**
 * Copies a string from one handle to another with a maximum length.
 *
 * @param dst Destination handle. If <= 0, a new one is allocated.
 * @param src Source handle.
 * @param max Maximum number of characters to copy.
 * @return The destination handle.
 */
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

/**
 * Internal helper to copy elements between m-arrays.
 */
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

/**
 * Copies a range of elements from one m-array to another, handling width differences.
 *
 * @param dest Destination handle.
 * @param destp Destination offset.
 * @param src Source handle.
 * @param srcp Source offset.
 * @param src_count Number of elements to copy.
 * @return The destination handle.
 */
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

/**
 * Fills an m-array with a constant byte.
 *
 * @param ln Handle of the m-array.
 * @param c Byte value.
 * @param w Number of bytes to fill.
 */
void m_memset (int ln, char c, int w)
{
	if (ln <= 0)
		return;
	m_setlen (ln, w);
	memset (m_buf (ln), c, w);
}

/**
 * Compares substrings of two m-array string handles.
 */
int s_strncmp2 (int s0, int p0, int s1, int p1, int len)
{
	return s_subcmp (s0, p0, p0 + len - 1, s1, p1, p1 + len - 1);
}

/**
 * Compares the end of a string buffer with a suffix string buffer.
 *
 * @param str Handle of the string buffer.
 * @param suffix Handle of the suffix string buffer.
 * @return 0 if it matches, otherwise non-zero.
 */
int s_strncmpr (int str, int suffix)
{
	return s_subcmp (str, -s_strlen (suffix), -1, suffix, 0, -1);
}

/**
 * Reads a line from a file into an m-array string buffer.
 *
 * @param buf Handle of the string buffer.
 * @param fp File pointer.
 * @return The length of the line read, or EOF.
 */
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
	if (m_len (buf) == 0 || CHAR (buf, m_len (buf) - 1) != 0)
		m_putc (buf, 0);
	return m_len (buf) - 1;
}


/**
 * Creates a ring buffer (circular buffer) of integers.
 *
 * @param size The capacity of the ring buffer.
 * @return The handle of the new ring buffer.
 */
int ring_create (int size)
{
	int r = m_create (size + 1, sizeof (int));
	lst_t *lp = exported_get_list (r);
	int *rd = lst_peek (*lp, 0);
	int *wr = &(*lp)->l;
	*rd = -1;
	*wr = 1;
	return r;
}

/**
 * Checks if a ring buffer is empty.
 *
 * @param r The ring buffer handle.
 * @return 1 if empty, 0 otherwise.
 */
int ring_empty (int r)
{
	lst_t *lp = exported_get_list (r);
	int *rd = lst_peek (*lp, 0);
	return (*rd < 0);
}

/**
 * Checks if a ring buffer is full.
 *
 * @param r The ring buffer handle.
 * @return 1 if full, 0 otherwise.
 */
int ring_full (int r)
{
	lst_t *lp = exported_get_list (r);
	int *rd = lst_peek (*lp, 0);
	int *wr = &(*lp)->l;
	return (*rd == *wr);
}

/**
 * Appends an integer to a ring buffer.
 *
 * @param r The ring buffer handle.
 * @param data The integer to append.
 * @return 0 on success, -1 if the buffer is full.
 */
int ring_put (int r, int data)
{
	lst_t *lp = exported_get_list (r);
	int *rd = lst_peek (*lp, 0);
	int *wr = &(*lp)->l;
	int max = (*lp)->max;
	int *d = lst_peek (*lp, *wr);
	if (*rd == *wr)
		return -1;
	*d = data;
	if (*rd < 0)
		*rd = *wr;
	(*wr)++;
	if (*wr >= max)
		*wr = 1;
	return 0;
}

/**
 * Retrieves and removes the oldest integer from a ring buffer.
 *
 * @param r The ring buffer handle.
 * @return The integer retrieved, or -1 if the buffer is empty.
 */
int ring_get (int r)
{
	lst_t *lp = exported_get_list (r);
	int *rd = lst_peek (*lp, 0);
	int *wr = &(*lp)->l;
	int max = (*lp)->max;
	if (*rd < 0)
		return -1;
	int *d = lst_peek (*lp, *rd);
	(*rd)++;
	if (*rd >= max)
		*rd = 1;
	if (*rd == *wr)
		*rd = -1;
	return *d;
}

/**
 * Frees a ring buffer.
 *
 * @param r The ring buffer handle.
 */
void ring_free (int r) { m_free (r); }


/**
 * Specialized free function for lists containing dynamically allocated strings (char *).
 * Frees each string in the list and optionally the list handle itself.
 *
 * @param list The handle of the string list.
 * @param CLEAR_ONLY If non-zero, the strings are freed and the list is cleared, but the handle remains.
 *                   If zero, the handle itself is also freed.
 */
void m_free_strings (int list, int CLEAR_ONLY)
{
	int index;
	char **strp;
	if (list < 1)
		return;
	for (index = -1; m_next (list, &index, &strp);) {
		if (*strp)
			free (*strp);
		*strp = NULL;
	}
	if (CLEAR_ONLY)
		m_clear (list);
	else
		m_free (list);
}
