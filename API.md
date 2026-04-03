# Project API Documentation

## File: `mls.c`

### `deb_err`

**Signature:**
```c
void deb_err(int line, const char *file, const char *function,
	      const char *format, ...)
```

Prints an error message with file and line information and terminates the program.
Sets the error_occurred flag for post-mortem analysis.

**Parameters:**
- line The line number where the error occurred.
- file The source file where the error occurred.
- function The function where the error occurred.
- format The format string for the error message.

---

### `deb_warn`

**Signature:**
```c
void deb_warn(int line, const char *file, const char *function,
	       const char *format, ...)
```

Prints a warning message with file and line information.

**Parameters:**
- line The line number where the warning occurred.
- file The source file where the warning occurred.
- function The function where the warning occurred.
- format The format string for the warning message.

---

### `deb_trace`

**Signature:**
```c
void deb_trace(int l, int line, const char *file, const char *function,
		const char *format, ...)
```

Prints a trace message if the provided level is greater than or equal to trace_level.

**Parameters:**
- l The trace level of this message.
- line The line number where the trace occurred.
- file The source file where the trace occurred.
- function The function where the trace occurred.
- format The format string for the trace message.

---

### `print_stacksize`

**Signature:**
```c
int print_stacksize()
```

Returns the current size of the master list (stack of allocated handles).

**Returns:** The number of handles in the master list.

---

### `_mlsdb_caller`

**Signature:**
```c
static void _mlsdb_caller(const char *me, int ln, const char *fn,
			   const char *fun, int args, int handle, int index,
			   const void *data)
```

Returns a pointer to the element at the specified index in a list structure.

/
void *lst (lst_t l, int i) { return &l->d[l->w * i]; }

struct lst_owner_st {
int allocated;
const char *fn, *fun;
int ln;
};
typedef struct lst_owner_st lst_owner;

static int DEB = 0; // debug list

struct debug_info_st {
char msg[500];
const char *me, *fn, *fun;
int ln, args, handle, index;
const void *data;
};

static struct debug_info_st debi;

/**
Records caller information for the current MLS operation.
Used by debug wrappers to provide context for error messages.

**Parameters:**
- l The list structure.
- i The index of the element.
- me The name of the function being wrapped.
- ln The line number of the call.
- fn The filename of the call.
- fun The function name of the caller.
- args Bitmask indicating which arguments are valid (1:handle, 2:index, 4:data).
- handle The MLS handle involved in the operation.
- index The index involved in the operation.
- data Pointer to data involved in the operation.

**Returns:** A pointer to the element data.

---

### `perr`

**Signature:**
```c
static void perr(const char *format, ...)
```

Prints a formatted error message to stderr, followed by a newline.

**Parameters:**
- format The format string.

---

### `_mlsdb_check_handle`

**Signature:**
```c
static int _mlsdb_check_handle()
```

Validates the handle stored in the current debug info.
Prints detailed information about the handle's state and allocation source.

**Returns:** 0 if the handle is valid, -1 otherwise.

---

### `_mlsdb_check_index`

**Signature:**
```c
static int _mlsdb_check_index()
```

Validates the index stored in the current debug info against its handle.
Prints an error message if the index is out of bounds.

**Returns:** 0 if the index is valid, -1 otherwise.

---

### `exit_error`

**Signature:**
```c
void exit_error()
```

Performs a post-mortem analysis of the last recorded MLS operation.
Registered as an atexit handler. Only runs if an error was detected.

---

### `lst_resize`

**Signature:**
```c
void lst_resize(lst_t *lp, int new_size)
```

Resizes a list structure to a new maximum size.

**Parameters:**
- lp Pointer to the list structure pointer.
- new_size The new maximum number of elements.

---

### `lst_create`

**Signature:**
```c
lst_t lst_create(int max, int w)
```

Creates a new list structure.

**Parameters:**
- max The initial maximum number of elements.
- w The width of each element in bytes.

**Returns:** The newly created list structure.

---

### `lst_new`

**Signature:**
```c
int lst_new(lst_t *lp, int n)
```

Reserves space for n new elements in a list.

**Parameters:**
- lp Pointer to the list structure pointer.
- n The number of elements to reserve.

**Returns:** The index of the first newly reserved element.

---

### `lst_put`

**Signature:**
```c
int lst_put(lst_t *lp, const void *d)
```

Appends an element to a list.

**Parameters:**
- lp Pointer to the list structure pointer.
- d Pointer to the element data to append.

**Returns:** The index of the appended element.

---

### `lst_del`

**Signature:**
```c
void lst_del(lst_t l, int p)
```

Returns a pointer to the element at the specified index without bounds checking.

/
void *lst_peek (lst_t l, int i)
{
if (i >= l->l || i < 0)
return NULL;
return lst (l, i);
}

/**
Deletes an element at the specified index from a list.

**Parameters:**
- l The list structure.
- i The index of the element.
- l The list structure.
- p The index of the element to delete.

**Returns:** A pointer to the element data, or NULL if index is out of bounds.

---

### `lst_remove`

**Signature:**
```c
void lst_remove(lst_t *lp, int p, int n)
```

Removes n elements starting from the specified index from a list.

**Parameters:**
- lp Pointer to the list structure pointer.
- p The starting index.
- n The number of elements to remove.

---

### `lst_next`

**Signature:**
```c
int lst_next(lst_t l, int *p, void *data)
```

Inserts n empty elements at the specified index in a list.

/
void *lst_ins (lst_t *lp, int p, int n)
{
int cnt;
if (p < 0 || p > (**lp).l)
return 0;
cnt = (**lp).l - p;
lst_new (lp, n);
if (cnt > 0)
memmove (lst (*lp, p + n), lst (*lp, p), cnt * (**lp).w);
memset (lst (*lp, p), 0, n * (**lp).w);
return lst (*lp, p);
}

/**
Iterates to the next element in a list.

**Parameters:**
- lp Pointer to the list structure pointer.
- p The insertion index.
- n The number of elements to insert.
- l The list structure.
- p Pointer to the current index. Should be initialized to -1.
- data Pointer to store the address of the next element.

**Returns:** 1 if an element was found, 0 otherwise.

---

### `lst_read`

**Signature:**
```c
int lst_read(lst_t l, int p, void **data, int n)
```

Reads n elements from a list into a buffer.

**Parameters:**
- l The list structure.
- p The starting index.
- data Pointer to the destination buffer pointer. If *data is 0, a buffer is allocated.
- n The number of elements to read.

**Returns:** 0 on success.

---

### `lst_write`

**Signature:**
```c
int lst_write(lst_t *lp, int p, const void *data, int n)
```

Writes n elements from a buffer into a list at the specified index.

**Parameters:**
- lp Pointer to the list structure pointer.
- p The starting index.
- data Pointer to the source data buffer.
- n The number of elements to write.

**Returns:** 0 on success.

---

### `get_list`

**Signature:**
```c
static inline lst_t * get_list(int m)
```

Internal function to retrieve a pointer to the list structure associated with a handle.
Performs validation checks for handle range, existence, and UAF protection.

**Parameters:**
- m The handle to look up.

**Returns:** A pointer to the list structure pointer in the master list.

---

### `free_strings_wrap`

**Signature:**
```c
static void free_strings_wrap(int h)
```

Public accessor for getting a list pointer from a handle.

/
lst_t *exported_get_list (int r) { return get_list (r); }

extern void m_free_strings (int list, int CLEAR_ONLY);

/**
same as m_free_strings to be used as a custom free handler.

**Parameters:**
- r The handle.
- l The list structure being freed.

**Returns:** A pointer to the list structure pointer.

---

### `free_list_wrap`

**Signature:**
```c
static void free_list_wrap(int h)
```

Custom free handler that recursively frees MLS handles stored in a list.

**Parameters:**
- l The list structure being freed.

---

### `m_init`

**Signature:**
```c
int m_init()
```

Initializes the MLS library system.
Allocates the master handle list and registers default free handlers.

**Returns:** 0 on success, 1 if already initialized.

---

### `m_destruct`

**Signature:**
```c
void m_destruct()
```

Destroys the MLS library system.
Explicitly frees all remaining allocated handles and their contents.

---

### `m_alloc`

**Signature:**
```c
int m_alloc(int max, int w, uint8_t hfree)
```

Allocates a new MLS handle with a specific free handler.

**Parameters:**
- max Initial maximum elements.
- w Width of each element in bytes.
- hfree The ID of the registered free handler to use.

**Returns:** A new 1-based MLS handle with UAF protection pattern.

---

### `m_create`

**Signature:**
```c
int m_create(int max, int w)
```

Creates a new MLS handle with the default free handler (MFREE).

**Parameters:**
- max Initial maximum elements.
- w Width of each element.

**Returns:** A new handle.

---

### `m_free`

**Signature:**
```c
int m_free(int m)
```

Frees an MLS handle and its associated list data.
The handle is marked as freed and returned to the free list for reuse.

**Parameters:**
- m The handle to free.

**Returns:** 0 on success.

---

### `m_reg_freefn`

**Signature:**
```c
int m_reg_freefn(free_fn_t  free_fn)
```

@brief Registers a  a cleanup callback for managed arrays
The @p free_fn is triggered automatically when m_free() is called.
This allows for custom iteration and deallocation of array elements
before the array container itself is removed.

**Parameters:**
- free_fn The function to be called when freeing handles with this handler ID.

**Returns:** The handler ID on success, -1 if handler is invalid or too many handlers registered.

---

### `m_is_freed`

**Signature:**
```c
int m_is_freed(int h)
```

Checks if an MLS handle is valid and hasn't been freed.

**Parameters:**
- h The handle to check.

**Returns:** 1 if the handle is freed or invalid, 0 if it is active.

---

### `m_free_hdl`

**Signature:**
```c
int m_free_hdl(int h)
```

Returns the free handler ID associated with an MLS handle.

**Parameters:**
- h The handle.

**Returns:** The free handler ID.

---

### `m_len`

**Signature:**
```c
int m_len(int m)
```

Returns the number of elements in the list associated with a handle.

**Parameters:**
- m The handle.

**Returns:** The number of elements.

---

### `m_new`

**Signature:**
```c
int m_new(int m, int n)
```

Returns a pointer to the raw data buffer of the list associated with a handle.

/
void *m_buf (int m)
{
return m_peek(m, 0);
}

/**
Returns a pointer to the element at the specified index with bounds checking.

/
void *mls (int m, int i)
{
lst_t *lp = get_list (m);
void *res = lst_peek (*lp, i);
if (!res)
ERR ("Index %d out of bounds for handle %d (len %d)", i, m,
(**lp).l);
return res;
}

/**
Reserves space for n new elements in a handle's list.

**Parameters:**
- m The handle.
- m The handle.
- i The index.
- m The handle.
- n The number of elements to reserve.

**Returns:** The index of the first newly reserved element.

---

### `m_next`

**Signature:**
```c
int m_next(int m, int *p, void *d)
```

Appends one new element to a handle's list and returns a pointer to it.

/
void *m_add (int m)
{
int p = m_new (m, 1);
return mls (m, p);
}

/**
Iterates through the elements of a handle's list.

**Parameters:**
- m The handle.
- m The handle.
- p Pointer to the current index.
- d Pointer to store the address of the next element.

**Returns:** 1 if an element was found, 0 otherwise.

---

### `m_put`

**Signature:**
```c
int m_put(int m, const void *data)
```

Appends an element to a handle's list.

**Parameters:**
- m The handle.
- data Pointer to the element data to append.

**Returns:** The index of the appended element.

---

### `m_setlen`

**Signature:**
```c
int m_setlen(int m, int len)
```

Sets the logical length of a handle's list.

**Parameters:**
- m The handle.
- len The new length.

**Returns:** 0 on success.

---

### `m_bufsize`

**Signature:**
```c
int m_bufsize(int m)
```

Returns the currently allocated capacity (buffer size) of a handle's list.

**Parameters:**
- m The handle.

**Returns:** The maximum number of elements before a realloc is needed.

---

### `m_write`

**Signature:**
```c
int m_write(int m, int p, const void *data, int n)
```

Returns a pointer to the element at the specified index without bounds checking.

/
void *m_peek (int m, int i)
{
lst_t *lp = get_list (m);
return lst_peek (*lp, i);
}

/**
Writes data to a handle's list starting at a specific index.
Resizes the list if necessary.

**Parameters:**
- m The handle.
- i The index.
- m The handle.
- p The starting index.
- data Pointer to the source data.
- n The number of elements to write.

**Returns:** 0 on success.

---

### `m_read`

**Signature:**
```c
int m_read(int h, int p, void **data, int n)
```

Reads data from a handle's list into a buffer.

**Parameters:**
- h The handle.
- p The starting index.
- data Pointer to the destination buffer pointer.
- n The number of elements to read.

**Returns:** 0 on success.

---

### `m_clear`

**Signature:**
```c
void m_clear(int m)
```

Sets the logical length of a handle's list to zero.

**Parameters:**
- m The handle.

---

### `m_del`

**Signature:**
```c
void m_del(int m, int p)
```

Deletes an element at the specified index from a handle's list.
Remaining elements are shifted to close the gap.

**Parameters:**
- m The handle.
- p The index of the element to delete.

---

### `m_ins`

**Signature:**
```c
int m_ins(int m, int p, int n)
```

Removes and returns a pointer to the last element of a handle's list.

/
void *m_pop (int m)
{
lst_t *lp = get_list (m);
if ((**lp).l == 0)
return NULL;
(**lp).l--;
return lst (*lp, (**lp).l);
}

/**
Inserts n empty (zero-initialized) elements at a specific index in a handle's list.

**Parameters:**
- m The handle.
- m The handle.
- p The insertion index.
- n The number of elements to insert.

**Returns:** 1 on success, 0 on failure.

---

### `m_width`

**Signature:**
```c
int m_width(int m)
```

Returns the width of each element in a handle's list in bytes.

**Parameters:**
- m The handle.

**Returns:** The element width.

---

### `m_resize`

**Signature:**
```c
void m_resize(int m, int new_size)
```

Resizes the allocated capacity of a handle's list.

**Parameters:**
- m The handle.
- new_size The new maximum number of elements.

---

### `m_slice`

**Signature:**
```c
int m_slice(int dest, int offs, int m, int a, int b)
```

Extracts a sub-range of elements from one list and appends or writes them into another.
Supports negative indices (relative to end of list).

**Parameters:**
- dest The destination handle. If <= 0, a new handle is created.
- offs The offset in the destination list to start writing.
- m The source handle.
- a The starting index in the source list.
- b The ending index in the source list (inclusive).

**Returns:** The destination handle.

---

### `m_remove`

**Signature:**
```c
void m_remove(int m, int p, int n)
```

Removes n elements starting from the specified index from a handle's list.
Remaining elements are shifted to close the gap.

**Parameters:**
- m The handle.
- p The starting index.
- n The number of elements to remove.

---

### `m_bzero`

**Signature:**
```c
void m_bzero(int m)
```

Zeroes out the entire data buffer of a handle's list.

**Parameters:**
- m The handle.

---

### `m_fscan2`

**Signature:**
```c
int m_fscan2(int m, char delim, FILE *fp)
```

Scans a file until a delimiter character is encountered or EOF is reached.
Appends the scanned characters to a handle's list.

**Parameters:**
- m The handle.
- delim The delimiter character.
- fp The file pointer to read from.

**Returns:** The delimiter character read, or EOF.

---

### `m_fscan`

**Signature:**
```c
int m_fscan(int m, char delim, FILE *fp)
```

Similar to m_fscan2, but returns the length of the data scanned.

**Parameters:**
- m The handle.
- delim The delimiter character.
- fp The file pointer.

**Returns:** The number of characters scanned, or EOF if no characters were read before EOF.

---

### `m_cmp`

**Signature:**
```c
int m_cmp(int a, int b)
```

Compares two handle's lists element-by-element using memcmp.

**Parameters:**
- a The first handle.
- b The second handle.

**Returns:** <0 if a < b, 0 if a == b, >0 if a > b.

---

### `m_lookup`

**Signature:**
```c
int m_lookup(int m, int key)
```

Looks up a key (represented by a handle) in a list of handles.
If not found, the key is appended to the list.

**Parameters:**
- m The handle of the list to search.
- key The handle to look for.

**Returns:** The handle found or inserted.

---

### `m_lookup_obj`

**Signature:**
```c
int m_lookup_obj(int m, void *obj, int size)
```

Looks up an object of fixed size in a list.
If not found, the object is appended.

**Parameters:**
- m The handle of the list to search.
- obj Pointer to the object to look for.
- size The size of the object in bytes.

**Returns:** The index of the object.

---

### `m_lookup_str`

**Signature:**
```c
int m_lookup_str(int m, const char *key, int NOT_INSERT)
```

Looks up a string in a list of strings (char *).
If not found and NOT_INSERT is 0, the string is duplicated and appended.

**Parameters:**
- m The handle of the string list.
- key The string to look for.
- NOT_INSERT If non-zero, do not insert the string if not found.

**Returns:** The index of the string, or -1 if not found and NOT_INSERT is set.

---

### `m_putc`

**Signature:**
```c
int m_putc(int m, char c)
```

Appends a single character to a handle's list.

**Parameters:**
- m The handle.
- c The character to append.

**Returns:** The character appended.

---

### `m_puti`

**Signature:**
```c
int m_puti(int m, int c)
```

Appends a single integer to a handle's list.

**Parameters:**
- m The handle.
- c The integer to append.

**Returns:** The integer appended.

---

### `m_utf8char`

**Signature:**
```c
int m_utf8char(int buf, int *p)
```

Decodes a UTF-8 character from a handle's list starting at index p.
Increments p by the number of bytes consumed.

**Parameters:**
- buf The handle of the buffer.
- p Pointer to the current index.

**Returns:** The decoded Unicode character point, or -1 on error/EOF.

---

### `utf8char`

**Signature:**
```c
int utf8char(char **s)
```

Decodes a UTF-8 character from a string pointer.
Increments the pointer by the number of bytes consumed.

**Parameters:**
- s Pointer to a string pointer.

**Returns:** The decoded Unicode character point, or -1 on error/EOF.

---

### `utf8_getchar`

**Signature:**
```c
int utf8_getchar(FILE *fp, utf8_char_t buf)
```

Reads a single UTF-8 character from a file pointer.

**Parameters:**
- fp The file pointer.
- buf Buffer to store the UTF-8 byte sequence (null-terminated if len < 6).

**Returns:** The number of bytes in the UTF-8 character, or EOF.

---

### `cmp_int`

**Signature:**
```c
int cmp_int(const void *a0, const void *b0)
```

Comparison function for integers, suitable for qsort or binary search.

**Parameters:**
- a0 Pointer to first integer.
- b0 Pointer to second integer.

**Returns:** a0 - b0.

---

### `m_blookup_int`

**Signature:**
```c
int m_blookup_int(int buf, int key, void (*new) (void *, void *), void *ctx)
```

Looks up an integer in a sorted list using binary search and inserts it if not found.

**Parameters:**
- buf The handle of the sorted list.
- key The integer to look for.
- new Optional callback function called when a new element is inserted.
- ctx Context pointer for the callback.

**Returns:** The index of the integer in the list.

---

### `m_binsert_int`

**Signature:**
```c
int m_binsert_int(int buf, int key)
```

Similar to m_blookup_int, but returns a pointer to the element.

/
void *m_blookup_int_p (int buf, int key, void (*new) (void *, void *),
void *ctx)
{
return mls (buf, m_blookup_int (buf, key, new, ctx));
}

/**
Inserts an integer into a sorted list using binary search.

**Parameters:**
- buf The handle of the sorted list.
- key The integer to look for.
- new Optional callback function.
- ctx Context pointer.
- buf The handle of the sorted list.
- key The integer to insert.

**Returns:** The index where the integer was inserted or found.

---

### `m_bsearch_int`

**Signature:**
```c
int m_bsearch_int(int buf, int key)
```

Searches for an integer in a sorted list using binary search.

**Parameters:**
- buf The handle of the sorted list.
- key The integer to search for.

**Returns:** The index of the integer, or -1 if not found.

---

### `_m_init`

**Signature:**
```c
int _m_init()
```

Internal debug version of m_init.
Initializes the master list and the debug ownership list.

**Returns:** 0 on success, 1 if already initialized.

---

### `_m_destruct`

**Signature:**
```c
void _m_destruct()
```

Internal debug version of m_destruct.
Checks for leaked lists and destroys all resources.

---

### `_m_create`

**Signature:**
```c
int _m_create(int ln, const char *fn, const char *fun, int n, int w)
```

Internal debug version of m_create.
Records caller information and allocation source.

**Parameters:**
- ln Caller line number.
- fn Caller filename.
- fun Caller function name.
- n Initial maximum elements.
- w Element width.

**Returns:** The new handle.

---

### `_m_free`

**Signature:**
```c
int _m_free(int ln, const char *fn, const char *fun, int m)
```

Internal debug version of m_free.
Records caller information and marks the list as freed in the debug tracker.

**Parameters:**
- ln Caller line number.
- fn Caller filename.
- fun Caller function name.
- m The handle to free.

**Returns:** 0.

---

### `_m_put`

**Signature:**
```c
int _m_put(int ln, const char *fn, const char *fun, int h, const void *d)
```

Internal debug version of mls.
Records caller information and bounds checks the access.

/
void *_mls (int ln, const char *fn, const char *fun, int h, int i)
{
_mlsdb_caller (__FUNCTION__, ln, fn, fun, 3, h, i, 0);
return mls (h, i);
}

/**
Internal debug version of m_put.
Records caller information.

**Parameters:**
- ln Caller line number.
- fn Caller filename.
- fun Caller function name.
- h The handle.
- i The index.
- ln Caller line number.
- fn Caller filename.
- fun Caller function name.
- h The handle.
- d Pointer to the element data.

**Returns:** The index of the appended element.

---

### `_m_next`

**Signature:**
```c
int _m_next(int ln, const char *fn, const char *fun, int h, int *i, void *d)
```

Internal debug version of m_next.
Records caller information.

**Parameters:**
- ln Caller line number.
- fn Caller filename.
- fun Caller function name.
- h The handle.
- i Pointer to the current index.
- d Pointer to store the address of the next element.

**Returns:** 1 if an element was found, 0 otherwise.

---

### `_m_clear`

**Signature:**
```c
void _m_clear(int ln, const char *fn, const char *fun, int h)
```

Internal debug version of m_clear.
Records caller information.

**Parameters:**
- ln Caller line number.
- fn Caller filename.
- fun Caller function name.
- h The handle.

---

### `_m_alloc`

**Signature:**
```c
int _m_alloc(int ln, const char *fn, const char *fun, int n, int w,
	      uint8_t hfree)
```

Internal debug version of m_buf.
Records caller information.

/
void *_m_buf (int ln, const char *fn, const char *fun, int m)
{
if (!m)
return 0;
_mlsdb_caller (__FUNCTION__, ln, fn, fun, 3, m, 0, 0);
return m_buf (m);
}

/**
Internal debug version of m_alloc.
Records caller information and allocation source.

**Parameters:**
- ln Caller line number.
- fn Caller filename.
- fun Caller function name.
- m The handle.
- ln Caller line number.
- fn Caller filename.
- fun Caller function name.
- n Initial maximum elements.
- w Element width.
- hfree The free handler ID.

**Returns:** The new handle.

---

## File: `m_tool.c`

### `m_dub`

**Signature:**
```c
int m_dub(int m)
```

Creates a duplicate of an existing m-array.

**Parameters:**
- m The handle of the m-array to duplicate.

**Returns:** A new handle to the duplicated m-array.

---

### `m_regex`

**Signature:**
```c
int m_regex(int m, const char *regex, const char *s)
```

Executes a regular expression on a string and stores sub-matches in an m-array.

**Parameters:**
- m The handle of the m-array to store matches. If <= 1, a new one is allocated.
- regex The regular expression pattern.
- s The string to search.

**Returns:** The handle of the m-array containing the matches.

---

### `m_qsort`

**Signature:**
```c
void m_qsort(int list, int (*compar) (const void *, const void *))
```

Sorts an m-array using qsort.

**Parameters:**
- list The handle of the m-array to sort.
- compar Comparison function pointer.

---

### `m_bsearch`

**Signature:**
```c
int m_bsearch(const void *key, int list,
	       int (*compar) (const void *, const void *))
```

Performs a binary search on a sorted m-array.

**Parameters:**
- key Pointer to the element to search for.
- list The handle of the sorted m-array.
- compar Comparison function pointer.

**Returns:** The index of the element if found, or -1.

---

### `m_lfind`

**Signature:**
```c
int m_lfind(const void *key, int list,
	     int (*compar) (const void *, const void *))
```

Performs a linear search on an m-array.

**Parameters:**
- key Pointer to the element to search for.
- list The handle of the m-array.
- compar Comparison function pointer.

**Returns:** The index of the element if found, or -1.

---

### `m_binsert`

**Signature:**
```c
int m_binsert(int buf, const void *data,
	       int (*cmpf) (const void *data, const void *buf_elem),
	       int with_duplicates)
```

Inserts an element into a sorted m-array, maintaining order.

**Parameters:**
- buf The handle of the m-array.
- data Pointer to the element to insert.
- cmpf Comparison function pointer.
- with_duplicates If non-zero, allows duplicate elements.

**Returns:** The index where the element was inserted, or -index if it exists and duplicates are not allowed.

---

### `ioread_all`

**Signature:**
```c
int ioread_all(int fd, int buffer)
```

Reads all available data from a file descriptor into an m-array.

**Parameters:**
- fd The file descriptor to read from.
- buffer The handle of the m-array to store the data.

**Returns:** The number of bytes read, or a negative value on error.

---

### `s_new`

**Signature:**
```c
int s_new(void)
```

Allocates a new string buffer.

**Returns:** The handle of the new string buffer.

---

### `s_free`

**Signature:**
```c
void s_free(int h)
```

Frees a string buffer.

**Parameters:**
- h The handle of the string buffer to free.

---

### `s_strlen`

**Signature:**
```c
int s_strlen(int m)
```

Calculates the length of a string stored in an m-array.

**Parameters:**
- m The handle of the string buffer.

**Returns:** The length of the string, excluding the null terminator.

---

### `s_app1`

**Signature:**
```c
int s_app1(int m, char *s)
```

Appends a C-style string to an existing string buffer.

**Parameters:**
- m The handle of the string buffer.
- s The C-style string to append.

**Returns:** The handle of the string buffer.

---

### `vas_app`

**Signature:**
```c
static int vas_app(int m, va_list ap)
```

Internal helper to append multiple strings from a va_list.

---

### `s_app`

**Signature:**
```c
int s_app(int m, ...)
```

Appends one or more C-style strings to a string buffer.

**Parameters:**
- m The handle of the string buffer. If <= 0, a new one is allocated.
- ... Variable list of C-style strings, terminated by NULL.

**Returns:** The handle of the string buffer.

---

### `vas_printf`

**Signature:**
```c
int vas_printf(int m, int p, const char *format, va_list ap)
```

Formatted append to a string buffer using a va_list.

**Parameters:**
- m The handle of the string buffer. If <= 0, a new one is allocated.
- p The position to start writing. If < 0 or > length, it appends.
- format The format string.
- ap The va_list of arguments.

**Returns:** The handle of the string buffer.

---

### `s_printf`

**Signature:**
```c
int s_printf(int m, int p, char *format, ...)
```

Formatted append to a string buffer.

**Parameters:**
- m The handle of the string buffer.
- p The position to start writing.
- format The format string.
- ... Variable list of arguments.

**Returns:** The handle of the string buffer.

---

### `s_dup`

**Signature:**
```c
int s_dup(const char *s)
```

Creates a new string buffer from a C-style string.

**Parameters:**
- s The C-style string to copy.

**Returns:** The handle of the new string buffer.

---

### `s_clone`

**Signature:**
```c
int s_clone(int h)
```

Clones a string buffer.

**Parameters:**
- h The handle of the string buffer to clone.

**Returns:** A new handle to the cloned string buffer.

---

### `s_resize`

**Signature:**
```c
int s_resize(int h, int len)
```

Resizes a string buffer and ensures it is null-terminated.

**Parameters:**
- h The handle of the string buffer.
- len The new length.

**Returns:** The handle of the string buffer.

---

### `s_clear`

**Signature:**
```c
void s_clear(int h)
```

Clears the content of a string buffer.

**Parameters:**
- h The handle of the string buffer.

---

### `s_has_prefix`

**Signature:**
```c
int s_has_prefix(int h, const char *prefix)
```

Checks if a string buffer starts with a given prefix.

**Parameters:**
- h The handle of the string buffer.
- prefix The prefix to check.

**Returns:** 1 if it has the prefix, 0 otherwise.

---

### `s_has_suffix`

**Signature:**
```c
int s_has_suffix(int h, const char *suffix)
```

Checks if a string buffer ends with a given suffix.

**Parameters:**
- h The handle of the string buffer.
- suffix The suffix to check.

**Returns:** 1 if it has the suffix, 0 otherwise.

---

### `s_from_long`

**Signature:**
```c
int s_from_long(long val)
```

Creates a string buffer from a long value.

**Parameters:**
- val The long value.

**Returns:** The handle of the new string buffer.

---

### `s_hash`

**Signature:**
```c
uint32_t s_hash(int h)
```

Calculates a hash value for a string buffer (djb2 algorithm).

**Parameters:**
- h The handle of the string buffer.

**Returns:** The 32-bit hash value.

---

### `s_join`

**Signature:**
```c
int s_join(const char *sep, ...)
```

Joins multiple C-style strings with a separator into a new string buffer.

**Parameters:**
- sep The separator string.
- ... Variable list of C-style strings, terminated by NULL.

**Returns:** The handle of the new string buffer.

---

### `s_cmp`

**Signature:**
```c
int s_cmp(int a, int b)
```

Compares two string buffers lexicographically.

**Parameters:**
- a Handle of the first string buffer.
- b Handle of the second string buffer.

**Returns:** 0 if equal, <0 if a < b, >0 if a > b.

---

### `s_ncmp`

**Signature:**
```c
int s_ncmp(int a, int b, int n)
```

Compares up to n characters of two string buffers.

**Parameters:**
- a Handle of the first string buffer.
- b Handle of the second string buffer.
- n Maximum number of characters to compare.

**Returns:** 0 if equal, <0 if a < b, >0 if a > b.

---

### `s_chr`

**Signature:**
```c
int s_chr(int h, int c, int off)
```

Finds the first occurrence of a character in a string buffer.

**Parameters:**
- h The handle of the string buffer.
- c The character to find.
- off The offset to start searching from.

**Returns:** The index of the character, or -1.

---

### `s_rchr`

**Signature:**
```c
int s_rchr(int h, int c)
```

Finds the last occurrence of a character in a string buffer.

**Parameters:**
- h The handle of the string buffer.
- c The character to find.

**Returns:** The index of the character, or -1.

---

### `s_find`

**Signature:**
```c
int s_find(int h, const char *sub)
```

Finds the first occurrence of a substring in a string buffer.

**Parameters:**
- h The handle of the string buffer.
- sub The substring to find.

**Returns:** The index of the substring, or -1.

---

### `s_spn`

**Signature:**
```c
int s_spn(int h, const char *accept)
```

Calculates the length of the initial segment of a string buffer which consists entirely of characters in accept.

**Parameters:**
- h The handle of the string buffer.
- accept C-style string containing characters to accept.

**Returns:** The number of characters in the initial segment.

---

### `s_cspn`

**Signature:**
```c
int s_cspn(int h, const char *reject)
```

Calculates the length of the initial segment of a string buffer which consists entirely of characters not in reject.

**Parameters:**
- h The handle of the string buffer.
- reject C-style string containing characters to reject.

**Returns:** The number of characters in the initial segment.

---

### `s_cat`

**Signature:**
```c
int s_cat(int h, const char *src)
```

Concatenates a C-style string to a string buffer.

**Parameters:**
- h The handle of the string buffer. If <= 0, a new one is allocated.
- src The C-style string to concatenate.

**Returns:** The handle of the string buffer.

---

### `s_ncat`

**Signature:**
```c
int s_ncat(int h, const char *src, int n)
```

Concatenates up to n characters of a C-style string to a string buffer.

**Parameters:**
- h The handle of the string buffer. If <= 0, a new one is allocated.
- src The C-style string to concatenate.
- n Maximum number of characters to concatenate.

**Returns:** The handle of the string buffer.

---

### `s_sub`

**Signature:**
```c
int s_sub(int h, int pos, int len)
```

Extracts a substring into a new string buffer.

**Parameters:**
- h The handle of the source string buffer.
- pos Starting position.
- len Length of the substring.

**Returns:** The handle of the new string buffer.

---

### `s_left`

**Signature:**
```c
int s_left(int h, int n)
```

Returns the leftmost n characters of a string buffer as a new string buffer.

**Parameters:**
- h The handle of the string buffer.
- n Number of characters to return.

**Returns:** The handle of the new string buffer.

---

### `s_right`

**Signature:**
```c
int s_right(int h, int n)
```

Returns the rightmost n characters of a string buffer as a new string buffer.

**Parameters:**
- h The handle of the string buffer.
- n Number of characters to return.

**Returns:** The handle of the new string buffer.

---

### `s_replace_c`

**Signature:**
```c
int s_replace_c(int h, const char *old, const char *replacement)
```

Replaces all occurrences of a substring with another string.

**Parameters:**
- h The handle of the string buffer.
- old Substring to find.
- replacement String to replace with.

**Returns:** A new handle to the string buffer with replacements.

---

### `s_trim_c`

**Signature:**
```c
int s_trim_c(int h, const char *chars)
```

Trims specified characters from both ends of a string buffer.

**Parameters:**
- h The handle of the string buffer.
- chars C-style string containing characters to trim. If NULL, whitespace is trimmed.

**Returns:** A new handle to the trimmed string buffer.

---

### `s_lastchar`

**Signature:**
```c
int s_lastchar(int m)
```

Returns the last non-null character of a string buffer.

**Parameters:**
- m The handle of the string buffer.

**Returns:** The last character, or 0 if empty.

---

### `s_copy`

**Signature:**
```c
int s_copy(int m, int first_char, int last_char)
```

Copies a range of characters from one string buffer to a new one.

**Parameters:**
- m The source handle.
- first_char Index of the first character.
- last_char Index of the last character. If negative, goes to the end.

**Returns:** The handle of the new string buffer.

---

### `s_index`

**Signature:**
```c
int s_index(int buf, int p, int ch)
```

Finds the index of a character in an m-array buffer.

**Parameters:**
- buf The handle of the buffer.
- p Starting index.
- ch Character to find.

**Returns:** The index, or -1.

---

### `s_split`

**Signature:**
```c
int s_split(int m, const char *s, int c, int remove_wspace)
```

Splits a string into an m-array of strings based on a delimiter character.

**Parameters:**
- m The handle to store the resulting strings. If 0, a new one is allocated.
- s The string to split.
- c The delimiter character.
- remove_wspace If non-zero, trims whitespace from the parts.

**Returns:** The handle of the m-array of strings.

---

### `s_slice`

**Signature:**
```c
int s_slice(int dest, int offs, int m, int a, int b)
```

Creates a slice of a string buffer.

**Parameters:**
- dest Destination handle. If 0, a new one is allocated.
- offs Destination offset.
- m Source handle.
- a Start index.
- b End index.

**Returns:** The handle of the resulting string buffer.

---

### `s_strstr`

**Signature:**
```c
int s_strstr(int m, int offs, int pattern)
```

Finds a pattern in a string buffer.

**Parameters:**
- m The handle of the string buffer.
- offs Starting offset.
- pattern The handle of the pattern string.

**Returns:** The index, or -1.

---

### `s_replace`

**Signature:**
```c
int s_replace(int dest, int src, int pattern, int replace, int count)
```

Replaces occurrences of a pattern in a string buffer.

**Parameters:**
- dest Destination handle. If 0, a new one is allocated.
- src Source handle.
- pattern Handle of the pattern to find.
- replace Handle of the replacement string.
- count Maximum number of replacements.

**Returns:** The handle of the destination string buffer.

---

### `s_strncmp`

**Signature:**
```c
int s_strncmp(int m, int offs, int pattern, int n)
```

Compares a substring of a string buffer with a pattern.

**Parameters:**
- m Source handle.
- offs Source offset.
- pattern Pattern handle.
- n Number of characters to compare.

**Returns:** 0 if equal, otherwise like strncmp.

---

### `s_isempty`

**Signature:**
```c
int s_isempty(int m)
```

Checks if a string buffer is empty.

**Parameters:**
- m The handle of the string buffer.

**Returns:** 1 if empty, 0 otherwise.

---

### `s_trim`

**Signature:**
```c
int s_trim(int m)
```

Trims whitespace from both ends of a string buffer (in-place).

**Parameters:**
- m The handle of the string buffer.

**Returns:** The handle of the string buffer.

---

### `s_lower`

**Signature:**
```c
int s_lower(int m)
```

Converts a string buffer to lowercase (in-place).

**Parameters:**
- m The handle of the string buffer.

**Returns:** The handle of the string buffer.

---

### `s_upper`

**Signature:**
```c
int s_upper(int m)
```

Converts a string buffer to uppercase (in-place).

**Parameters:**
- m The handle of the string buffer.

**Returns:** The handle of the string buffer.

---

### `s_msplit`

**Signature:**
```c
int s_msplit(int dest, int src, int pattern)
```

Splits a string buffer into an m-array of string handles.

**Parameters:**
- dest Handle of the resulting m-array. If 0, a new one is allocated.
- src Handle of the source string buffer.
- pattern Handle of the delimiter string buffer.

**Returns:** The handle of the resulting m-array.

---

### `s_implode`

**Signature:**
```c
int s_implode(int dest, int srcs, int seperator)
```

Joins an m-array of string handles into a single string buffer.

**Parameters:**
- dest Destination handle. If 0, a new one is allocated.
- srcs Handle of the m-array of string handles.
- separator Handle of the separator string buffer.

**Returns:** The handle of the destination string buffer.

---

### `s_strdup_c`

**Signature:**
```c
int s_strdup_c(const char *s)
```

Duplicates a C-style string into a new string buffer.

**Parameters:**
- s The C-style string.

**Returns:** The handle of the new string buffer.

---

### `s_strcpy_c`

**Signature:**
```c
int s_strcpy_c(int out, const char *s)
```

Copies a C-style string into an existing string buffer.

**Parameters:**
- out Handle of the destination string buffer. If <= 0, a new one is allocated.
- s The C-style string.

**Returns:** The handle of the string buffer.

---

### `s_puts`

**Signature:**
```c
void s_puts(int m)
```

Prints a string buffer followed by a newline.

**Parameters:**
- m The handle of the string buffer.

---

### `s_write`

**Signature:**
```c
void s_write(int m, int n)
```

Truncates a string buffer to a given length and ensures null-termination.

**Parameters:**
- m The handle of the string buffer.
- n The new length.

---

### `mls_printf_handler`

**Signature:**
```c
static int mls_printf_handler(FILE *stream, const struct printf_info *info,
			       const void *const *args)
```

Internal handler for the %M specifier in printf.
Expects an m-array string handle.

---

### `mls_printf_arginfo`

**Signature:**
```c
static int mls_printf_arginfo(const struct printf_info *info, size_t n,
			       int *argtypes, int *size)
```

Internal arginfo handler for the %M specifier in printf.

---

### `m_register_printf`

**Signature:**
```c
void m_register_printf()
```

Registers the %M specifier for printf to handle m-array string handles.

---

### `conststr_init`

**Signature:**
```c
void conststr_init(void)
```

Initializes the constant string system.

---

### `conststr_free`

**Signature:**
```c
void conststr_free(void)
```

Frees the constant string system.

---

### `conststr_lookup_c`

**Signature:**
```c
int conststr_lookup_c(const char *s)
```

Looks up or creates a constant string from a C-style string.

**Parameters:**
- s The C-style string.

**Returns:** The handle of the constant string.

---

### `conststr_lookup`

**Signature:**
```c
int conststr_lookup(int s)
```

Looks up or creates a constant string from an existing string buffer.

**Parameters:**
- s Handle of the source string buffer.

**Returns:** The handle of the constant string.

---

### `cs_printf`

**Signature:**
```c
int cs_printf(const char *format, ...)
```

Formatted creation of a constant string.

**Parameters:**
- format Format string.
- ... Arguments.

**Returns:** The handle of the constant string.

---

### `v_init`

**Signature:**
```c
int v_init(void)
```

Initializes a variable system.

**Returns:** The handle of the new variable system.

---

### `v_free`

**Signature:**
```c
void v_free(int vl)
```

Frees a variable system and all its variables.

**Parameters:**
- vl The handle of the variable system.

---

### `v_find_key`

**Signature:**
```c
int v_find_key(int vs, const char *name)
```

Finds the index of a variable key in a variable system.

**Parameters:**
- vs The variable system handle.
- name The name of the variable.

**Returns:** The index, or -1.

---

### `v_lookup`

**Signature:**
```c
int v_lookup(int vl, const char *name)
```

Looks up or creates a variable in a variable system.

**Parameters:**
- vl The variable system handle.
- name The name of the variable.

**Returns:** The handle of the variable.

---

### `v_kset`

**Signature:**
```c
void v_kset(int var, const char *v, int row)
```

Sets a value at a specific position in a variable.

**Parameters:**
- var The variable handle.
- v The string value.
- row The position (index).

---

### `v_set`

**Signature:**
```c
int v_set(int vs, const char *name, const char *value, int pos)
```

Sets a variable value in a variable system.

**Parameters:**
- vs The variable system handle.
- name The variable name.
- value The string value.
- pos The position (VAR_APPEND for append).

**Returns:** The handle of the variable.

---

### `v_vaset`

**Signature:**
```c
void v_vaset(int vs, ...)
```

Variadic set of multiple variable names and values.

**Parameters:**
- vs The variable system handle.
- ... Pairs of (char* name, char* value), terminated by NULL.

---

### `v_kclr`

**Signature:**
```c
void v_kclr(int var)
```

Clears all values from a variable, keeping only the name.

**Parameters:**
- var The variable handle.

---

### `v_clr`

**Signature:**
```c
void v_clr(int vs, const char *name)
```

Clears a variable by name in a variable system.

**Parameters:**
- vs The variable system handle.
- name The variable name.

---

### `v_klen`

**Signature:**
```c
int v_klen(int key)
```

Gets a value from a variable at a specific position.

/
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
Gets a variable value from a variable system.

/
char *v_get (int vs, const char *name, int pos)
{
return v_kget (v_lookup (vs, name), pos);
}

/**
Returns the number of values in a variable (excluding the name).

**Parameters:**
- var The variable handle.
- row The position.
- vs The variable system handle.
- name The variable name.
- pos The position.
- key The variable handle.

**Returns:** The number of values.

---

### `v_len`

**Signature:**
```c
int v_len(int vs, const char *name)
```

Returns the number of values in a variable by name.

**Parameters:**
- vs The variable system handle.
- name The variable name.

**Returns:** The number of values.

---

### `v_remove`

**Signature:**
```c
void v_remove(int vs, const char *name)
```

Removes a variable from a variable system.

**Parameters:**
- vs The variable system handle.
- name The variable name.

---

### `se_init`

**Signature:**
```c
void se_init(str_exp_t *se)
```

Initializes a string expansion structure.

**Parameters:**
- se Pointer to the string expansion structure.

---

### `se_free`

**Signature:**
```c
void se_free(str_exp_t *se)
```

Frees resources associated with a string expansion structure.

**Parameters:**
- se Pointer to the string expansion structure.

---

### `se_realloc_buffers`

**Signature:**
```c
void se_realloc_buffers(str_exp_t *se)
```

Reallocates or initializes buffers for string expansion.

**Parameters:**
- se Pointer to the string expansion structure.

---

### `parse_index`

**Signature:**
```c
static int parse_index(const char **s)
```

Internal helper to parse index in string expansion (e.g., [1], [*]).

---

### `se_parse`

**Signature:**
```c
void se_parse(str_exp_t *se, const char *frm)
```

Parses a format string for expansion.

**Parameters:**
- se Pointer to the string expansion structure.
- frm The format string.

---

### `repl_char`

**Signature:**
```c
static void repl_char(int buf, char ch)
```

Internal helper to replace special characters with escaped sequences.

---

### `escape_buf`

**Signature:**
```c
void escape_buf(int buf, char *src)
```

Escapes a string into a buffer.

**Parameters:**
- buf Handle of the destination buffer.
- src Source C-style string.

---

### `escape_str`

**Signature:**
```c
int escape_str(int buf, char *src)
```

Creates an escaped version of a string in a new or existing buffer.

**Parameters:**
- buf Destination handle (if 0, a new one is allocated).
- src Source C-style string.

**Returns:** The handle of the buffer containing the escaped string.

---

### `field_escape`

**Signature:**
```c
static int field_escape(int s2, char *s, int quotes)
```

Internal helper to escape a field, optionally adding quotes.

---

### `get_variable_handle`

**Signature:**
```c
static int get_variable_handle(int vl, const char *name)
```

Internal helper to get a variable handle from a variable system or table.

---

### `if`

**Signature:**
```c
 if(vn < m_len (se->values) && (val_ptr = (char **)mls (se->values, vn)) && *val_ptr == s)
```

Expands a parsed format string using values from a variable system or table.

/
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
/* Check if this part in splitbuf matches the next expected variable in se->values

**Parameters:**
- se Pointer to the parsed string expansion structure.
- vl Handle of the variable system or table.
- row The row index to use for expansion.

**Returns:** Pointer to the expanded string (stored in se->buf).

---

### `m_str_from_file`

**Signature:**
```c
int m_str_from_file(char *filename)
```

Expands a format string in one step and returns the result.

/
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
Reads the entire content of a file into a new string buffer.

**Parameters:**
- vl Handle of the variable system or table.
- frm The format string.
- filename Path to the file.

**Returns:** The handle of the new string buffer, or -1 on error.

---

### `skip_delim`

**Signature:**
```c
static void skip_delim(char **s, char *delim)
```

Internal helper to skip a delimiter string.

---

### `skip_unitl_delim`

**Signature:**
```c
static void skip_unitl_delim(char **s, char *delim)
```

Internal helper to skip characters until a delimiter string is found.

---

### `cut_word`

**Signature:**
```c
static int cut_word(char **s, char *delim, int trimws, char **a, char **b)
```

Internal helper to cut a word from a string based on a delimiter.

---

### `m_str_split`

**Signature:**
```c
int m_str_split(int ms, char *s, char *delim, int trimws)
```

Internal helper to duplicate a word between two pointers, optionally trimming whitespace.
/
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
Splits a string into an m-array of strings using a delimiter string.

**Parameters:**
- ms Handle of the m-array to store strings.
- s Source C-style string.
- delim Delimiter string.
- trimws If non-zero, trims whitespace from results.

**Returns:** The m-array handle.

---

### `s_strcmp_c`

**Signature:**
```c
int s_strcmp_c(int mstr, const char *s)
```

Compares an m-array string handle with a C-style string.

**Parameters:**
- mstr Handle of the m-array string.
- s C-style string.

**Returns:** 0 if equal, otherwise like strcmp.

---

### `m_strncpy`

**Signature:**
```c
int m_strncpy(int dst, int src, int max)
```

Copies a string from one handle to another with a maximum length.

**Parameters:**
- dst Destination handle. If <= 0, a new one is allocated.
- src Source handle.
- max Maximum number of characters to copy.

**Returns:** The destination handle.

---

### `element_copy`

**Signature:**
```c
static void element_copy(int dest, int destp, int src, int srcp, int src_count,
			  int width)
```

Internal helper to copy elements between m-arrays.

---

### `m_mcopy`

**Signature:**
```c
int m_mcopy(int dest, int destp, int src, int srcp, int src_count)
```

Copies a range of elements from one m-array to another, handling width differences.

**Parameters:**
- dest Destination handle.
- destp Destination offset.
- src Source handle.
- srcp Source offset.
- src_count Number of elements to copy.

**Returns:** The destination handle.

---

### `m_memset`

**Signature:**
```c
void m_memset(int ln, char c, int w)
```

Fills an m-array with a constant byte.

**Parameters:**
- ln Handle of the m-array.
- c Byte value.
- w Number of bytes to fill.

---

### `s_strncmp2`

**Signature:**
```c
int s_strncmp2(int s0, int p0, int s1, int p1, int len)
```

Compares substrings of two m-array string handles.

---

### `s_strncmpr`

**Signature:**
```c
int s_strncmpr(int str, int suffix)
```

Compares the end of a string buffer with a suffix string buffer.

**Parameters:**
- str Handle of the string buffer.
- suffix Handle of the suffix string buffer.

**Returns:** 0 if it matches, otherwise non-zero.

---

### `s_readln`

**Signature:**
```c
int s_readln(int buf, FILE *fp)
```

Reads a line from a file into an m-array string buffer.

**Parameters:**
- buf Handle of the string buffer.
- fp File pointer.

**Returns:** The length of the line read, or EOF.

---

### `ring_create`

**Signature:**
```c
int ring_create(int size)
```

Creates a ring buffer (circular buffer) of integers.

**Parameters:**
- size The capacity of the ring buffer.

**Returns:** The handle of the new ring buffer.

---

### `ring_empty`

**Signature:**
```c
int ring_empty(int r)
```

Checks if a ring buffer is empty.

**Parameters:**
- r The ring buffer handle.

**Returns:** 1 if empty, 0 otherwise.

---

### `ring_full`

**Signature:**
```c
int ring_full(int r)
```

Checks if a ring buffer is full.

**Parameters:**
- r The ring buffer handle.

**Returns:** 1 if full, 0 otherwise.

---

### `ring_put`

**Signature:**
```c
int ring_put(int r, int data)
```

Appends an integer to a ring buffer.

**Parameters:**
- r The ring buffer handle.
- data The integer to append.

**Returns:** 0 on success, -1 if the buffer is full.

---

### `ring_get`

**Signature:**
```c
int ring_get(int r)
```

Retrieves and removes the oldest integer from a ring buffer.

**Parameters:**
- r The ring buffer handle.

**Returns:** The integer retrieved, or -1 if the buffer is empty.

---

### `ring_free`

**Signature:**
```c
void ring_free(int r)
```

Frees a ring buffer.

**Parameters:**
- r The ring buffer handle.

---

### `m_free_strings`

**Signature:**
```c
void m_free_strings(int list, int CLEAR_ONLY)
```

Specialized free function for lists containing dynamically allocated strings (char *).
Frees each string in the list and optionally the list handle itself.

If zero, the handle itself is also freed.

**Parameters:**
- list The handle of the string list.
- CLEAR_ONLY If non-zero, the strings are freed and the list is cleared, but the handle remains.

---

## File: `m_table.c`

### `m_table_create`

**Signature:**
```c
int m_table_create()
```

Creates a new empty table handle.
A table stores key-value pairs where keys can be integers or strings,
and values can be various types (ints, strings, lists, other tables).

**Returns:** The handle of the new table.

---

### `m_is_table`

**Signature:**
```c
int m_is_table(int table_h)
```

Checks if a handle refers to a table.

**Parameters:**
- table_h The handle to check.

**Returns:** 1 if it is a table, 0 otherwise.

---

### `m_table_free`

**Signature:**
```c
void m_table_free(int table_h)
```

Frees a table and all its contents recursively.

**Parameters:**
- table_h The handle of the table to free.

---

### `m_table_set_int_key`

**Signature:**
```c
void m_table_set_int_key(int table_h, int key_idx, int value,
			  mls_table_type_t type)
```

Sets a value for an integer key in the table.

**Parameters:**
- table_h Handle of the table.
- key_idx Integer key.
- value Value (raw int or handle).
- type Type of the value.

---

### `m_table_set_str_key_ext`

**Signature:**
```c
void m_table_set_str_key_ext(int table_h, int key_str_h,
			      mls_table_type_t key_type, int value,
			      mls_table_type_t type)
```

Sets a value for a string handle key with a specific key type.

**Parameters:**
- table_h Handle of the table.
- key_str_h Handle of the key string.
- key_type Type of the key (e.g. MLS_TABLE_TYPE_STRING or MLS_TABLE_TYPE_CONST_STRING).
- value Value (raw int or handle).
- type Type of the value.

---

### `m_table_set_str_key`

**Signature:**
```c
void m_table_set_str_key(int table_h, int key_str_h, int value,
			  mls_table_type_t type)
```

Sets a value for a string handle key (defaults key type to MLS_TABLE_TYPE_STRING).

**Parameters:**
- table_h Handle of the table.
- key_str_h Handle of the key string.
- value Value (raw int or handle).
- type Type of the value.

---

### `m_table_set_cstr_key`

**Signature:**
```c
void m_table_set_cstr_key(int table_h, const char *key_cstr, int value,
			   mls_table_type_t type)
```

Sets a value for a C-style string key.

**Parameters:**
- table_h Handle of the table.
- key_cstr The C-style string key.
- value Value (raw int or handle).
- type Type of the value.

---

### `mt_seti`

**Signature:**
```c
void mt_seti(int table_h, const char *key, int val)
```

Sets an integer value for a C-string key.

**Parameters:**
- table_h Handle of the table.
- key Key name.
- val Integer value.

---

### `mt_sets`

**Signature:**
```c
void mt_sets(int table_h, const char *key, const char *val)
```

Sets a string value for a C-string key.

**Parameters:**
- table_h Handle of the table.
- key Key name.
- val String value.

---

### `mt_setc`

**Signature:**
```c
void mt_setc(int table_h, const char *key, const char *val)
```

Sets a constant string value for a C-string key.

**Parameters:**
- table_h Handle of the table.
- key Key name.
- val String value.

---

### `mt_seth`

**Signature:**
```c
void mt_seth(int table_h, const char *key, int handle, mls_table_type_t type)
```

Sets a handle value for a C-string key.

**Parameters:**
- table_h Handle of the table.
- key Key name.
- handle The handle to store.
- type The type of the handle.

---

### `mt_get`

**Signature:**
```c
int mt_get(int table_h, const char *key)
```

Gets a value from the table by C-string key.

**Parameters:**
- table_h Handle of the table.
- key Key name.

**Returns:** The value (int or handle).

---

### `m_table_get_int`

**Signature:**
```c
int m_table_get_int(int table_h, int key_idx)
```

Gets a value from the table by integer key.

**Parameters:**
- table_h Handle of the table.
- key_idx Integer key.

**Returns:** The value (int or handle), or 0 if not found.

---

### `m_table_get_str`

**Signature:**
```c
int m_table_get_str(int table_h, int key_str_h)
```

Gets a value from the table by string handle key.

**Parameters:**
- table_h Handle of the table.
- key_str_h String handle key.

**Returns:** The value (int or handle), or 0 if not found.

---

### `m_table_get_cstr`

**Signature:**
```c
int m_table_get_cstr(int table_h, const char *key_cstr)
```

Gets a value from the table by C-string key.

**Parameters:**
- table_h Handle of the table.
- key_cstr C-string key.

**Returns:** The value (int or handle), or 0 if not found.

---

### `m_table_keys`

**Signature:**
```c
int m_table_keys(int table_h)
```

Returns a list of all keys in the table.

User is responsible for freeing the list (but not the keys themselves).

**Parameters:**
- table_h Handle of the table.

**Returns:** Handle of the m-array containing the keys.

---

### `m_table_get_type_int`

**Signature:**
```c
mls_table_type_t m_table_get_type_int(int table_h, int key_idx)
```

Gets the type of a value by integer key.

**Parameters:**
- table_h Handle of the table.
- key_idx Integer key.

**Returns:** The type of the value.

---

### `m_table_get_type_str`

**Signature:**
```c
mls_table_type_t m_table_get_type_str(int table_h, int key_str_h)
```

Gets the type of a value by string handle key.

**Parameters:**
- table_h Handle of the table.
- key_str_h String handle key.

**Returns:** The type of the value.

---

### `m_table_get_type_cstr`

**Signature:**
```c
mls_table_type_t m_table_get_type_cstr(int table_h, const char *key_cstr)
```

Gets the type of a value by C-string key.

**Parameters:**
- table_h Handle of the table.
- key_cstr C-string key.

**Returns:** The type of the value.

---

## File: `m_extra.c`

### `s_casecmp`

**Signature:**
```c
int s_casecmp(int a, int b)
```

Case-insensitive comparison of two strings.

**Parameters:**
- a The first string handle.
- b The second string handle.

**Returns:** <0 if a < b, 0 if a == b, >0 if a > b.

---

### `s_ncasecmp`

**Signature:**
```c
int s_ncasecmp(int a, int b, int n)
```

Case-insensitive comparison of up to n characters of two strings.

**Parameters:**
- a The first string handle.
- b The second string handle.
- n Maximum number of characters to compare.

**Returns:** <0 if a < b, 0 if a == b, >0 if a > b.

---

### `s_to_long`

**Signature:**
```c
long s_to_long(int h)
```

Converts a string to a long integer.

**Parameters:**
- h The string handle.

**Returns:** The long value of the string, or 0 if string is empty or invalid.

---

### `s_trim_left_c`

**Signature:**
```c
int s_trim_left_c(int h, const char *chars)
```

Trims specific characters from the left side of a string.

**Parameters:**
- h The string handle.
- chars A string containing characters to trim. If NULL, trims whitespace.

**Returns:** A new handle containing the trimmed string.

---

### `s_trim_right_c`

**Signature:**
```c
int s_trim_right_c(int h, const char *chars)
```

Trims specific characters from the right side of a string.

**Parameters:**
- h The string handle.
- chars A string containing characters to trim. If NULL, trims whitespace.

**Returns:** A new handle containing the trimmed string.

---

### `s_reverse`

**Signature:**
```c
int s_reverse(int h)
```

Reverses the string in-place.

**Parameters:**
- h The string handle.

**Returns:** The original handle (h).

---

### `s_pad_left`

**Signature:**
```c
int s_pad_left(int h, int width, char pad)
```

Pads the string on the left to reach a specified width.

**Parameters:**
- h The string handle.
- width The target width.
- pad The character to pad with.

**Returns:** A new handle containing the padded string.

---

### `s_pad_right`

**Signature:**
```c
int s_pad_right(int h, int width, char pad)
```

Pads the string on the right to reach a specified width.

**Parameters:**
- h The string handle.
- width The target width.
- pad The character to pad with.

**Returns:** A new handle containing the padded string.

---

### `s_is_numeric`

**Signature:**
```c
int s_is_numeric(int h)
```

Checks if the string consists only of digits.

**Parameters:**
- h The string handle.

**Returns:** 1 if numeric, 0 otherwise.

---

### `s_is_alpha`

**Signature:**
```c
int s_is_alpha(int h)
```

Checks if the string consists only of alphabetic characters.

/
/**
Checks if the string consists only of alphabetic characters.

**Parameters:**
- h The string handle.
- h The string handle.

**Returns:** 1 if alphabetic, 0 otherwise.

---

### `s_base64_decode`

**Signature:**
```c
int s_base64_decode(int h)
```

Decodes a base64 encoded string.

**Parameters:**
- h The string handle containing base64 data.

**Returns:** A new handle containing the decoded binary data.

---

### `s_secure_cmp`

**Signature:**
```c
int s_secure_cmp(int a, int b)
```

Constant-time comparison of two strings.
Prevents timing attacks when comparing secrets like passwords or keys.

**Parameters:**
- a The first string handle.
- b The second string handle.

**Returns:** 0 if strings are equal, 1 otherwise.

---

