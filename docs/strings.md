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

Got some ugly whitespace? `s_trim` will fix it in one go.

```c
int s = m_alloc(0, 1, MFREE);
s_printf(s, 0, "   too much space   ");
s_trim(s); // Result: "too much space"
```

## Why this is Awesome:
1. **No Buffer Overflows:** Every function checks the size of the destination list.
2. **Automatic Null Termination:** `s_` functions always ensure your string ends with a `0`.
3. **Easy Cleanup:** Just call `m_free(s)` and you're done. No need to track multiple `char*` pointers.

---

[Back to Home](README.md)
