#ifndef M_FORTH_H
#define M_FORTH_H

#include <stdint.h>

#include "arena.h"
#include "carray.h"
#include "darray.h"
#include "htable.h"
#include "strbuf.h"
#include "token.h"

/* Re-entrant, DTC-style Forth VM in portable C. No stdout: all output goes to
   an internal buffer (m_forth_output). Primitives are C functions taking the
   VM context, so multiple VMs can coexist (NFR-05). */
typedef struct m_forth m_forth_t;
typedef struct m_word m_word_t;
typedef void (*m_forth_code_t) (m_forth_t *f);

m_forth_t *m_forth_new (void);
void m_forth_free (m_forth_t *f);

/* 0 = success, negative ANS throw code on error. */
int m_forth_eval (m_forth_t *f, const char *source);
int m_forth_eval_line (m_forth_t *f, const char *line);

const char *m_forth_output (const m_forth_t *f);
void m_forth_output_clear (m_forth_t *f);
int m_forth_error (const m_forth_t *f);
const char *m_forth_error_msg (const m_forth_t *f);

void m_forth_push (m_forth_t *f, cell_t v);
cell_t m_forth_pop (m_forth_t *f);
cell_t m_forth_peek (m_forth_t *f, int n);

void m_forth_define (m_forth_t *f, const char *name, m_forth_code_t code,
		     int immediate);
m_word_t *m_forth_lookup (m_forth_t *f, const char *name);
void m_forth_execute (m_forth_t *f, m_word_t *w);

void m_forth_set_user (m_forth_t *f, void *user);
void *m_forth_user (const m_forth_t *f);
void m_forth_raise (m_forth_t *f, int code, const char *msg);

/* String literals (S") live in a VM-local table (IDs start at 1). */
uint32_t m_forth_intern (m_forth_t *f, const char *s);
const char *m_forth_resolve (const m_forth_t *f, uint32_t id);

/* Wordlists / search order for per-table overrides (FR-28). */
int m_forth_wordlist (m_forth_t *f); /* new wid */
int m_forth_get_current (const m_forth_t *f);
int m_forth_set_current (m_forth_t *f, int wid);
int m_forth_set_order (m_forth_t *f, const int *wids, int n);
int m_forth_get_order (const m_forth_t *f, int *wids, int cap);

#endif
