# Project API Documentation

## File: `mls_base.c`

### `deb_err`

**Signature:**
```c
void deb_err(int line, const char *file, const char *function,
	      const char *format, ...)
```

Prints an error message with file and line information and terminates the
program. Sets the error_occurred flag for post-mortem analysis.

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

Prints a trace message if the provided level is greater than or equal to
trace_level.

**Parameters:**
- l The trace level of this message.
- line The line number where the trace occurred.
- file The source file where the trace occurred.
- function The function where the trace occurred.
- format The format string for the trace message.

---

### `_mls_die`

**Signature:**
```c
void _mls_die(int line, const char *file, const char *function)
```

Report the pending mls_errno error with caller location and exit(1).
Used by the mls_must() macro; never returns.

---

### `lst`

**Signature:**
```c
void * lst(lst_t l, size_t i)
```

Returns a pointer to the element at the specified index in a list structure.

**Parameters:**
- l The list structure.
- i The index of the element.

**Returns:** A pointer to the element data.

---

### `lst_resize`

**Signature:**
```c
void lst_resize(lst_t lp, size_t new_size)
```

Resizes a list structure to a new maximum size.
Dies on error; lst_resize_safe() reports the error instead.

**Parameters:**
- lp Pointer to the list structure pointer.
- new_size The new maximum number of elements.

---

### `lst_create`

**Signature:**
```c
void lst_create(lst_t l, size_t max, size_t w)
```

Creates a new list structure.

**Parameters:**
- l The list structure to initialize.
- max The initial maximum number of elements.
- w The width of each element in bytes.

---

### `lst_new`

**Signature:**
```c
int lst_new(lst_t lp, size_t n)
```

Reserves space for n new elements in a list.
if no space left, increase max space by 50%

**Parameters:**
- lp Pointer to the list structure pointer.
- n The number of elements to reserve.

**Returns:** The index of the first newly reserved element.

---

### `lst_put`

**Signature:**
```c
int lst_put(lst_t lp, const void *d)
```

Appends an element to a list.

**Parameters:**
- lp Pointer to the list structure pointer.
- d Pointer to the element data to append.

**Returns:** The index of the appended element.

---

### `lst_peek`

**Signature:**
```c
void * lst_peek(lst_t l, size_t i)
```

Returns a pointer to the element at the specified index without using
allocated bounds checking this function allows to access all elementes in the
array. normaly the length (l) value is compared to the index, you need to
call m_setlen or use m_put which automatically increases the array length
value. lst_peek gives you access to all allocated memory postion in this
array

**Parameters:**
- l The list structure.
- i The index of the element.

**Returns:** A pointer to the element data

---

### `lst_del`

**Signature:**
```c
void lst_del(lst_t l, size_t p)
```

Deletes an element at the specified index from a list.

**Parameters:**
- l The list structure.
- p The index of the element to delete.

---

### `lst_remove`

**Signature:**
```c
void lst_remove(lst_t lp, size_t p, size_t n)
```

Removes n elements starting from the specified index from a list.

**Parameters:**
- lp Pointer to the list structure pointer.
- p The starting index.
- n The number of elements to remove.

---

### `lst_ins`

**Signature:**
```c
void * lst_ins(lst_t lp, size_t p, size_t n)
```

Inserts n empty elements at the specified index in a list.

**Parameters:**
- lp Pointer to the list structure pointer.
- p The insertion index.
- n The number of elements to insert.

**Returns:** A pointer to the first newly inserted element.

---

### `lst_next`

**Signature:**
```c
int lst_next(lst_t l, int *p, void *data)
```

Iterates to the next element in a list.

**Parameters:**
- l The list structure.
- p Pointer to the current index. Should be initialized to -1.
- data Pointer to store the address of the next element.

**Returns:** 1 if an element was found, 0 otherwise.

---

### `lst_read`

**Signature:**
```c
int lst_read(lst_t l, size_t p, void **data, size_t n)
```

Reads n elements from a list into a buffer.

buffer is allocated.

**Parameters:**
- l The list structure.
- p The starting index.
- data Pointer to the destination buffer pointer. If *data is 0, a
- n The number of elements to read.

**Returns:** 0 on success.

---

### `lst_write`

**Signature:**
```c
int lst_write(lst_t lp, size_t p, const void *data, size_t n)
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
static inline lst_t get_list(int m)
```

Internal function to retrieve a pointer to the list structure associated with
a handle. Performs validation checks for handle range, existence, and UAF
protection.

**Parameters:**
- m The handle to look up.

**Returns:** A pointer to the list structure pointer in the master list.

---

### `exported_get_list`

**Signature:**
```c
lst_t exported_get_list(int r)
```

Public accessor for getting a list pointer from a handle.

**Parameters:**
- r The handle.

**Returns:** A pointer to the list structure pointer.

---

### `m_alloc_safe`

**Signature:**
```c
int m_alloc_safe(size_t max, size_t w, uint8_t hfree)
```

Non-aborting version of m_alloc(): returns -1 and sets mls_errno on
OOM or size overflow. A bad free-handler ID is a parameter error and
still exits.

**Parameters:**
- max Initial maximum elements.
- w Width of each element in bytes.
- hfree The ID of the registered free handler to use.

**Returns:** A new 1-based MLS handle, or -1 on error.

---

### `m_create`

**Signature:**
```c
int m_create(size_t max, size_t w)
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
Handles with MFREE_NODESTRUCT will not be touched - useful if you want to
make sure that the memory stays allocated forever and the list never gets
destroyed

**Parameters:**
- m The handle to free.

**Returns:** 0 on success.

---

### `m_len`

**Signature:**
```c
size_t m_len(int m)
```

Returns the number of elements in the list associated with a handle.

**Parameters:**
- m The handle.

**Returns:** The number of elements.

---

### `m_buf`

**Signature:**
```c
void * m_buf(int m)
```

Returns a pointer to the raw data buffer of the list associated with a
handle.

**Parameters:**
- m The handle.

**Returns:** A pointer to the data buffer, or NULL if handle is invalid.

---

### `mls`

**Signature:**
```c
void * mls(int m, size_t i)
```

Returns a pointer to the element at the specified index with bounds checking.

**Parameters:**
- m The handle.
- i The index.

**Returns:** A pointer to the element.

---

### `mls_safe`

**Signature:**
```c
void * mls_safe(int m, size_t i)
```

Non-aborting version of mls(). Returns NULL on any error and sets mls_errno.

**Parameters:**
- m The handle.
- i The index.

**Returns:** A pointer to the element, or NULL on error.

---

### `m_next`

**Signature:**
```c
int m_next(int m, int *p, void *d)
```

Iterates through the elements of a handle's list.

**Parameters:**
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

### `m_put_safe`

**Signature:**
```c
int m_put_safe(int m, const void *data)
```

Non-aborting version of m_put(). Returns -1 on error and sets mls_errno.

**Parameters:**
- m The handle.
- data Pointer to the element data to append.

**Returns:** The index of the appended element, or -1 on error.

---

### `m_setlen`

**Signature:**
```c
int m_setlen(int m, size_t len)
```

Sets the logical length of a handle's list.

**Parameters:**
- m The handle.
- len The new length.

**Returns:** 0 on success.

---

### `m_setlen_safe`

**Signature:**
```c
int m_setlen_safe(int m, size_t len)
```

Non-aborting version of m_setlen(). Returns -1 on error and sets mls_errno.

**Parameters:**
- m The handle.
- len The new length.

**Returns:** 0 on success, -1 on error.

---

### `m_write`

**Signature:**
```c
int m_write(int m, size_t p, const void *data, size_t n)
```

Writes data to a handle's list starting at a specific index.
Resizes the list if necessary.

**Parameters:**
- m The handle.
- p The starting index.
- data Pointer to the source data.
- n The number of elements to write.

**Returns:** 0 on success.

---

### `m_write_safe`

**Signature:**
```c
int m_write_safe(int m, size_t p, const void *data, size_t n)
```

Non-aborting version of m_write(). Returns -1 on error and sets mls_errno.

**Parameters:**
- m The handle.
- p The starting index.
- data Pointer to the source data.
- n The number of elements to write.

**Returns:** 0 on success, -1 on error.

---

### `m_peek`

**Signature:**
```c
void * m_peek(int m, size_t i)
```

Returns a pointer to the element at the specified index without bounds
checking.

**Parameters:**
- m The handle.
- i The index.

**Returns:** A pointer to the element, or NULL if index is out of bounds.

---

### `m_width`

**Signature:**
```c
size_t m_width(int m)
```

Returns the width of each element in a handle's list in bytes.

**Parameters:**
- m The handle.

**Returns:** The element width.

---

### `m_resize_safe`

**Signature:**
```c
int m_resize_safe(int m, size_t new_size)
```

Resizes the allocated capacity of a handle's list.

**Parameters:**
- m The handle.
- new_size The new maximum number of elements.

---

### `m_base_init`

**Signature:**
```c
int m_base_init()
```

Initializes the core MLS system: master handle list + free list only.
Ext state (const-string map, free-handler table) is set up separately by
mls_ext (see m_init()).

**Returns:** 0 on success.

---

### `m_base_destruct`

**Signature:**
```c
void m_base_destruct()
```

Destroys the core MLS system: frees all remaining handle data and the
master list. Does not touch ext state.

---

## File: `mls_ext.c`

### `_mlsdb_caller`

**Signature:**
```c
static void _mlsdb_caller(const char *me, int ln, const char *fn,
			   const char *fun, int args, int handle, int index,
			   const void *data)
```

Records caller information for the current MLS operation.
Used by debug wrappers to provide context for error messages.

4:data).

**Parameters:**
- me The name of the function being wrapped.
- ln The line number of the call.
- fn The filename of the call.
- fun The function name of the caller.
- args Bitmask indicating which arguments are valid (1:handle, 2:index,
- handle The MLS handle involved in the operation.
- index The index involved in the operation.
- data Pointer to data involved in the operation.

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

### `free_strings_wrap`

**Signature:**
```c
static void free_strings_wrap(int h)
```

same as m_free_strings to be used as a custom free handler.

**Parameters:**
- h The handle being freed.

---

### `free_list_wrap`

**Signature:**
```c
static void free_list_wrap(int h)
```

Custom free handler that recursively frees MLS handles stored in a list.

**Parameters:**
- h The handle being freed.

---

### `conststr_lookup_c`

**Signature:**
```c
int conststr_lookup_c(const char *s, int copy_string)
```

Looks up or creates a constant string from a C-style string.

zero-copy interning.

**Parameters:**
- s The C-style string.
- copy_string If non-zero, duplicates the string; if zero, uses

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

### `m_init`

**Signature:**
```c
int m_init()
```

Initializes the MLS library system.
Allocates the master handle list and registers default free handlers.

**Returns:** 0 on success, 1 if already initialized.

---

### `conststr_free`

**Signature:**
```c
void conststr_free(void)
```

Frees the constant string system.
this function does nothing, freeing a constant does not make sense while
program is running. instead m_destruct() will free allocated memory for us

---

### `m_destruct`

**Signature:**
```c
void m_destruct()
```

Destroys the MLS library system.
Explicitly frees all remaining allocated handles and their contents.
Policy: Do not call user registered free_handler functions

---

### `m_reg_freefn`

**Signature:**
```c
int m_reg_freefn(free_fn_t free_fn)
```

@brief Registers a  a cleanup callback for managed arrays
The @p free_fn is triggered automatically when m_free() is called.
This allows for custom iteration and deallocation of array elements
before the array container itself is removed.

handler ID.
handlers registered.

**Parameters:**
- free_fn The function to be called when freeing handles with this

**Returns:** The handler ID on success, -1 if handler is invalid or too many

---

### `m_is_valid`

**Signature:**
```c
int m_is_valid(int h)
```

Checks if an MLS handle is valid and hasn't been freed.
the purpose of this function is make sure your program
does not exit() if you have an invalid handle

**Parameters:**
- h The handle to check.

**Returns:** 1 if the handle is valid, 0 if it is not valid.

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

### `m_dub`

**Signature:**
```c
int m_dub(int m)
```

Creates a duplicate of an existing m-array.
if the original list was write-protected with MFREE_NOALLOC
the copy will be read-write. This is not a deep copy operation.

**Parameters:**
- m The handle of the m-array to duplicate.

**Returns:** A new handle to the duplicated m-array.

---

### `m_new_safe`

**Signature:**
```c
int m_new_safe(int m, size_t n)
```

Reserves space for n new elements in a handle's list.

**Parameters:**
- m The handle.
- n The number of elements to reserve.

**Returns:** The index of the first newly reserved element.

---

### `m_add_safe`

**Signature:**
```c
void * m_add_safe(int m)
```

Appends one new element to a handle's list and returns a pointer to it.

**Parameters:**
- m The handle.

**Returns:** A pointer to the new element.

---

### `m_bufsize`

**Signature:**
```c
size_t m_bufsize(int m)
```

Returns the currently allocated capacity (buffer size) of a handle's list.

**Parameters:**
- m The handle.

**Returns:** The maximum number of elements before a realloc is needed.

---

### `m_read`

**Signature:**
```c
int m_read(int h, size_t p, void **data, size_t n)
```

Reads data from a handle's list into a buffer.

**Parameters:**
- h The handle.
- p The starting index.
- data Pointer to the destination buffer pointer.
- n The number of elements to read.

**Returns:** 0 on success.

---

### `m_read_safe`

**Signature:**
```c
int m_read_safe(int h, size_t p, void **data, size_t n)
```

Non-aborting version of m_read(). Returns -1 on error and sets mls_errno.

**Parameters:**
- h The handle.
- p The starting index.
- data Pointer to the destination buffer pointer.
- n The number of elements to read.

**Returns:** 0 on success, -1 on error.

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
void m_del(int m, size_t p)
```

Deletes an element at the specified index from a handle's list.
Remaining elements are shifted to close the gap.

**Parameters:**
- m The handle.
- p The index of the element to delete.

---

### `m_del_safe`

**Signature:**
```c
int m_del_safe(int m, size_t p)
```

Non-aborting version of m_del(). Returns -1 on error and sets mls_errno.

**Parameters:**
- m The handle.
- p The index of the element to delete.

**Returns:** 0 on success, -1 on error.

---

### `m_pop`

**Signature:**
```c
void * m_pop(int m)
```

Removes and returns a pointer to the last element of a handle's list.

**Parameters:**
- m The handle.

**Returns:** A pointer to the element, or NULL if the list is empty.

---

### `m_ins_safe`

**Signature:**
```c
int m_ins_safe(int m, size_t p, size_t n)
```

Inserts n empty (zero-initialized) elements at a specific index in a handle's
list.

**Parameters:**
- m The handle.
- p The insertion index.
- n The number of elements to insert.

**Returns:** 1 on success, 0 on failure.

---

### `m_slice`

**Signature:**
```c
int m_slice(int dest, int offs, int m, int a, int b)
```

TODO
Extracts a sub-range of elements from one list and appends or writes them
into another. Supports negative indices (relative to end of list).

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
void m_remove(int m, size_t p, size_t n)
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

were read before EOF.

**Parameters:**
- m The handle.
- delim The delimiter character.
- fp The file pointer.

**Returns:** The number of characters scanned, or EOF if no characters

---

### `m_cmp`

**Signature:**
```c
int m_cmp(int a, int b)
```

Compares two handle's lists element-by-element using memcmp.
only makes sense if width is equal on booth lists

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

Looks up a string in a sorted list of strings (char *).
If not found and NOT_INSERT is 0, the string is duplicated and inserted in
sorted position.

The list must be kept sorted: build it via these functions, or call
m_qsort(list, cmp_mstr) first. Lookups are then O(log n), not O(n).

An empty (or NULL) key counts as "not found": it returns -1 and is never
inserted. Empty strings are treated as invalid, so a bad key can never abort
the program and needs no special handling by the caller.

**Parameters:**
- m The handle of the sorted string list.
- key The string to look for.
- NOT_INSERT If non-zero, do not insert the string if not found.

**Returns:** The index of the string, or -1 if not found (or key is empty).

---

### `m_lookup_mstr`

**Signature:**
```c
int m_lookup_mstr(int m, int key, int NOT_INSERT)
```

Looks up a string handle in a sorted list of strings (char *). The key is an
mls string handle, so a caller that already holds one need not convert it
with m_str(). If not found and NOT_INSERT is 0, the key's text is duplicated
and inserted in sorted position.

Like m_lookup_str, the list must be kept sorted (O(log n) lookups). An empty
key counts as "not found": it returns -1 and is never inserted.

**Parameters:**
- m The handle of the sorted string list.
- key The string handle to look for.
- NOT_INSERT If non-zero, do not insert the string if not found.

**Returns:** The index of the string, or -1 if not found (or key is empty).

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

if len < 6).

**Parameters:**
- fp The file pointer.
- buf Buffer to store the UTF-8 byte sequence (null-terminated

**Returns:** The number of bytes in the UTF-8 character, or EOF.

---

### `cmp_int`

**Signature:**
```c
int cmp_int(const void *a0, const void *b0)
```

Comparison function for integers, suitable for qsort or binary
search.

**Parameters:**
- a0 Pointer to first integer.
- b0 Pointer to second integer.

**Returns:** a0 - b0.

---

### `m_binsert`

**Signature:**
```c
int m_binsert(int buf, const void *data,
	       int (*cmpf) (const void *data, const void *buf_elem),
	       int with_duplicates)
```

Inserts an element into a sorted m-array, maintaining order.

exists and duplicates are not allowed.
@bugs if data is not allocated or its alloced size if less the
m_width(buf) this will crash!

**Parameters:**
- buf The handle of the m-array.
- data Pointer to the element to insert.
- cmpf Comparison function pointer.
- with_duplicates If non-zero, allows duplicate elements.

**Returns:** The index where the element was inserted, or -index if it

---

### `m_binsert2`

**Signature:**
```c
int m_binsert2(int buf, const void *data,
		int (*cmpf) (const void *data, const void *buf_elem),
		int with_duplicates, int with_copy)
```

Inserts an element into a sorted m-array, maintaining order.
Like m_binsert, but with_copy=0 skips copying data into the array —
only an empty slot is made room for and the caller must fill it.
Use when data is not m_width(buf) bytes long (e.g. a C string looked
up in an int list).

an empty (zeroed) slot is inserted instead.
exists and duplicates are not allowed.

**Parameters:**
- buf The handle of the m-array.
- data Pointer to the element to insert.
- cmpf Comparison function pointer.
- with_duplicates If non-zero, allows duplicate elements.
- with_copy If non-zero, data is copied into the array; if zero,

**Returns:** The index where the element was inserted, or -index if it

---

### `m_blookup_int`

**Signature:**
```c
int m_blookup_int(int buf, int key, void (*new) (void *, void *), void *ctx)
```

Looks up an integer in a sorted list using binary search and inserts
it if not found.

inserted.

**Parameters:**
- buf The handle of the sorted list.
- key The integer to look for.
- new Optional callback function called when a new element is
- ctx Context pointer for the callback.

**Returns:** The index of the integer in the list.

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

### `m_blookup_int_p`

**Signature:**
```c
void * m_blookup_int_p(int buf, int key, void (*new) (void *, void *),
		       void *ctx)
```

Similar to m_blookup_int, but returns a pointer to the element.

**Parameters:**
- buf The handle of the sorted list.
- key The integer to look for.
- new Optional callback function.
- ctx Context pointer.

**Returns:** A pointer to the element.

---

### `m_binsert_int`

**Signature:**
```c
int m_binsert_int(int buf, int key)
```

Inserts an integer into a sorted list using binary search.

**Parameters:**
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

### `m_count_allocated`

**Signature:**
```c
size_t m_count_allocated(void)
```

Returns the number of currently active (allocated, unfreed) handles.
Excludes slot 0 (the internal free list).

**Returns:** The count of active handles.

---

### `m_total_bytes`

**Signature:**
```c
size_t m_total_bytes(void)
```

Returns the total number of bytes allocated across all active
handles. Sums (capacity x width) for handles that own their memory
(no MFREE_NOALLOC).

**Returns:** The total allocated bytes.

---

### `m_peak_handles`

**Signature:**
```c
size_t m_peak_handles(void)
```

Returns the total number of handle slots in the master list.
Since slots are never removed, this equals the peak slot count.

**Returns:** The slot count.

---

### `m_debug_print`

**Signature:**
```c
void m_debug_print(FILE *fp)
```

Prints a tabular dump of all active handles to the given stream.
Useful for debugging leaks and understanding handle state.

**Parameters:**
- fp The output FILE stream (stderr if NULL).

---

### `debug_on_free`

**Signature:**
```c
static void debug_on_free(int realhdl)
```

Internal debug version of m_init.
Initializes the master list and the debug ownership list.

/
/* Called from mls_base's m_free() for every released handle. Marks the
handle freed in the debug list when it was freed inside library code
(list_free/tbl_free freeing owned values), so _m_destruct() does not
report false positives.

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

### `_m_alloc`

**Signature:**
```c
int _m_alloc(int ln, const char *fn, const char *fun, size_t n, size_t w,
	      uint8_t hfree)
```

Internal debug version of m_alloc.
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
Records caller information and marks the list as freed in the debug
tracker.

**Parameters:**
- ln Caller line number.
- fn Caller filename.
- fun Caller function name.
- m The handle to free.

**Returns:** 0.

---

### `_mls`

**Signature:**
```c
void * _mls(int ln, const char *fn, const char *fun, int h, size_t i)
```

Internal debug version of mls.
Records caller information and bounds checks the access.

**Parameters:**
- ln Caller line number.
- fn Caller filename.
- fun Caller function name.
- h The handle.
- i The index.

**Returns:** A pointer to the element.

---

### `_m_put`

**Signature:**
```c
int _m_put(int ln, const char *fn, const char *fun, int h, const void *d)
```

Internal debug version of m_put.
Records caller information.

**Parameters:**
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

### `_m_buf`

**Signature:**
```c
void * _m_buf(int ln, const char *fn, const char *fun, int m)
```

Internal debug version of m_buf.
Records caller information.

**Parameters:**
- ln Caller line number.
- fn Caller filename.
- fun Caller function name.
- m The handle.

**Returns:** A pointer to the data buffer.

---

### `_m_create`

**Signature:**
```c
int _m_create(int ln, const char *fn, const char *fun, size_t n, size_t w)
```

Internal debug version of m_create.
Records caller information and allocation source.

**Parameters:**
- ln Caller line number.
- fn Caller filename.
- fun Caller function name.
- n Initial maximum elements.
- w Element width.
- hfree The free handler ID.

**Returns:** The new handle.

---

## File: `m_tool.c`

### `m_regex`

**Signature:**
```c
int m_regex(int m, const char *regex, const char *s)
```

Executes a regular expression on a string and stores sub-matches in an
m-array.

allocated.

**Parameters:**
- m The handle of the m-array to store matches. If <= 1, a new one is
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

### `cmp_mstr`

**Signature:**
```c
int cmp_mstr(const void *a, const void *b)
```

qsort comparator for an m-array of char* strings (MFREE_STR).
Use with m_qsort(list, cmp_mstr): compares the pointed-to strings, so the
list ends up in strcmp order.

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

### `s_mdup`

**Signature:**
```c
int s_mdup(int h)
```

Copies a string buffer into a new string buffer.

**Parameters:**
- h The handle of the string buffer to copy.

**Returns:** The handle of the new string buffer.

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

Calculates the length of the initial segment of a string buffer which
consists entirely of characters in accept.

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

Calculates the length of the initial segment of a string buffer which
consists entirely of characters not in reject.

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

### `s_mcat`

**Signature:**
```c
int s_mcat(int h, int src)
```

Concatenates the content of a string buffer to a string buffer.

**Parameters:**
- h The handle of the string buffer. If <= 0, a new one is allocated.
- src The handle of the string buffer to concatenate.

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

TODO!
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

TODO!
Trims specified characters from both ends of a string buffer.

whitespace is trimmed.

**Parameters:**
- h The handle of the string buffer.
- chars C-style string containing characters to trim. If NULL,

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

allocated.

**Parameters:**
- m The handle to store the resulting strings. If 0, a new one is
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

### `s_subcmp`

**Signature:**
```c
int s_subcmp(int s0, int s0a, int s0b, int s1, int s1a, int s1b)
```

Compares two substrings of m-arrays.

**Parameters:**
- s0 First m-array handle.
- s0a Start index of first substring.
- s0b End index of first substring.
- s1 Second m-array handle.
- s1a Start index of second substring.
- s1b End index of second substring.

**Returns:** 0 if equal, <0 if s0 < s1, >0 if s0 > s1.

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

TODO!
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

### `s_msplit_trim`

**Signature:**
```c
int s_msplit_trim(int dest, int src, int pattern, int trim)
```

Like s_msplit, but optionally trims whitespace from each token in place
(s_trim) before returning. The destination array is MFREE_EACH: m_free
it once to release all token handles.

**Parameters:**
- dest Destination handle. If 0, a new one is allocated.
- src The source string buffer.
- pattern The pattern string buffer.
- trim If non-zero, s_trim is applied to each token.

**Returns:** The handle of the destination array.

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
- seperator Handle of the separator string buffer.

**Returns:** The handle of the destination string buffer.

---

### `s_implode_q`

**Signature:**
```c
int s_implode_q(int dest, int srcs, int seperator, char quote)
```

Like s_implode(), but optionally quotes every element. With quote == '\''
or '"' each element is wrapped in that character and any occurrence of it
inside the element is doubled, so the result stays well formed for SQL/CSV
(e.g. 'O''Brien'). quote == 0 joins without quoting, exactly like
s_implode(); an empty/NULL separator just concatenates the elements.

**Parameters:**
- dest Destination handle. If 0, a new one is allocated.
- srcs Handle of the m-array of string handles.
- seperator Handle of the separator string buffer.
- quote Quote character: 0 for none, or '\'' / '"'.

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

allocated.

**Parameters:**
- out Handle of the destination string buffer. If <= 0, a new one is
- s The C-style string.

**Returns:** The handle of the string buffer.

---

### `s_strcpy`

**Signature:**
```c
int s_strcpy(int out, int in)
```

Copies a MLS-style string into an string buffer.

allocated.

**Parameters:**
- out Handle of the destination string buffer. If <= 0, a new one is
- s The MLS-style string.

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

TODO!
print up to n charcaters of string

**Parameters:**
- m The handle of the string buffer.
- n The length

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

### `m_str_from_file`

**Signature:**
```c
int m_str_from_file(char *filename)
```

Reads the entire content of a file into a new string buffer.

**Parameters:**
- filename Path to the file.

**Returns:** The handle of the new string buffer, or -1 on error.

---

### `m_strlist_save`

**Signature:**
```c
int m_strlist_save(int list, const char *path)
```

Writes an m-array of char* strings (MFREE_STR, e.g. from s_split()) to a
text file, one element per line. NULL elements are skipped. This is the
inverse of m_strlist_load() for the common "one name per line" file.

ponytail: a '\n' or '\r' inside an element cannot be represented (it would
read back as extra lines); upgrade path is escaping or length-prefixing.
The file is truncated in place, so a crash mid-write loses the old content;
upgrade path is write-temp-then-rename.

**Parameters:**
- list Handle of the m-array of char* strings.
- path Output file path.

**Returns:** 0 on success, -1 if the file cannot be opened or closed.

---

### `m_strlist_load`

**Signature:**
```c
int m_strlist_load(const char *path)
```

Reads a text file into a fresh m-array of char* strings (MFREE_STR), one
element per line. A trailing carriage return is stripped, so CRLF files
load cleanly. The round-trip with m_strlist_save() is symmetric for any
element without a '\n' or '\r'. An empty file yields an empty (but valid)
list.

**Parameters:**
- path Input file path.

**Returns:** Handle of the new string list, or -1 if the file cannot be opened.

---

### `m_dir_list`

**Signature:**
```c
int m_dir_list(const char *path)
```

Lists the entries of a directory into a fresh m-array of char* strings
(MFREE_STR), like os.listdir(). "." and ".." are skipped; the order is the
filesystem readdir() order. This is the directory counterpart of
m_str_from_file().

opened.

**Parameters:**
- path Directory path.

**Returns:** Handle of the new string list, or -1 if the directory cannot be

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

### `dup_word`

**Signature:**
```c
static char * dup_word(char *a, char *b, int trimws)
```

Internal helper to duplicate a word between two pointers, optionally trimming
whitespace.

---

### `m_str_split`

**Signature:**
```c
int m_str_split(int ms, char *s, char *delim, int trimws)
```

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

Copies a range of elements from one m-array to another, handling width
differences.

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

### `s_read_fields`

**Signature:**
```c
int s_read_fields(int dest, FILE *fp, const char *sep, int strip)
```

Reads one line from a file and splits it into its fields, separated by the
literal string sep, into an m-array of string handles (MFREE_EACH: free the
result with m_free(), access a field with m_str (INT (list, i))). An empty
or NULL sep makes the whole line a single field. If strip is non-zero,
leading/trailing whitespace of every field is removed. A non-zero dest is
reused, its previous field handles freed first. Convenient for reading e.g.
CSV or colon-separated files (/etc/passwd with sep ":").

as a single field.
NULL) this is an empty list (0 fields), never EOF; test with
m_len (result) == 0.

**Parameters:**
- dest Handle of the resulting m-array, or 0 to allocate a new one.
- fp The file to read.
- sep The literal separator string; empty/NULL keeps the whole line
- strip Non-zero to trim whitespace around each field.

**Returns:** The handle of the m-array of fields. At end of input (or when fp is

---

### `ring_create`

**Signature:**
```c
int ring_create(int size)
```

Creates a new ring buffer.

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

Specialized free function for lists containing dynamically allocated strings
(char *). Frees each string in the list and optionally the list handle
itself.

but the handle remains. If zero, the handle itself is also freed.

**Parameters:**
- list The handle of the string list.
- CLEAR_ONLY If non-zero, the strings are freed and the list is cleared,

---

## File: `m_var.c`

### `vs_init`

**Signature:**
```c
int vs_init(void)
```

Creates a new, empty variable system.

**Returns:** The handle of the new variable system.

---

### `vs_free`

**Signature:**
```c
void vs_free(int vs)
```

Frees a variable system and all its variables.

Equivalent to m_free(vs): the MFREE_EACH / MFREE_STR handlers free the
variables and their values recursively, so a `tmls` handle cleans up the
whole system at block end.

**Parameters:**
- vs The variable system handle.

---

### `vs_find_key`

**Signature:**
```c
int vs_find_key(int vs, const char *name)
```

Finds the index of a variable by name.

**Parameters:**
- vs The variable system handle.
- name The variable name.

**Returns:** The index of the variable, or -1 if absent.

---

### `vs_lookup`

**Signature:**
```c
int vs_lookup(int vs, const char *name)
```

Looks up a variable by name, creating an empty one if needed.

**Parameters:**
- vs The variable system handle.
- name The variable name (must not be empty).

**Returns:** The variable handle, or -1 on an empty name.

---

### `vs_kset`

**Signature:**
```c
void vs_kset(int var, const char *value, int pos)
```

Sets a value at a position in a variable.

Element 0 holds the name; values live at positions 1..n. A negative or
out-of-range pos appends the value.

**Parameters:**
- var The variable handle.
- value The new value (copied; NULL stores NULL).
- pos The value position (use VS_APPEND to append).

---

### `vs_set`

**Signature:**
```c
int vs_set(int vs, const char *name, const char *value, int pos)
```

Sets a value by variable name, creating the variable if needed.

**Parameters:**
- vs The variable system handle.
- name The variable name.
- value The new value.
- pos The value position (use VS_APPEND to append).

**Returns:** The handle of the variable.

---

### `vs_vaset`

**Signature:**
```c
void vs_vaset(int vs, ...)
```

Sets multiple variables from (name, value) pairs, terminated by NULL.

**Parameters:**
- vs The variable system handle.
- ... Alternating name/value pairs, ending with a NULL name.

---

### `vs_kclr`

**Signature:**
```c
void vs_kclr(int var)
```

Drops all values of a variable, keeping its name.

**Parameters:**
- var The variable handle.

---

### `vs_clr`

**Signature:**
```c
void vs_clr(int vs, const char *name)
```

Drops all values of a variable selected by name.

**Parameters:**
- vs The variable system handle.
- name The variable name.

---

### `vs_kget`

**Signature:**
```c
char * vs_kget(int var, int pos)
```

Gets a value from a variable at a position.

**Parameters:**
- var The variable handle.
- pos The value position.

**Returns:** The value, or "" if absent.

---

### `vs_get`

**Signature:**
```c
char * vs_get(int vs, const char *name, int pos)
```

Gets a value by variable name.

**Parameters:**
- vs The variable system handle.
- name The variable name.
- pos The value position.

**Returns:** The value, or "" if absent.

---

### `vs_klen`

**Signature:**
```c
int vs_klen(int var)
```

Returns the number of values in a variable (excluding its name).

**Parameters:**
- var The variable handle.

**Returns:** The number of values.

---

### `vs_len`

**Signature:**
```c
int vs_len(int vs, const char *name)
```

Returns the number of values of a variable selected by name.

**Parameters:**
- vs The variable system handle.
- name The variable name.

**Returns:** The number of values.

---

### `vs_remove`

**Signature:**
```c
void vs_remove(int vs, const char *name)
```

Removes a variable and frees its values.

**Parameters:**
- vs The variable system handle.
- name The variable name.

---

### `vs_exp_init`

**Signature:**
```c
void vs_exp_init(vs_exp_t *se)
```

Initializes an expansion context. Every vs_exp_t must start here.

**Parameters:**
- se The expansion context.

---

### `vs_exp_free`

**Signature:**
```c
void vs_exp_free(vs_exp_t *se)
```

Frees the buffers of an expansion context.

**Parameters:**
- se The expansion context.

---

### `vs_exp_parse`

**Signature:**
```c
void vs_exp_parse(vs_exp_t *se, const char *frm)
```

Parses a format string into literal and $-token parts.

Supports $name, $name[N], $name[*], quoted $'name' and escaped \$:
- $name        value of the current row (see vs_expand)
- $name[N]     N-th value (0-based among the values)
- $name[*]     every value joined with commas
- $'name'      quoted; a bare quoted name joins every value quoted
- \$           literal dollar sign

**Parameters:**
- se The expansion context.
- frm The format string.

---

### `vs_expand`

**Signature:**
```c
char * vs_expand(vs_exp_t *se, int vs, int row)
```

Expands a parsed format string using the values of a variable system.

The result lives in se->buf and stays valid until the next expansion or
vs_exp_free. Set se.sql = 1 before parsing/expanding for SQL quoting.

**Parameters:**
- se The parsed expansion context.
- vs The variable system handle.
- row Row used for a $-token without an explicit index.

**Returns:** Pointer to the expanded string.

---

### `vs_string`

**Signature:**
```c
char * vs_string(int vs, const char *frm)
```

Parses and expands a format string in one call.

The result is stored in the variable system (under "vs_result") so the
returned pointer outlives the internal buffer.

**Parameters:**
- vs The variable system handle.
- frm The format string.

**Returns:** The expanded string.

---

### `vs_sql_string`

**Signature:**
```c
char * vs_sql_string(int vs, const char *frm)
```

Like vs_string(), but quoted fields use SQL quoting (single quotes doubled).

**Parameters:**
- vs The variable system handle.
- frm The format string.

**Returns:** The expanded string.

---

### `vs_add`

**Signature:**
```c
int vs_add(int vs, const char *name, const char *const *list, int n)
```

Adds n C-strings as the values of a variable.

Creates the variable system when vs <= 0; values are appended to any
existing ones.

**Parameters:**
- vs The variable system handle, or 0.
- name The variable name.
- list The C-string array (may be NULL).
- n Number of strings.

**Returns:** The variable system handle.

---

### `vs_string_builder`

**Signature:**
```c
int vs_string_builder(int strh, int vs, const char *frm)
```

Expands a format string and appends the result to a string handle.

**Parameters:**
- strh Destination string handle, or 0 to allocate a new one.
- vs The variable system handle.
- frm The format string.

**Returns:** The destination string handle.

---

### `vs_sql_string_builder`

**Signature:**
```c
int vs_sql_string_builder(int strh, int vs, const char *frm)
```

Like vs_string_builder(), but quoted fields use SQL quoting.

**Parameters:**
- strh Destination string handle, or 0 to allocate a new one.
- vs The variable system handle.
- frm The format string.

**Returns:** The destination string handle.

---

### `vs_read_fields`

**Signature:**
```c
int vs_read_fields(int vs, const char *name, FILE *fp, const char *sep)
```

Reads one line and stores its fields as the values of a variable.

The line is split on the literal sep and each field is whitespace-trimmed.
Previous values are dropped. An empty line yields one empty field; at EOF
the variable ends up with 0 values (the loop-end signal, analogous to an
empty s_read_fields). vs must already be initialized.

**Parameters:**
- vs The variable system handle.
- name The variable name.
- fp The input stream.
- sep The literal separator (empty/NULL: whole line is one field).

**Returns:** The variable handle.

---

## File: `m_sql.c`

### `sql_cols`

**Signature:**
```c
int sql_cols(const char *spec)
```

@brief Split a terse column spec into a string list.

Separators are whitespace and commas; runs and leading/trailing separators
are ignored, so "x y z", "x,y,z" and " x , y  z " all give [x, y, z].

**Parameters:**
- spec The column spec (may be NULL or empty -> empty list).

**Returns:** An m-array of char* (MFREE_STR): free it with m_free().

---

### `sql_select`

**Signature:**
```c
int sql_select(const char *table, const char *cols, const char *where)
```

@brief Build a SELECT statement from a table and a terse column spec.

The column spec is expanded by sql_cols() and joined with ", "; the table
name and the optional where clause are emitted verbatim.

**Parameters:**
- table The table name (used verbatim).
- cols The terse column spec, e.g. "id name qty".
- where Optional WHERE clause without the keyword; NULL or "" omits it.

**Returns:** A string handle "SELECT ... FROM ..."; free it with m_free().

---

### `sql_insert`

**Signature:**
```c
int sql_insert(int vs, const char *table, const char *cols)
```

@brief Build an INSERT statement, with values expanded from a variable
system.

The column list is built like sql_select(); the VALUES list is the
expansion of "$'vals[*]'" against vs, so every value is SQL-quoted by
vs_sql_string_builder() (single quotes doubled: O'Brien -> 'O''Brien').
Populate the values with vs_add(vs, "vals", list, n) or
vs_read_fields(vs, "vals", ...).

m_free().

**Parameters:**
- vs The variable system holding the values under the variable "vals".
- table The table name (used verbatim).
- cols The terse column spec, matching the values in "vals".

**Returns:** A string handle "INSERT INTO ... (...) VALUES (...)"; free it with

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

whitespace.

**Parameters:**
- h The string handle.
- chars A string containing characters to trim. If NULL, trims

**Returns:** A new handle containing the trimmed string.

---

### `s_trim_right_c`

**Signature:**
```c
int s_trim_right_c(int h, const char *chars)
```

Trims specific characters from the right side of a string.

whitespace.

**Parameters:**
- h The string handle.
- chars A string containing characters to trim. If NULL, trims

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

**Parameters:**
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

### `s_from_double`

**Signature:**
```c
int s_from_double(double val)
```

Converts a double value to a string handle.
Uses %g format which removes trailing zeros.

**Parameters:**
- val The double value.

**Returns:** A new string handle.

---

### `s_from_int`

**Signature:**
```c
int s_from_int(int val)
```

Converts an int value to a string handle.

**Parameters:**
- val The int value.

**Returns:** A new string handle.

---

