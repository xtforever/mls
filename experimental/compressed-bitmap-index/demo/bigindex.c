/* Compression-ratio benchmark for the chunked bitmap index.
 *
 * Builds one bitmap per distinct integer key of a CSV column and reports the
 * index size against the dense (K x N bits) equivalent.
 *
 *   bigindex <csv> [--col N] [--sort] [--limit N] [--label TEXT]
 *
 * Without --sort, row i gets id i (file order). With --sort, ids are assigned
 * by a stable counting sort on the key, so every key becomes one contiguous
 * id run -- the case compressed bitmaps are built for.
 *
 * Integer keys only, no quoted CSV fields (fine for ratings.csv). */
#include "bm.h"
#include "mls.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double now_s (void)
{
	struct timespec ts;
	clock_gettime (CLOCK_MONOTONIC, &ts);
	return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static long rss_kb (void)
{
	FILE *f = fopen ("/proc/self/statm", "r");
	long sz = 0, res = 0;
	if (f) {
		if (fscanf (f, "%ld %ld", &sz, &res) != 2)
			res = 0;
		fclose (f);
	}
	return res * 4;
}

static int parse_col (const char *line, int col, long *out)
{
	const char *p = line;
	for (int c = 0; c < col; c++) {
		const char *q = strchr (p, ',');
		if (!q)
			return -1;
		p = q + 1;
	}
	char *end;
	long v = strtol (p, &end, 10);
	if (end == p)
		return -1;
	*out = v;
	return 0;
}

static void human (double bytes, char *buf, size_t n)
{
	static const char *u[] = {"B", "KiB", "MiB", "GiB", "TiB", "PiB"};
	int i = 0;
	while (bytes >= 1024.0 && i < 5) {
		bytes /= 1024.0;
		i++;
	}
	snprintf (buf, n, "%.2f %s", bytes, u[i]);
}

int main (int argc, char **argv)
{
	if (argc < 2) {
		fprintf (stderr,
			 "usage: %s <csv> [--col N] [--sort] [--limit N] "
			 "[--label TEXT]\n",
			 argv[0]);
		return 2;
	}
	const char *csv = argv[1];
	int col = 0, sort = 0, verify = 0;
	long limit = 0;
	const char *label = csv;
	for (int i = 2; i < argc; i++) {
		if (strcmp (argv[i], "--col") == 0 && i + 1 < argc)
			col = atoi (argv[++i]);
		else if (strcmp (argv[i], "--sort") == 0)
			sort = 1;
		else if (strcmp (argv[i], "--verify") == 0)
			verify = 1;
		else if (strcmp (argv[i], "--limit") == 0 && i + 1 < argc)
			limit = atol (argv[++i]);
		else if (strcmp (argv[i], "--label") == 0 && i + 1 < argc)
			label = argv[++i];
		else {
			fprintf (stderr, "bad arg: %s\n", argv[i]);
			return 2;
		}
	}

	FILE *fp = fopen (csv, "r");
	if (!fp) {
		perror (csv);
		return 1;
	}
	char *line = NULL;
	size_t lcap = 0;
	if (getline (&line, &lcap, fp) < 0) {
		fclose (fp);
		return 1;
	} /* header */

	size_t n = 0, cap = 1 << 20;
	uint32_t *keys = malloc (cap * sizeof *keys);
	if (!keys)
		return 1;
	long maxkey = 0;
	while (getline (&line, &lcap, fp) >= 0) {
		if (limit && n >= (size_t)limit)
			break;
		long v;
		if (parse_col (line, col, &v) != 0 || v < 0)
			continue;
		if (n == cap) {
			cap *= 2;
			keys = realloc (keys, cap * sizeof *keys);
			if (!keys)
				return 1;
		}
		keys[n++] = (uint32_t)v;
		if (v > maxkey)
			maxkey = v;
	}
	free (line);
	fclose (fp);

	if (maxkey > 50000000L) {
		fprintf (stderr, "max key %ld too large for this tool\n",
			 maxkey);
		free (keys);
		return 1;
	}

	m_init ();
	bm_init ();

	size_t *cnt = calloc ((size_t)maxkey + 2, sizeof *cnt);
	size_t *start = calloc ((size_t)maxkey + 2, sizeof *start);
	int *bm = malloc (((size_t)maxkey + 1) * sizeof *bm);
	if (!cnt || !start || !bm)
		return 1;
	for (long k = 0; k <= maxkey; k++)
		bm[k] = -1;

	for (size_t i = 0; i < n; i++)
		cnt[keys[i] + 1]++;
	if (sort) {
		for (long k = 0; k <= maxkey; k++)
			start[k + 1] = start[k] + cnt[k + 1];
	}
	/* cursor for stable id assignment when sorted */
	size_t *cur = sort ? malloc (((size_t)maxkey + 1) * sizeof *cur) : NULL;
	if (sort)
		memcpy (cur, start, ((size_t)maxkey + 1) * sizeof *cur);

	long r0 = rss_kb ();
	size_t t0 = m_total_bytes ();
	double b0 = now_s ();

	size_t distinct = 0;
	for (size_t i = 0; i < n; i++) {
		long k = keys[i];
		if (bm[k] < 0) {
			bm[k] = bm_create ();
			distinct++;
		}
		size_t id = sort ? cur[k]++ : i;
		bm_set (bm[k], (bm_bit_t)id);
	}

	double b1 = now_s ();
	size_t t1 = m_total_bytes ();
	long r1 = rss_kb ();

	size_t chunks = 0;
	for (long k = 0; k <= maxkey; k++)
		if (bm[k] >= 0)
			chunks += (size_t)bm_chunks (bm[k]);

	if (verify) {
		int checked = 0, bad = 0;
		for (long k = 0; k <= maxkey && checked < 200; k++) {
			if (bm[k] < 0)
				continue;
			size_t want = cnt[k + 1], c = 0;
			int64_t first = -1, last = -1;
			int64_t b = bm_next1 (bm[k], 0);
			while (b >= 0) {
				if (first < 0)
					first = b;
				last = b;
				c++;
				if (b == (int64_t)UINT32_MAX)
					break;
				b = bm_next1 (bm[k], (bm_bit_t)(b + 1));
			}
			if (c != want) {
				fprintf (stderr,
					 "verify FAIL key %ld count %zu != "
					 "%zu\n",
					 k, c, want);
				bad = 1;
			}
			if (sort && want &&
			    (first != (int64_t)start[k] ||
			     last != (int64_t)(start[k] + want - 1))) {
				fprintf (stderr,
					 "verify FAIL key %ld range "
					 "[%lld,%lld] != [%zu,%zu]\n",
					 k, (long long)first, (long long)last,
					 start[k], start[k] + want - 1);
				bad = 1;
			}
			checked++;
		}
		printf ("verify            : %s (%d keys)\n",
			bad ? "FAILED" : "ok", checked);
	}

	double dense = (double)distinct * (double)n / 8.0;
	double index = (double)(r1 - r0) * 1024.0;
	char hd[32], hi[32], hp[32], hr[32];
	human (dense, hd, sizeof hd);
	human (index, hi, sizeof hi);
	human ((double)(t1 - t0), hp, sizeof hp);
	human (dense / (index > 0 ? index : 1), hr, sizeof hr);

	printf ("== %s%s ==\n", label,
		sort ? " (sorted by key)" : " (file order)");
	printf ("rows              : %zu\n", n);
	printf ("distinct keys     : %zu\n", distinct);
	printf ("dense  (K x N bit): %s  (%.0f bytes)\n", hd, dense);
	printf ("index  (RSS)      : %s  (%.0f bytes)\n", hi, index);
	printf ("index  (m_total)  : %s\n", hp);
	printf ("bitmap chunks     : %zu  (%.1f B/chunk incl. container)\n",
		chunks, chunks ? index / chunks : 0.0);
	printf ("compression ratio : %.1fx dense/index\n",
		index > 0 ? dense / index : 0.0);
	printf ("build time        : %.2fs\n", b1 - b0);
	(void)hr;

	for (long k = 0; k <= maxkey; k++)
		if (bm[k] >= 0)
			bm_destroy (bm[k]);
	free (cur);
	free (start);
	free (cnt);
	free (bm);
	free (keys);
	m_destruct ();
	return 0;
}
