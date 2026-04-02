#include "m_extra.h"
#include <strings.h>
#include <ctype.h>

/**
 * Case-insensitive comparison of two strings.
 *
 * @param a The first string handle.
 * @param b The second string handle.
 * @return <0 if a < b, 0 if a == b, >0 if a > b.
 */
int s_casecmp (int a, int b)
{
	if (a == 0 && b == 0)
		return 0;
	if (a == 0)
		return -1;
	if (b == 0)
		return 1;
	return strcasecmp (m_str (a), m_str (b));
}

/**
 * Case-insensitive comparison of up to n characters of two strings.
 *
 * @param a The first string handle.
 * @param b The second string handle.
 * @param n Maximum number of characters to compare.
 * @return <0 if a < b, 0 if a == b, >0 if a > b.
 */
int s_ncasecmp (int a, int b, int n)
{
	if (n <= 0)
		return 0;
	if (a == 0 && b == 0)
		return 0;
	if (a == 0)
		return -1;
	if (b == 0)
		return 1;
	return strncasecmp (m_str (a), m_str (b), n);
}

/**
 * Converts a string to a long integer.
 *
 * @param h The string handle.
 * @return The long value of the string, or 0 if string is empty or invalid.
 */
long s_to_long (int h)
{
	if (s_isempty (h))
		return 0;
	return atol (m_str (h));
}

/**
 * Trims specific characters from the left side of a string.
 *
 * @param h The string handle.
 * @param chars A string containing characters to trim. If NULL, trims whitespace.
 * @return A new handle containing the trimmed string.
 */
int s_trim_left_c (int h, const char *chars)
{
	if (h == 0)
		return s_new ();
	if (chars == NULL || *chars == 0)
		chars = " \t\n\r";

	const char *s = m_str (h);
	int len = s_strlen (h);
	int start = 0;

	while (start < len && strchr (chars, s[start]))
		start++;

	return s_sub (h, start, len - start);
}

/**
 * Trims specific characters from the right side of a string.
 *
 * @param h The string handle.
 * @param chars A string containing characters to trim. If NULL, trims whitespace.
 * @return A new handle containing the trimmed string.
 */
int s_trim_right_c (int h, const char *chars)
{
	if (h == 0)
		return s_new ();
	if (chars == NULL || *chars == 0)
		chars = " \t\n\r";

	const char *s = m_str (h);
	int len = s_strlen (h);
	int end = len - 1;

	while (end >= 0 && strchr (chars, s[end]))
		end--;

	return s_sub (h, 0, end + 1);
}

/**
 * Reverses the string in-place.
 *
 * @param h The string handle.
 * @return The original handle (h).
 */
int s_reverse (int h)
{
	if (s_isempty (h))
		return h;
	char *s = (char *)m_buf (h);
	int len = s_strlen (h);
	for (int i = 0; i < len / 2; i++) {
		char tmp = s[i];
		s[i] = s[len - 1 - i];
		s[len - 1 - i] = tmp;
	}
	return h;
}

/**
 * Pads the string on the left to reach a specified width.
 *
 * @param h The string handle.
 * @param width The target width.
 * @param pad The character to pad with.
 * @return A new handle containing the padded string.
 */
int s_pad_left (int h, int width, char pad)
{
	int len = h ? s_strlen (h) : 0;
	if (len >= width)
		return h ? s_clone (h) : s_new ();

	int new_h = s_new ();
	int pad_len = width - len;
	for (int i = 0; i < pad_len; i++)
		m_putc (new_h, pad);
	if (h)
		s_app1 (new_h, (char *)m_str (h));
	else
		m_putc (new_h, 0);

	return new_h;
}

/**
 * Pads the string on the right to reach a specified width.
 *
 * @param h The string handle.
 * @param width The target width.
 * @param pad The character to pad with.
 * @return A new handle containing the padded string.
 */
int s_pad_right (int h, int width, char pad)
{
	int len = h ? s_strlen (h) : 0;
	if (len >= width)
		return h ? s_clone (h) : s_new ();

	int new_h = h ? s_clone (h) : s_new ();
	int pad_len = width - len;
	int cur_len = s_strlen (new_h);
	m_setlen (new_h, cur_len); /* remove null terminator temporarily */
	for (int i = 0; i < pad_len; i++)
		m_putc (new_h, pad);
	m_putc (new_h, 0);
	return new_h;
}

/**
 * Checks if the string consists only of digits.
 *
 * @param h The string handle.
 * @return 1 if numeric, 0 otherwise.
 */
int s_is_numeric (int h)
{
	if (s_isempty (h))
		return 0;
	const char *s = m_str (h);
	while (*s) {
		if (!isdigit (*s++))
			return 0;
	}
	return 1;
}

/**
 * Checks if the string consists only of alphabetic characters.
 *
 * @param h The string handle.
 * @return 1 if alphabetic, 0 otherwise.
 */
/**
 * Checks if the string consists only of alphabetic characters.
 *
 * @param h The string handle.
 * @return 1 if alphabetic, 0 otherwise.
 */
int s_is_alpha (int h)
{
	if (s_isempty (h))
		return 0;
	const char *s = m_str (h);
	while (*s) {
		if (!isalpha (*s++))
			return 0;
	}
	return 1;
}

/**
 * Constant-time comparison of two strings.
 * Prevents timing attacks when comparing secrets like passwords or keys.
 *
 * @param a The first string handle.
 * @param b The second string handle.
 * @return 0 if strings are equal, 1 otherwise.
 */
int s_secure_cmp (int a, int b)
{
	if (a == 0 || b == 0)
		return (a != b);

	const unsigned char *s1 = (const unsigned char *)m_str (a);
	const unsigned char *s2 = (const unsigned char *)m_str (b);
	size_t len1 = s_strlen (a);
	size_t len2 = s_strlen (b);
	size_t i;
	unsigned char result = 0;

	if (len1 != len2)
		return 1;

	for (i = 0; i < len1; i++) {
		result |= (s1[i] ^ s2[i]);
	}

	return (result != 0);
}
