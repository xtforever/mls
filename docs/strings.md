# String Handling without the Headaches

In C, strings are just pointers to bytes that end with a `0`. This is simple, but it's also where most bugs live. `mls` treats strings as first-class citizens using integer handles, making them as easy to use as in a high-level language.

## 1. Creating a String

Stop worrying about `malloc` and `strlen`. Just use `s_printf` to build your string on the fly.

```c
int s = m_alloc(0, 1, MFREE);
s_printf(s, 0, "Hello MLS");
// Now 's' is a handle. It's safe, it's tracked, it's modifiable and it's happy.
```

## 2. Concatenation (The "Glue" Function)

Standard C's `strcat` is dangerous and slow. `s_app` is safe, fast, and takes as many arguments as you want!

```c
int s = m_alloc(0, 1, MFREE);
s_printf(s, 0, "I love ");
s_app(s, "C", " but ", "I hate ", "segfaults", NULL);
// Result: "I love C but I hate segfaults"
```
*Note: Always end `s_app` with `NULL` so it knows when to stop!*

## 3. Formatted Strings (The Powerhouse)

Need to build a string from variables? `s_printf` is your friend. It's like `sprintf`, but it handles all the memory resizing for you. 

**Pro Tip:** If you pass `0` as the handle, MLS will automatically create a new list for you and return its handle.

```c
// Create a new string on the fly
int msg = s_printf(0, 0, "Error %d in file %s", 404, "index.c");

// Or overwrite/append to an existing one
s_printf(msg, 0, "New message: %s", "Operation successful");
```

## 4. Substrings (The Safe Slice)

We've already seen `s_slice`, but it's worth repeating: it's the safest way to get a piece of a string.

```c
int s = m_alloc(0, 1, MFREE);
s_printf(s, 0, "Banana");
int sub = s_slice(0, 0, s, 0, 1); // Result: "Ba"
```

## 5. Cleaning Up

Got some ugly whitespace? `s_trim` will fix it in one go. You can also use `s_trim_left_c` or `s_trim_right_c` for more control.

```c
int s = s_dup("   too much space   ");
s_trim(s); // Result: "too much space"
```

## 6. Advanced Utilities (`m_extra`)

For more complex tasks, `mls` includes additional string functions:

-   **Case-Insensitive Comparison:** `s_casecmp(a, b)` and `s_ncasecmp(a, b, n)` for comparing strings regardless of case.
-   **Padding:** `s_pad_left(s, 10, '*')` and `s_pad_right(s, 10, '-')` to reach a specific width.
-   **Reversal:** `s_reverse(s)` flips the string in-place.
-   **Numeric Conversion:** `s_to_long(s)` safely converts a string to a long integer.
-   **Classification:** `s_is_numeric(s)` and `s_is_alpha(s)` to check string content.

```c
int s = s_dup("123");
if (s_is_numeric(s)) {
    long val = s_to_long(s); // 123L
}
```

## 7. String Expansion

For a truly modern experience, `mls` provides a powerful string interpolation system via `se_string`.

```c
int vs = v_init();
v_set(vs, "name", "John", 1);
char *res = se_string(vs, "Hello $name!"); // Result: "Hello John!"
```
[Learn more about String Expansion](string_expansion.md)

## Why this is Awesome:
1. **No Buffer Overflows:** Every function checks the size of the destination list.
2. **Automatic Null Termination:** `s_` functions always ensure your string ends with a `0`.
3. **Easy Cleanup:** Just call `m_free(s)` and you're done. No need to track multiple `char*` pointers.

---

[Back to Home](README.md)
