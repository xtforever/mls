#include "m_forth.h"

#include "hashmap.h"

#include <inttypes.h>
#include <limits.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MF_MAX_WL 16
#define MF_MAX_ORDER 16
#define MF_MAX_HANDLERS 32

#define MF_TAG 1
#define MF_IS_LIT(p) ((((intptr_t)(p)) & MF_TAG) != 0)
#define MF_LIT(v) ((void *)((((uintptr_t)(cell_t)(v)) << 1) | MF_TAG))
#define MF_VAL(p) ((cell_t)(((intptr_t)(p)) >> 1))
#define MF_PTR(p) ((m_word_t *)(((intptr_t)(p)) & ~(intptr_t)MF_TAG))

struct m_word {
	char *name;
	m_forth_code_t code;
	int immediate;
	darray_t *def;
	int has_data;
	int data_addr;
	int is_const;
	cell_t const_val;
	int is_defer;
	m_word_t *defer_target;
};

typedef struct {
	jmp_buf jb;
	int dsp, rsp, csp;
} mf_handler_t;

typedef struct {
	darray_t *id2str;
	hashmap_t *str2id;
	uint32_t count;
} mf_strtab_t;

struct m_forth {
	carray_t *dstack;
	carray_t *rstack;
	darray_t *call;
	arena_t *arena;
	htable_t *wl[MF_MAX_WL];
	int nwl;
	int cur;
	int order[MF_MAX_ORDER];
	int norder;
	darray_t *all_words;

	m_word_t *current_word;
	m_word_t *last_word;
	darray_t *current_def;
	carray_t *cf_stack;
	int compiling;
	int noname;
	m_word_t *last_created;

	tokenizer_t tok;
	strbuf_t *output;
	mf_strtab_t *strings;
	int pad;

	int loop_action; /* 1 branch, 2 skip */
	char *eval_buf;
	void *user;
	int error;
	char error_msg[160];
	int aborted;
	int throw_code;
	mf_handler_t handlers[MF_MAX_HANDLERS];
	int nhandlers;

	/* internal words, resolved once at init */
	m_word_t *w_exit, *w_branch, *w_qbranch, *w_does;
	m_word_t *w_compile, *w_is;
	m_word_t *w_loop, *w_ploop;
};

static char *mf_strdup (const char *s)
{
	size_t n = strlen (s) + 1;
	char *p = malloc (n);
	if (p)
		memcpy (p, s, n);
	return p;
}

static void mf_throw (m_forth_t *f, int code, const char *msg);

void m_forth_set_user (m_forth_t *f, void *user)
{
	if (f)
		f->user = user;
}

void *m_forth_user (const m_forth_t *f)
{
	return f ? f->user : NULL;
}

void m_forth_raise (m_forth_t *f, int code, const char *msg)
{
	if (f)
		mf_throw (f, code, msg);
}

void m_forth_push (m_forth_t *f, cell_t v)
{
	if (f)
		ca_push (f->dstack, v);
}

cell_t m_forth_pop (m_forth_t *f)
{
	if (!f)
		return 0;
	if (ca_len (f->dstack) == 0)
		mf_throw (f, -4, "stack underflow");
	return ca_pop (f->dstack);
}

cell_t m_forth_peek (m_forth_t *f, int n)
{
	if (!f)
		return 0;
	if (n < 0)
		n = ca_len (f->dstack) + n;
	if (n < 0 || n >= ca_len (f->dstack))
		mf_throw (f, -4, "stack underflow");
	return ca_peek (f->dstack, n);
}

/* k=0 top, 1 next, ... */
static cell_t mf_top (m_forth_t *f, int k)
{
	return m_forth_peek (f, -(k + 1));
}

static void mf_throw (m_forth_t *f, int code, const char *msg)
{
	f->throw_code = code;
	if (msg)
		snprintf (f->error_msg, sizeof f->error_msg, "%s", msg);
	if (f->nhandlers > 0)
		longjmp (f->handlers[f->nhandlers - 1].jb, 1);
	f->error = code;
	f->aborted = 1;
}

static void mf_msg (m_forth_t *f, const char *s) { sb_append_s (f->output, s); }

/* --- string table ------------------------------------------------------ */

static mf_strtab_t *strtab_new (void)
{
	mf_strtab_t *st = calloc (1, sizeof *st);
	if (!st)
		return NULL;
	st->id2str = da_new ();
	st->str2id = hm_new ();
	if (!st->id2str || !st->str2id) {
		da_free (st->id2str);
		hm_free (st->str2id);
		free (st);
		return NULL;
	}
	return st;
}

static void strtab_free (mf_strtab_t *st)
{
	if (!st)
		return;
	for (int i = 0; i < da_len (st->id2str); i++)
		free (da_get (st->id2str, i));
	da_free (st->id2str);
	hm_free (st->str2id);
	free (st);
}

static uint32_t strtab_intern (mf_strtab_t *st, const char *s)
{
	if (!st || !s)
		return 0;
	uint32_t id;
	if (hm_get (st->str2id, s, &id))
		return id;
	if (st->count == UINT32_MAX)
		return 0;
	char *copy = mf_strdup (s);
	if (!copy)
		return 0;
	da_push (st->id2str, copy);
	if (da_len (st->id2str) != (int)st->count + 1 ||
	    hm_put (st->str2id, copy, st->count + 1) != 0) {
		free (da_pop (st->id2str));
		free (copy);
		return 0;
	}
	return ++st->count;
}

static const char *strtab_resolve (const mf_strtab_t *st, uint32_t id)
{
	if (!st || id == 0 || id > st->count)
		return NULL;
	return da_get (st->id2str, (int)id - 1);
}

uint32_t m_forth_intern (m_forth_t *f, const char *s)
{
	return f ? strtab_intern (f->strings, s) : 0;
}

const char *m_forth_resolve (const m_forth_t *f, uint32_t id)
{
	return f ? strtab_resolve (f->strings, id) : NULL;
}

/* --- dictionary / wordlists ------------------------------------------- */

static m_word_t *word_new (m_forth_t *f, const char *name, int wid, int reg)
{
	m_word_t *w = calloc (1, sizeof *w);
	if (!w)
		mf_throw (f, -8, "out of memory");
	w->name = mf_strdup (name);
	if (!w->name)
		mf_throw (f, -8, "out of memory");
	da_push (f->all_words, w);
	if (reg) {
		ht_set (f->wl[wid], name, w);
		f->last_word = w;
	}
	return w;
}

static int wordlist_new (m_forth_t *f)
{
	if (f->nwl >= MF_MAX_WL)
		return -1;
	htable_t *h = ht_new ();
	if (!h)
		return -1;
	f->wl[f->nwl] = h;
	return f->nwl++;
}

void m_forth_define (m_forth_t *f, const char *name, m_forth_code_t code,
		     int immediate)
{
	if (!f || !name)
		return;
	m_word_t *w = word_new (f, name, f->cur, 1);
	w->code = code;
	w->immediate = immediate;
}

m_word_t *m_forth_lookup (m_forth_t *f, const char *name)
{
	if (!f || !name)
		return NULL;
	for (int i = 0; i < f->norder; i++) {
		int wid = f->order[i];
		if (wid < 0 || wid >= f->nwl)
			continue;
		m_word_t *w = ht_get (f->wl[wid], name);
		if (w)
			return w;
	}
	return NULL;
}

int m_forth_wordlist (m_forth_t *f)
{
	if (!f)
		return -1;
	int wid = wordlist_new (f);
	if (wid < 0)
		mf_throw (f, -1, "too many wordlists");
	return wid;
}

int m_forth_get_current (const m_forth_t *f) { return f ? f->cur : -1; }

int m_forth_set_current (m_forth_t *f, int wid)
{
	if (!f || wid < 0 || wid >= f->nwl)
		return -1;
	f->cur = wid;
	return 0;
}

int m_forth_set_order (m_forth_t *f, const int *wids, int n)
{
	if (!f || !wids || n < 0 || n > MF_MAX_ORDER)
		return -1;
	for (int i = 0; i < n; i++)
		if (wids[i] < 0 || wids[i] >= f->nwl)
			return -1;
	for (int i = 0; i < n; i++)
		f->order[i] = wids[i];
	f->norder = n;
	return 0;
}

int m_forth_get_order (const m_forth_t *f, int *wids, int cap)
{
	if (!f || !wids || cap < f->norder)
		return -1;
	for (int i = 0; i < f->norder; i++)
		wids[i] = f->order[i];
	return f->norder;
}

/* --- arena helpers ----------------------------------------------------- */

static int arena_ok (m_forth_t *f, cell_t addr, cell_t len)
{
	return addr >= 0 && len >= 0 && addr <= f->arena->used &&
	       len <= f->arena->used - addr;
}

static int store_string (m_forth_t *f, const char *text, int *addr, int *len)
{
	size_t n = strlen (text);
	int a = ar_alloc (f->arena, (int)n + 1);
	if (a < 0)
		return -1;
	memcpy (f->arena->mem + a, text, n + 1);
	*addr = a;
	*len = (int)n;
	return 0;
}

/* --- call stack -------------------------------------------------------- */

typedef struct {
	darray_t *def;
	int ip;
} mf_frame_t;

static void call_push (m_forth_t *f, darray_t *def, int ip)
{
	mf_frame_t *fr = malloc (sizeof *fr);
	if (!fr)
		mf_throw (f, -8, "out of memory");
	fr->def = def;
	fr->ip = ip;
	da_push (f->call, fr);
}

static int call_pop (m_forth_t *f, darray_t **def, int *ip)
{
	mf_frame_t *fr = da_pop (f->call);
	if (!fr)
		return 0;
	*def = fr->def;
	*ip = fr->ip;
	free (fr);
	return 1;
}

/* --- compiler helpers -------------------------------------------------- */

static void cf_mark (m_forth_t *f)
{
	ca_push (f->cf_stack, da_len (f->current_def));
}

static void cf_fwd (m_forth_t *f, m_word_t *w)
{
	da_push (f->current_def, w);
	ca_push (f->cf_stack, da_len (f->current_def));
	da_push (f->current_def, MF_LIT (0));
}

static void cf_back (m_forth_t *f, m_word_t *w, int dest)
{
	da_push (f->current_def, w);
	int off = dest - da_len (f->current_def) - 1;
	da_push (f->current_def, MF_LIT (off));
}

static void cf_resolve (m_forth_t *f, int idx, int target)
{
	if (target < 0)
		target = da_len (f->current_def);
	da_set (f->current_def, idx, MF_LIT (target - idx - 1));
}

/* --- inner interpreter ------------------------------------------------- */

void m_forth_execute (m_forth_t *f, m_word_t *w)
{
	if (!f || !w || f->aborted)
		return;
	while (w->is_defer) {
		if (!w->defer_target)
			mf_throw (f, -14, "uninitialized DEFER");
		w = w->defer_target;
	}
	if (w->code) {
		w->code (f);
		return;
	}
	if (w->is_const) {
		m_forth_push (f, w->const_val);
		return;
	}
	if (w->def) {
		if (w->has_data)
			m_forth_push (f, w->data_addr);
		{
			int ip = 0;
			darray_t *words = w->def;
			for (;;) {
				while (ip < da_len (words) && !f->aborted) {
					void *item = da_get (words, ip);
					if (MF_IS_LIT (item)) {
						m_forth_push (f, MF_VAL (item));
						ip++;
						continue;
					}
					m_word_t *cw = MF_PTR (item);
					while (cw->is_defer) {
						if (!cw->defer_target)
							mf_throw (f, -14,
								  "uninitialize"
								  "d DEFER");
						cw = cw->defer_target;
					}
					if (!cw->code) {
						if (cw == f->w_exit) {
							darray_t *nd;
							int ni;
							if (call_pop (f, &nd,
								      &ni)) {
								words = nd;
								ip = ni;
								continue;
							}
							return;
						}
						if (cw == f->w_branch) {
							int off = (int)MF_VAL (
								da_get (words,
									ip + 1));
							ip = ip + 2 + off;
							continue;
						}
						if (cw == f->w_qbranch) {
							cell_t flag =
								m_forth_pop (f);
							int off = (int)MF_VAL (
								da_get (words,
									ip + 1));
							ip = flag ? ip + 2
								  : ip + 2 + off;
							continue;
						}
						if (cw == f->w_does) {
							m_word_t *made =
								f->last_created;
							if (!made)
								mf_throw (
									f, -1,
									"DOES> "
									"withou"
									"t "
									"CREAT"
									"E");
							darray_t *body =
								da_new ();
							for (int j = ip + 1;
							     j < da_len (words);
							     j++)
								da_push (
									body,
									da_get (words,
										j));
							made->def = body;
							made->has_data = 1;
							f->last_created = NULL;
							ip = da_len (words);
							continue;
						}
					}
					if (cw->is_const) {
						m_forth_push (f, cw->const_val);
						ip++;
						continue;
					}
					if (cw->def) {
						call_push (f, words, ip + 1);
						if (cw->has_data)
							m_forth_push (
								f,
								cw->data_addr);
						words = cw->def;
						ip = 0;
						continue;
					}
					if (cw->has_data) {
						m_forth_push (f, cw->data_addr);
						ip++;
						continue;
					}
					if (cw->code) {
						cw->code (f);
						if ((cw == f->w_loop ||
						     cw == f->w_ploop) &&
						    f->loop_action) {
							int off = (int)MF_VAL (
								da_get (words,
									ip + 1));
							if (f->loop_action == 1)
								ip = ip + 2 +
								     off;
							else
								ip = ip + 2;
							f->loop_action = 0;
							continue;
						}
					}
					ip++;
				}
				darray_t *nd;
				int ni;
				if (!call_pop (f, &nd, &ni))
					return;
				words = nd;
				ip = ni;
			}
		}
	}
	if (w->has_data)
		m_forth_push (f, w->data_addr);
}

/* --- number parsing ---------------------------------------------------- */

static int parse_number (const char *token, cell_t *out)
{
	if (!token || !*token)
		return 0;
	const char *s = token;
	int base = 10;
	if (*s == '$') {
		base = 16;
		s++;
	} else if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
		base = 16;
		s += 2;
	} else if (*s == '\'' && s[1]) {
		*out = (unsigned char)s[1];
		return 1;
	}
	int neg = 0;
	if (*s == '-') {
		neg = 1;
		s++;
	}
	if (!*s)
		return 0;
	char *end;
	long long v = strtoll (s, &end, base);
	if (end == s || *end != '\0')
		return 0;
	if (neg)
		v = -v;
	*out = (cell_t)v;
	return 1;
}

/* --- primitives: stack ------------------------------------------------- */

static void p_dup (m_forth_t *f)
{
	cell_t a = mf_top (f, 0);
	m_forth_push (f, a);
}
static void p_drop (m_forth_t *f) { m_forth_pop (f); }
static void p_swap (m_forth_t *f)
{
	cell_t a = m_forth_pop (f), b = m_forth_pop (f);
	m_forth_push (f, a);
	m_forth_push (f, b);
}
static void p_over (m_forth_t *f) { m_forth_push (f, mf_top (f, 1)); }
static void p_rot (m_forth_t *f)
{
	cell_t c = m_forth_pop (f), b = m_forth_pop (f), a = m_forth_pop (f);
	m_forth_push (f, b);
	m_forth_push (f, c);
	m_forth_push (f, a);
}
static void p_nip (m_forth_t *f)
{
	cell_t a = m_forth_pop (f);
	m_forth_pop (f);
	m_forth_push (f, a);
}
static void p_tuck (m_forth_t *f)
{
	cell_t b = m_forth_pop (f), a = m_forth_pop (f);
	m_forth_push (f, b);
	m_forth_push (f, a);
	m_forth_push (f, b);
}
static void p_2drop (m_forth_t *f)
{
	m_forth_pop (f);
	m_forth_pop (f);
}
static void p_2dup (m_forth_t *f)
{
	cell_t a = mf_top (f, 1), b = mf_top (f, 0);
	m_forth_push (f, a);
	m_forth_push (f, b);
}
static void p_2swap (m_forth_t *f)
{
	cell_t d = m_forth_pop (f), c = m_forth_pop (f);
	cell_t b = m_forth_pop (f), a = m_forth_pop (f);
	m_forth_push (f, c);
	m_forth_push (f, d);
	m_forth_push (f, a);
	m_forth_push (f, b);
}
static void p_2over (m_forth_t *f)
{
	cell_t a = mf_top (f, 3), b = mf_top (f, 2);
	m_forth_push (f, a);
	m_forth_push (f, b);
}
static void p_qdup (m_forth_t *f)
{
	cell_t a = mf_top (f, 0);
	if (a)
		m_forth_push (f, a);
}
static void p_depth (m_forth_t *f) { m_forth_push (f, ca_len (f->dstack)); }

/* --- primitives: arithmetic -------------------------------------------- */

static void p_add (m_forth_t *f)
{
	cell_t a = m_forth_pop (f), b = m_forth_pop (f);
	m_forth_push (f, b + a);
}
static void p_sub (m_forth_t *f)
{
	cell_t a = m_forth_pop (f), b = m_forth_pop (f);
	m_forth_push (f, b - a);
}
static void p_mul (m_forth_t *f)
{
	cell_t a = m_forth_pop (f), b = m_forth_pop (f);
	m_forth_push (f, b * a);
}
static void p_div (m_forth_t *f)
{
	cell_t a = m_forth_pop (f), b = m_forth_pop (f);
	if (a == 0)
		mf_throw (f, -10, "division by zero");
	if (a == -1 && b == INTPTR_MIN)
		mf_throw (f, -11, "result out of range");
	m_forth_push (f, b / a);
}
static void p_mod (m_forth_t *f)
{
	cell_t a = m_forth_pop (f), b = m_forth_pop (f);
	if (a == 0)
		mf_throw (f, -10, "division by zero");
	if (a == -1 && b == INTPTR_MIN)
		mf_throw (f, -11, "result out of range");
	m_forth_push (f, b % a);
}
static void p_dm (m_forth_t *f)
{
	cell_t a = m_forth_pop (f), b = m_forth_pop (f);
	if (a == 0)
		mf_throw (f, -10, "division by zero");
	if (a == -1 && b == INTPTR_MIN)
		mf_throw (f, -11, "result out of range");
	m_forth_push (f, b % a);
	m_forth_push (f, b / a);
}
static void p_1p (m_forth_t *f) { m_forth_push (f, m_forth_pop (f) + 1); }
static void p_1m (m_forth_t *f) { m_forth_push (f, m_forth_pop (f) - 1); }
static void p_2p (m_forth_t *f) { m_forth_push (f, m_forth_pop (f) + 2); }
static void p_2m (m_forth_t *f) { m_forth_push (f, m_forth_pop (f) - 2); }
static void p_abs (m_forth_t *f)
{
	cell_t a = m_forth_pop (f);
	m_forth_push (f, a < 0 ? -a : a);
}
static void p_negate (m_forth_t *f) { m_forth_push (f, -m_forth_pop (f)); }

/* --- primitives: bitwise / compare ------------------------------------- */

static void p_and (m_forth_t *f)
{
	m_forth_push (f, m_forth_pop (f) & m_forth_pop (f));
}
static void p_or (m_forth_t *f)
{
	m_forth_push (f, m_forth_pop (f) | m_forth_pop (f));
}
static void p_xor (m_forth_t *f)
{
	m_forth_push (f, m_forth_pop (f) ^ m_forth_pop (f));
}
static void p_invert (m_forth_t *f) { m_forth_push (f, ~m_forth_pop (f)); }
static void p_eq (m_forth_t *f)
{
	m_forth_push (f, m_forth_pop (f) == m_forth_pop (f) ? -1 : 0);
}
static void p_ne (m_forth_t *f)
{
	m_forth_push (f, m_forth_pop (f) != m_forth_pop (f) ? -1 : 0);
}
static void p_lt (m_forth_t *f)
{
	cell_t a = m_forth_pop (f), b = m_forth_pop (f);
	m_forth_push (f, b < a ? -1 : 0);
}
static void p_gt (m_forth_t *f)
{
	cell_t a = m_forth_pop (f), b = m_forth_pop (f);
	m_forth_push (f, b > a ? -1 : 0);
}
static void p_le (m_forth_t *f)
{
	cell_t a = m_forth_pop (f), b = m_forth_pop (f);
	m_forth_push (f, b <= a ? -1 : 0);
}
static void p_ge (m_forth_t *f)
{
	cell_t a = m_forth_pop (f), b = m_forth_pop (f);
	m_forth_push (f, b >= a ? -1 : 0);
}
static void p_0eq (m_forth_t *f)
{
	m_forth_push (f, m_forth_pop (f) == 0 ? -1 : 0);
}
static void p_0lt (m_forth_t *f)
{
	m_forth_push (f, m_forth_pop (f) < 0 ? -1 : 0);
}
static void p_0gt (m_forth_t *f)
{
	m_forth_push (f, m_forth_pop (f) > 0 ? -1 : 0);
}

/* --- primitives: memory ------------------------------------------------ */

static void p_fetch (m_forth_t *f)
{
	cell_t a = m_forth_pop (f);
	if (!arena_ok (f, a, (cell_t)sizeof (cell_t)))
		mf_throw (f, -9, "invalid memory");
	m_forth_push (f, ar_fetch_cell (f->arena, (int)a));
}
static void p_store (m_forth_t *f)
{
	cell_t a = m_forth_pop (f), v = m_forth_pop (f);
	if (!arena_ok (f, a, (cell_t)sizeof (cell_t)))
		mf_throw (f, -9, "invalid memory");
	ar_store_cell (f->arena, (int)a, v);
}
static void p_cfetch (m_forth_t *f)
{
	cell_t a = m_forth_pop (f);
	if (!arena_ok (f, a, 1))
		mf_throw (f, -9, "invalid memory");
	m_forth_push (f, ar_fetch_byte (f->arena, (int)a));
}
static void p_cstore (m_forth_t *f)
{
	cell_t a = m_forth_pop (f), v = m_forth_pop (f);
	if (!arena_ok (f, a, 1))
		mf_throw (f, -9, "invalid memory");
	ar_store_byte (f->arena, (int)a, (uint8_t)v);
}
static void p_here (m_forth_t *f) { m_forth_push (f, f->arena->used); }
static void p_allot (m_forth_t *f)
{
	cell_t n = m_forth_pop (f);
	if (n < 0 || ar_alloc (f->arena, (int)n) < 0)
		mf_throw (f, -8, "out of memory");
}
static void p_comma (m_forth_t *f)
{
	cell_t v = m_forth_pop (f);
	int a = ar_alloc (f->arena, (int)sizeof (cell_t));
	if (a < 0)
		mf_throw (f, -8, "out of memory");
	ar_store_cell (f->arena, a, v);
}
static void p_plus_store (m_forth_t *f)
{
	cell_t a = m_forth_pop (f), n = m_forth_pop (f);
	if (!arena_ok (f, a, (cell_t)sizeof (cell_t)))
		mf_throw (f, -9, "invalid memory");
	ar_store_cell (f->arena, (int)a, ar_fetch_cell (f->arena, (int)a) + n);
}

/* --- primitives: return stack / output --------------------------------- */

static void p_tor (m_forth_t *f) { ca_push (f->rstack, m_forth_pop (f)); }
static void rpop (m_forth_t *f)
{
	if (ca_len (f->rstack) == 0)
		mf_throw (f, -4, "return stack underflow");
	ca_pop (f->rstack);
}
static void p_rfrom (m_forth_t *f)
{
	cell_t v = ca_peek (f->rstack, -1);
	if (ca_len (f->rstack) == 0)
		mf_throw (f, -4, "return stack underflow");
	rpop (f);
	m_forth_push (f, v);
}
static void p_rfetch (m_forth_t *f)
{
	if (ca_len (f->rstack) == 0)
		mf_throw (f, -4, "return stack underflow");
	m_forth_push (f, ca_peek (f->rstack, -1));
}
static void p_emit (m_forth_t *f)
{
	sb_append_c (f->output, (char)(m_forth_pop (f) & 0xFF));
}
static void p_cr (m_forth_t *f) { sb_append_c (f->output, '\n'); }
static void p_space (m_forth_t *f) { sb_append_c (f->output, ' '); }
static void p_spaces (m_forth_t *f)
{
	cell_t n = m_forth_pop (f);
	while (n-- > 0)
		sb_append_c (f->output, ' ');
}
static void p_type (m_forth_t *f)
{
	cell_t len = m_forth_pop (f), addr = m_forth_pop (f);
	if (!arena_ok (f, addr, len))
		mf_throw (f, -9, "invalid memory");
	sb_append_n (f->output, (char *)f->arena->mem + addr, (int)len);
}
static void p_count (m_forth_t *f)
{
	cell_t a = m_forth_pop (f);
	if (!arena_ok (f, a, 1))
		mf_throw (f, -9, "invalid memory");
	m_forth_push (f, a + 1);
	m_forth_push (f, ar_fetch_byte (f->arena, (int)a));
}
static void p_execute (m_forth_t *f)
{
	m_forth_execute (f, (m_word_t *)m_forth_pop (f));
}

/* --- primitives: numbers / constants ----------------------------------- */

static void p_bl (m_forth_t *f) { m_forth_push (f, ' '); }
static void p_true (m_forth_t *f) { m_forth_push (f, -1); }
static void p_false (m_forth_t *f) { m_forth_push (f, 0); }
static void p_cell (m_forth_t *f) { m_forth_push (f, (cell_t)sizeof (cell_t)); }
static void p_cellplus (m_forth_t *f)
{
	m_forth_push (f, m_forth_pop (f) + (cell_t)sizeof (cell_t));
}
static void p_pad (m_forth_t *f) { m_forth_push (f, f->pad); }

static void p_dot (m_forth_t *f)
{
	char buf[48];
	snprintf (buf, sizeof buf, "%" PRIdPTR " ", (intptr_t)m_forth_pop (f));
	mf_msg (f, buf);
}
static void p_udot (m_forth_t *f)
{
	char buf[48];
	snprintf (buf, sizeof buf, "%" PRIuPTR " ", (uintptr_t)m_forth_pop (f));
	mf_msg (f, buf);
}

/* --- primitives: strings ----------------------------------------------- */

static void p_sintern (m_forth_t *f)
{
	cell_t len = m_forth_pop (f), addr = m_forth_pop (f);
	if (!arena_ok (f, addr, len))
		mf_throw (f, -9, "invalid memory");
	uint32_t id = m_forth_intern (f, (char *)f->arena->mem + addr);
	if (!id)
		mf_throw (f, -8, "string intern failed");
	m_forth_push (f, id);
}
static void p_sdot (m_forth_t *f)
{
	uint32_t id = (uint32_t)m_forth_pop (f);
	const char *s = m_forth_resolve (f, id);
	if (!s)
		mf_throw (f, -1, "invalid string id");
	mf_msg (f, s);
}

/* --- primitives: control flow ------------------------------------------ */

static void p_do (m_forth_t *f)
{
	cell_t idx = m_forth_pop (f), lim = m_forth_pop (f);
	ca_push (f->rstack, lim);
	ca_push (f->rstack, idx);
}
static void p_loop (m_forth_t *f)
{
	cell_t idx = ca_pop (f->rstack) + 1;
	cell_t lim = ca_pop (f->rstack);
	ca_push (f->rstack, lim);
	ca_push (f->rstack, idx);
	if (idx < lim) {
		f->loop_action = 1;
	} else {
		ca_pop (f->rstack);
		ca_pop (f->rstack);
		f->loop_action = 2;
	}
}
static void p_ploop (m_forth_t *f)
{
	cell_t n = m_forth_pop (f);
	cell_t idx = ca_pop (f->rstack) + n;
	cell_t lim = ca_pop (f->rstack);
	ca_push (f->rstack, lim);
	ca_push (f->rstack, idx);
	int keep = (n >= 0 && idx < lim) || (n < 0 && idx >= lim);
	if (keep) {
		f->loop_action = 1;
	} else {
		ca_pop (f->rstack);
		ca_pop (f->rstack);
		f->loop_action = 2;
	}
}
static void p_i (m_forth_t *f) { m_forth_push (f, ca_peek (f->rstack, -1)); }
static void p_j (m_forth_t *f) { m_forth_push (f, ca_peek (f->rstack, -3)); }
static void p_leave (m_forth_t *f)
{
	if (ca_len (f->rstack) >= 2)
		f->rstack->items[f->rstack->len - 1] =
			f->rstack->items[f->rstack->len - 2];
}
static void p_unloop (m_forth_t *f)
{
	if (ca_len (f->rstack) >= 2) {
		ca_pop (f->rstack);
		ca_pop (f->rstack);
	}
}

/* --- compiler words ---------------------------------------------------- */

#define COMPILE_ONLY(f, msg)                                                   \
	do {                                                                   \
		if (!(f)->compiling)                                           \
			mf_throw ((f), -14, (msg));                            \
	} while (0)

static void w_colon (m_forth_t *f)
{
	char *name = tok_next (&f->tok);
	if (!name)
		mf_throw (f, -16, "missing name after :");
	m_word_t *w = word_new (f, name, f->cur, 1);
	w->def = da_new ();
	if (!w->def)
		mf_throw (f, -8, "out of memory");
	f->compiling = 1;
	f->noname = 0;
	f->current_word = w;
	f->current_def = w->def;
}
static void w_noname (m_forth_t *f)
{
	m_word_t *w = word_new (f, "", f->cur, 0);
	w->def = da_new ();
	if (!w->def)
		mf_throw (f, -8, "out of memory");
	f->compiling = 1;
	f->noname = 1;
	f->current_word = w;
	f->current_def = w->def;
}
static void w_semicolon (m_forth_t *f)
{
	COMPILE_ONLY (f, "; is compile-only");
	da_push (f->current_def, f->w_exit);
	f->compiling = 0;
	if (f->noname) {
		m_forth_push (f, (cell_t)f->current_word);
		f->noname = 0;
	}
	f->current_word = NULL;
	f->current_def = NULL;
}
static void w_immediate (m_forth_t *f)
{
	if (f->current_word)
		f->current_word->immediate = 1;
	else if (f->last_word)
		f->last_word->immediate = 1;
}
static void w_if (m_forth_t *f)
{
	COMPILE_ONLY (f, "IF is compile-only");
	cf_fwd (f, f->w_qbranch);
}
static void w_else (m_forth_t *f)
{
	COMPILE_ONLY (f, "ELSE is compile-only");
	int idx = ca_pop (f->cf_stack);
	cf_resolve (f, idx, da_len (f->current_def) + 2);
	cf_fwd (f, f->w_branch);
}
static void w_then (m_forth_t *f)
{
	COMPILE_ONLY (f, "THEN is compile-only");
	cf_resolve (f, ca_pop (f->cf_stack), -1);
}
static void w_begin (m_forth_t *f)
{
	COMPILE_ONLY (f, "BEGIN is compile-only");
	cf_mark (f);
}
static void w_until (m_forth_t *f)
{
	COMPILE_ONLY (f, "UNTIL is compile-only");
	cf_back (f, f->w_qbranch, ca_pop (f->cf_stack));
}
static void w_again (m_forth_t *f)
{
	COMPILE_ONLY (f, "AGAIN is compile-only");
	cf_back (f, f->w_branch, ca_pop (f->cf_stack));
}
static void w_while (m_forth_t *f)
{
	COMPILE_ONLY (f, "WHILE is compile-only");
	cf_fwd (f, f->w_qbranch);
}
static void w_repeat (m_forth_t *f)
{
	COMPILE_ONLY (f, "REPEAT is compile-only");
	int iw = ca_pop (f->cf_stack), db = ca_pop (f->cf_stack);
	cf_resolve (f, iw, da_len (f->current_def) + 2);
	cf_back (f, f->w_branch, db);
}
static void w_do (m_forth_t *f)
{
	COMPILE_ONLY (f, "DO is compile-only");
	da_push (f->current_def, m_forth_lookup (f, "(DO)"));
	cf_mark (f);
}
static void w_loop (m_forth_t *f)
{
	COMPILE_ONLY (f, "LOOP is compile-only");
	int d = ca_pop (f->cf_stack);
	da_push (f->current_def, f->w_loop);
	int off = d - da_len (f->current_def) - 1;
	da_push (f->current_def, MF_LIT (off));
}
static void w_ploop (m_forth_t *f)
{
	COMPILE_ONLY (f, "+LOOP is compile-only");
	int d = ca_pop (f->cf_stack);
	da_push (f->current_def, f->w_ploop);
	int off = d - da_len (f->current_def) - 1;
	da_push (f->current_def, MF_LIT (off));
}
static void w_literal (m_forth_t *f)
{
	COMPILE_ONLY (f, "LITERAL is compile-only");
	da_push (f->current_def, MF_LIT (m_forth_pop (f)));
}
static void w_recurse (m_forth_t *f)
{
	COMPILE_ONLY (f, "RECURSE is compile-only");
	if (!f->current_word)
		mf_throw (f, -14, "RECURSE outside a definition");
	da_push (f->current_def, f->current_word);
}
static void w_see (m_forth_t *f)
{
	char *name = tok_next (&f->tok);
	m_word_t *w = name ? m_forth_lookup (f, name) : NULL;
	if (!w)
		mf_throw (f, -13, "undefined word after SEE");
	mf_msg (f, ": ");
	mf_msg (f, w->name);
	mf_msg (f, " ");
	for (int i = 0; i < da_len (w->def); i++) {
		void *it = da_get (w->def, i);
		if (MF_IS_LIT (it)) {
			char b[40];
			snprintf (b, sizeof b, "%" PRIdPTR " ",
				  (intptr_t)MF_VAL (it));
			mf_msg (f, b);
		} else {
			mf_msg (f, MF_PTR (it)->name);
			mf_msg (f, " ");
		}
	}
}

static void w_postpone (m_forth_t *f)
{
	COMPILE_ONLY (f, "POSTPONE is compile-only");
	char *name = tok_next (&f->tok);
	m_word_t *w = name ? m_forth_lookup (f, name) : NULL;
	if (!w)
		mf_throw (f, -13, "undefined word after POSTPONE");
	if (w->immediate) {
		da_push (f->current_def, w);
	} else {
		da_push (f->current_def, MF_LIT ((cell_t)w));
		da_push (f->current_def, f->w_compile);
	}
}
static void p_compile (m_forth_t *f)
{
	m_word_t *w = (m_word_t *)m_forth_pop (f);
	if (!f->compiling || !f->current_def)
		mf_throw (f, -14, "(COMPILE) outside compilation");
	da_push (f->current_def, w);
}
static void w_bracket (m_forth_t *f) { f->compiling = 0; }
static void w_bracket_end (m_forth_t *f) { f->compiling = 1; }

/* --- defining words ---------------------------------------------------- */

static void w_create (m_forth_t *f)
{
	char *name = tok_next (&f->tok);
	if (!name)
		mf_throw (f, -16, "CREATE needs a name");
	m_word_t *w = word_new (f, name, f->cur, 1);
	w->has_data = 1;
	w->data_addr = f->arena->used;
	f->last_created = w;
}
static void w_constant (m_forth_t *f)
{
	char *name = tok_next (&f->tok);
	if (!name)
		mf_throw (f, -16, "CONSTANT needs a name");
	cell_t v = m_forth_pop (f);
	m_word_t *w = word_new (f, name, f->cur, 1);
	w->is_const = 1;
	w->const_val = v;
}
static void w_variable (m_forth_t *f)
{
	char *name = tok_next (&f->tok);
	if (!name)
		mf_throw (f, -16, "VARIABLE needs a name");
	int a = ar_alloc (f->arena, (int)sizeof (cell_t));
	if (a < 0)
		mf_throw (f, -8, "out of memory");
	ar_store_cell (f->arena, a, 0);
	m_word_t *w = word_new (f, name, f->cur, 1);
	w->has_data = 1;
	w->data_addr = a;
}
static void w_tick (m_forth_t *f)
{
	char *name = tok_next (&f->tok);
	m_word_t *w = name ? m_forth_lookup (f, name) : NULL;
	if (!w)
		mf_throw (f, -13, "undefined word after '");
	m_forth_push (f, (cell_t)w);
}
static void w_defer (m_forth_t *f)
{
	char *name = tok_next (&f->tok);
	if (!name)
		mf_throw (f, -16, "DEFER needs a name");
	m_word_t *w = word_new (f, name, f->cur, 1);
	w->is_defer = 1;
}
static void p_is (m_forth_t *f)
{
	m_word_t *target = (m_word_t *)m_forth_pop (f);
	cell_t xt = m_forth_pop (f);
	if (!target || !target->is_defer)
		mf_throw (f, -1, "IS target is not DEFER");
	target->defer_target = (m_word_t *)xt;
}
static void w_is (m_forth_t *f)
{
	char *name = tok_next (&f->tok);
	m_word_t *target = name ? m_forth_lookup (f, name) : NULL;
	if (!target || !target->is_defer)
		mf_throw (f, -1, "IS target is not DEFER");
	if (f->compiling) {
		da_push (f->current_def, MF_LIT ((cell_t)target));
		da_push (f->current_def, f->w_is);
	} else {
		cell_t xt = m_forth_pop (f);
		target->defer_target = (m_word_t *)xt;
	}
}

/* --- exceptions -------------------------------------------------------- */

static void p_throw (m_forth_t *f)
{
	cell_t n = m_forth_pop (f);
	if (n)
		mf_throw (f, (int)n, "THROW");
}
static void p_abort (m_forth_t *f) { mf_throw (f, -1, "ABORT"); }
static void p_catch (m_forth_t *f)
{
	cell_t xt = m_forth_pop (f);
	int dsp = ca_len (f->dstack);
	int rsp = ca_len (f->rstack);
	int csp = da_len (f->call);
	int idx = f->nhandlers;
	if (idx >= MF_MAX_HANDLERS)
		mf_throw (f, -1, "CATCH nesting too deep");
	mf_handler_t *h = &f->handlers[idx];
	h->dsp = dsp;
	h->rsp = rsp;
	h->csp = csp;
	f->nhandlers++;
	if (setjmp (h->jb) == 0) {
		m_forth_execute (f, (m_word_t *)xt);
		f->nhandlers--;
		m_forth_push (f, 0);
	} else {
		f->nhandlers = idx;
		f->dstack->len = dsp;
		f->rstack->len = rsp;
		while (da_len (f->call) > csp)
			free (da_pop (f->call));
		f->loop_action = 0;
		m_forth_push (f, f->throw_code);
	}
}

/* --- strings in source ------------------------------------------------- */

static void w_squote (m_forth_t *f)
{
	char *text = tok_parse_string (&f->tok, '"');
	if (f->compiling) {
		int addr, len;
		if (store_string (f, text, &addr, &len) != 0)
			mf_throw (f, -8, "out of memory");
		da_push (f->current_def, MF_LIT (addr));
		da_push (f->current_def, MF_LIT (len));
		da_push (f->current_def, m_forth_lookup (f, "(S\")"));
	} else {
		uint32_t id = m_forth_intern (f, text);
		if (!id)
			mf_throw (f, -8, "string intern failed");
		m_forth_push (f, id);
	}
}
static void w_zquote (m_forth_t *f)
{
	char *text = tok_parse_string (&f->tok, '"');
	if (f->compiling) {
		int addr, len;
		if (store_string (f, text, &addr, &len) != 0)
			mf_throw (f, -8, "out of memory");
		da_push (f->current_def, MF_LIT (addr));
		da_push (f->current_def, MF_LIT (len));
		da_push (f->current_def, m_forth_lookup (f, "(Z\")"));
	} else {
		uint32_t id = m_forth_intern (f, text);
		if (!id)
			mf_throw (f, -8, "string intern failed");
		m_forth_push (f, id);
	}
}
static void w_dotquote (m_forth_t *f)
{
	char *text = tok_parse_string (&f->tok, '"');
	if (f->compiling) {
		int addr, len;
		if (store_string (f, text, &addr, &len) != 0)
			mf_throw (f, -8, "out of memory");
		da_push (f->current_def, MF_LIT (addr));
		da_push (f->current_def, MF_LIT (len));
		da_push (f->current_def, m_forth_lookup (f, "TYPE"));
	} else {
		mf_msg (f, text);
	}
}

/* --- wordlists --------------------------------------------------------- */

static void p_wordlist (m_forth_t *f)
{
	m_forth_push (f, m_forth_wordlist (f));
}
static void p_get_current (m_forth_t *f) { m_forth_push (f, f->cur); }
static void p_set_current (m_forth_t *f)
{
	cell_t wid = m_forth_pop (f);
	if (wid < 0 || wid >= f->nwl)
		mf_throw (f, -1, "invalid wordlist");
	f->cur = (int)wid;
}
static void p_get_order (m_forth_t *f)
{
	for (int i = f->norder - 1; i >= 0; i--)
		m_forth_push (f, f->order[i]);
	m_forth_push (f, f->norder);
}
static void p_set_order (m_forth_t *f)
{
	cell_t n = m_forth_pop (f);
	if (n < 0 || n > MF_MAX_ORDER)
		mf_throw (f, -1, "invalid search order");
	for (int i = 0; i < (int)n; i++) {
		cell_t wid = m_forth_pop (f);
		if (wid < 0 || wid >= f->nwl)
			mf_throw (f, -1, "invalid wordlist in order");
		f->order[i] = (int)wid;
	}
	f->norder = (int)n;
}

/* --- text interpreter -------------------------------------------------- */

static void process_token (m_forth_t *f, const char *token)
{
	m_word_t *w = m_forth_lookup (f, token);
	if (w) {
		if (f->compiling && !w->immediate)
			da_push (f->current_def, w);
		else
			m_forth_execute (f, w);
		return;
	}
	cell_t val;
	if (parse_number (token, &val)) {
		if (f->compiling)
			da_push (f->current_def, MF_LIT (val));
		else
			m_forth_push (f, val);
		return;
	}
	mf_throw (f, -13, "undefined word");
}

int m_forth_eval_line (m_forth_t *f, const char *line)
{
	if (!f || !line)
		return -1;
	tok_free (&f->tok);
	if (tok_init (&f->tok, line) != 0) {
		f->error = -8;
		snprintf (f->error_msg, sizeof f->error_msg, "out of memory");
		return -8;
	}
	while (tok_has_more (&f->tok) && !f->aborted) {
		char *token = tok_next (&f->tok);
		if (!token)
			break;
		process_token (f, token);
	}
	return f->error;
}

int m_forth_eval (m_forth_t *f, const char *source)
{
	if (!f || !source)
		return -1;
	f->error = 0;
	f->error_msg[0] = '\0';
	f->aborted = 0;
	f->throw_code = 0;

	int idx = f->nhandlers;
	if (idx >= MF_MAX_HANDLERS) {
		f->error = -1;
		snprintf (f->error_msg, sizeof f->error_msg,
			  "handler overflow");
		return f->error;
	}
	mf_handler_t *h = &f->handlers[idx];
	h->dsp = ca_len (f->dstack);
	h->rsp = ca_len (f->rstack);
	h->csp = da_len (f->call);
	f->nhandlers++;
	if (setjmp (h->jb) != 0) {
		f->nhandlers = idx;
		free (f->eval_buf);
		f->eval_buf = NULL;
		f->error = f->throw_code;
		f->aborted = 0;
		return f->error;
	}

	const char *p = source;
	while (*p && !f->aborted) {
		const char *start = p;
		while (*p && *p != '\n')
			p++;
		int len = (int)(p - start);
		char *line = malloc ((size_t)len + 1);
		if (!line) {
			f->nhandlers = idx;
			f->error = -8;
			return -8;
		}
		memcpy (line, start, (size_t)len);
		line[len] = '\0';
		while (len > 0 && line[len - 1] == '\r')
			line[--len] = '\0';
		f->eval_buf = line;
		int rc = m_forth_eval_line (f, line);
		free (line);
		f->eval_buf = NULL;
		if (rc != 0) {
			f->nhandlers = idx;
			return rc;
		}
		if (*p == '\n')
			p++;
	}
	f->nhandlers = idx;
	return f->error;
}

const char *m_forth_output (const m_forth_t *f)
{
	return f ? sb_cstr (f->output) : "";
}

void m_forth_output_clear (m_forth_t *f)
{
	if (f)
		sb_clear (f->output);
}

int m_forth_error (const m_forth_t *f) { return f ? f->error : -1; }

const char *m_forth_error_msg (const m_forth_t *f)
{
	return f ? f->error_msg : "";
}

/* --- VM lifetime ------------------------------------------------------- */

static void def_internal (m_forth_t *f, const char *name, m_forth_code_t code,
			  int imm)
{
	m_forth_define (f, name, code, imm);
}

m_forth_t *m_forth_new (void)
{
	m_forth_t *f = calloc (1, sizeof *f);
	if (!f)
		return NULL;
	f->dstack = ca_new ();
	f->rstack = ca_new ();
	f->call = da_new ();
	f->arena = ar_new (65536);
	f->output = sb_new ();
	f->cf_stack = ca_new ();
	f->all_words = da_new ();
	f->strings = strtab_new ();
	f->nwl = 0;
	f->cur = 0;
	f->norder = 1;
	f->order[0] = 0;
	if (tok_init (&f->tok, "") != 0)
		goto fail;
	if (!f->dstack || !f->rstack || !f->call || !f->arena || !f->output ||
	    !f->cf_stack || !f->all_words || !f->strings)
		goto fail;
	if (wordlist_new (f) < 0)
		goto fail;

	/* special threaded words (code == NULL, handled by the interpreter) */
	def_internal (f, "EXIT", NULL, 0);
	def_internal (f, "BRANCH", NULL, 0);
	def_internal (f, "?BRANCH", NULL, 0);
	def_internal (f, "DOES>", NULL, 1);
	def_internal (f, "(COMPILE)", p_compile, 0);
	def_internal (f, "(IS)", p_is, 0);
	def_internal (f, "(DO)", p_do, 0);
	def_internal (f, "(LOOP)", p_loop, 0);
	def_internal (f, "(+LOOP)", p_ploop, 0);
	def_internal (f, "(S\")", p_sintern, 0);
	def_internal (f, "(Z\")", p_sintern, 0);
	f->w_exit = m_forth_lookup (f, "EXIT");
	f->w_branch = m_forth_lookup (f, "BRANCH");
	f->w_qbranch = m_forth_lookup (f, "?BRANCH");
	f->w_does = m_forth_lookup (f, "DOES>");
	f->w_compile = m_forth_lookup (f, "(COMPILE)");
	f->w_is = m_forth_lookup (f, "(IS)");
	f->w_loop = m_forth_lookup (f, "(LOOP)");
	f->w_ploop = m_forth_lookup (f, "(+LOOP)");

	/* stack */
	def_internal (f, "DUP", p_dup, 0);
	def_internal (f, "DROP", p_drop, 0);
	def_internal (f, "SWAP", p_swap, 0);
	def_internal (f, "OVER", p_over, 0);
	def_internal (f, "ROT", p_rot, 0);
	def_internal (f, "NIP", p_nip, 0);
	def_internal (f, "TUCK", p_tuck, 0);
	def_internal (f, "2DROP", p_2drop, 0);
	def_internal (f, "2DUP", p_2dup, 0);
	def_internal (f, "2SWAP", p_2swap, 0);
	def_internal (f, "2OVER", p_2over, 0);
	def_internal (f, "?DUP", p_qdup, 0);
	def_internal (f, "DEPTH", p_depth, 0);

	/* arithmetic */
	def_internal (f, "+", p_add, 0);
	def_internal (f, "-", p_sub, 0);
	def_internal (f, "*", p_mul, 0);
	def_internal (f, "/", p_div, 0);
	def_internal (f, "MOD", p_mod, 0);
	def_internal (f, "/MOD", p_dm, 0);
	def_internal (f, "1+", p_1p, 0);
	def_internal (f, "1-", p_1m, 0);
	def_internal (f, "2+", p_2p, 0);
	def_internal (f, "2-", p_2m, 0);
	def_internal (f, "ABS", p_abs, 0);
	def_internal (f, "NEGATE", p_negate, 0);

	/* bitwise / compare */
	def_internal (f, "AND", p_and, 0);
	def_internal (f, "OR", p_or, 0);
	def_internal (f, "XOR", p_xor, 0);
	def_internal (f, "INVERT", p_invert, 0);
	def_internal (f, "=", p_eq, 0);
	def_internal (f, "<>", p_ne, 0);
	def_internal (f, "<", p_lt, 0);
	def_internal (f, ">", p_gt, 0);
	def_internal (f, "<=", p_le, 0);
	def_internal (f, ">=", p_ge, 0);
	def_internal (f, "0=", p_0eq, 0);
	def_internal (f, "0<", p_0lt, 0);
	def_internal (f, "0>", p_0gt, 0);

	/* memory */
	def_internal (f, "@", p_fetch, 0);
	def_internal (f, "!", p_store, 0);
	def_internal (f, "C@", p_cfetch, 0);
	def_internal (f, "C!", p_cstore, 0);
	def_internal (f, "HERE", p_here, 0);
	def_internal (f, "ALLOT", p_allot, 0);
	def_internal (f, ",", p_comma, 0);
	def_internal (f, "+!", p_plus_store, 0);
	def_internal (f, "PAD", p_pad, 0);

	/* return stack / output */
	def_internal (f, ">R", p_tor, 0);
	def_internal (f, "R>", p_rfrom, 0);
	def_internal (f, "R@", p_rfetch, 0);
	def_internal (f, "EMIT", p_emit, 0);
	def_internal (f, "CR", p_cr, 0);
	def_internal (f, "SPACE", p_space, 0);
	def_internal (f, "SPACES", p_spaces, 0);
	def_internal (f, "TYPE", p_type, 0);
	def_internal (f, "COUNT", p_count, 0);

	/* execution / numbers */
	def_internal (f, "EXECUTE", p_execute, 0);
	def_internal (f, "BL", p_bl, 0);
	def_internal (f, "TRUE", p_true, 0);
	def_internal (f, "FALSE", p_false, 0);
	def_internal (f, "CELL", p_cell, 0);
	def_internal (f, "CELL+", p_cellplus, 0);
	def_internal (f, ".", p_dot, 0);
	def_internal (f, "U.", p_udot, 0);
	def_internal (f, "S.", p_sdot, 0);
	def_internal (f, "Z.", p_sdot, 0);

	/* strings */
	def_internal (f, "S\"", w_squote, 1);
	def_internal (f, "Z\"", w_zquote, 1);
	def_internal (f, ".\"", w_dotquote, 1);

	/* compiler */
	def_internal (f, ":", w_colon, 1);
	def_internal (f, ":NONAME", w_noname, 1);
	def_internal (f, ";", w_semicolon, 1);
	def_internal (f, "IMMEDIATE", w_immediate, 0);
	def_internal (f, "IF", w_if, 1);
	def_internal (f, "ELSE", w_else, 1);
	def_internal (f, "THEN", w_then, 1);
	def_internal (f, "BEGIN", w_begin, 1);
	def_internal (f, "UNTIL", w_until, 1);
	def_internal (f, "AGAIN", w_again, 1);
	def_internal (f, "WHILE", w_while, 1);
	def_internal (f, "REPEAT", w_repeat, 1);
	def_internal (f, "DO", w_do, 1);
	def_internal (f, "LOOP", w_loop, 1);
	def_internal (f, "+LOOP", w_ploop, 1);
	def_internal (f, "I", p_i, 0);
	def_internal (f, "J", p_j, 0);
	def_internal (f, "LEAVE", p_leave, 0);
	def_internal (f, "UNLOOP", p_unloop, 0);
	def_internal (f, "LITERAL", w_literal, 1);
	def_internal (f, "RECURSE", w_recurse, 1);
	def_internal (f, "POSTPONE", w_postpone, 1);
	def_internal (f, "SEE", w_see, 0);
	def_internal (f, "[", w_bracket, 1);
	def_internal (f, "]", w_bracket_end, 1);

	/* defining words */
	def_internal (f, "CREATE", w_create, 0);
	def_internal (f, "CONSTANT", w_constant, 0);
	def_internal (f, "VARIABLE", w_variable, 0);
	def_internal (f, "'", w_tick, 0);
	def_internal (f, "DEFER", w_defer, 0);
	def_internal (f, "IS", w_is, 1);

	/* exceptions */
	def_internal (f, "THROW", p_throw, 0);
	def_internal (f, "ABORT", p_abort, 0);
	def_internal (f, "CATCH", p_catch, 0);

	/* wordlists */
	def_internal (f, "WORDLIST", p_wordlist, 0);
	def_internal (f, "GET-CURRENT", p_get_current, 0);
	def_internal (f, "SET-CURRENT", p_set_current, 0);
	def_internal (f, "GET-ORDER", p_get_order, 0);
	def_internal (f, "SET-ORDER", p_set_order, 0);

	f->pad = ar_alloc (f->arena, 256);
	if (f->pad < 0)
		goto fail;
	return f;

fail:
	m_forth_free (f);
	return NULL;
}

void m_forth_free (m_forth_t *f)
{
	if (!f)
		return;
	tok_free (&f->tok);
	if (f->all_words) {
		for (int i = 0; i < da_len (f->all_words); i++) {
			m_word_t *w = da_get (f->all_words, i);
			if (!w)
				continue;
			free (w->name);
			da_free (w->def);
			free (w);
		}
		da_free (f->all_words);
	}
	for (int i = 0; i < f->nwl; i++)
		ht_free (f->wl[i]);
	for (int i = 0; i < da_len (f->call); i++)
		free (da_get (f->call, i));
	da_free (f->call);
	free (f->eval_buf);
	da_free (f->current_def);
	ca_free (f->cf_stack);
	ca_free (f->dstack);
	ca_free (f->rstack);
	ar_free (f->arena);
	sb_free (f->output);
	strtab_free (f->strings);
	free (f);
}
