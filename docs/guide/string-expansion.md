# Powerful String Expansion with `vs_string`

The `mls` library includes a sophisticated string expansion system (module
`m_var`) that allows you to embed variables and array elements directly into
your strings. This is similar to string interpolation in modern languages like
Python or Ruby.

> The old `v_*` / `se_*` API was removed; everything now lives under the `vs_`
> prefix in `m_var.h`.

## 1. Basic Variable Expansion

Use the `$` prefix followed by a variable name to insert its value.

```c
int vs = vs_init();
vs_set(vs, "user", "Alice", 1);

char *res = vs_string(vs, "Welcome, $user!");
// Result: "Welcome, Alice!"
```

## 2. Array Indexing

If a variable contains multiple values (rows), you can access them using
`[index]`.

```c
int vs = vs_init();
vs_set(vs, "colors", "red", 1);
vs_set(vs, "colors", "green", 2);
vs_set(vs, "colors", "blue", 3);

char *res = vs_string(vs, "My favorite color is $colors[1].");
// Result: "My favorite color is green."
// Note: [0] refers to the current row context, [1] to the first explicit row, etc.
```

## 3. Quoting and Escaping

Use `$'var'` to wrap the expanded value in single quotes and escape special
characters within it.

```c
int vs = vs_init();
vs_set(vs, "input", "it's a \"test\"", 1);

char *res = vs_string(vs, "Data: $'input'");
// Result: "Data: 'it\'s a \"test\"'"
```

For SQL, use `vs_sql_string()` / `vs_sql_string_builder()`: single quotes are
doubled (`O'Brien` -> `'O''Brien'`), which both MySQL and SQLite accept.

```c
char *sql = vs_sql_string(vs, "select * from t where name = $'input'");
```

To include a literal `$` sign, escape it with a backslash: `\$`.

```c
char *res = vs_string(vs, "Total: \$100");
// Result: "Total: $100"
```

## 4. Full Array Expansion

Use `[*]` to expand all values in a variable as a comma-separated list. A bare
quoted name (`$'tags'`) expands to the quoted list.

```c
int vs = vs_init();
vs_set(vs, "tags", "c", 1);
vs_set(vs, "tags", "linux", 2);
vs_set(vs, "tags", "mls", 3);

char *res = vs_string(vs, "Tags: $tags[*]");
// Result: "Tags: c,linux,mls"
```

## 5. Working with Rows

When using `vs_expand` directly, you can pass a `row` parameter to determine
which index `$var` (without brackets) should use.

```c
vs_exp_t se;
vs_exp_init(&se);
vs_exp_parse(&se, "Row value: $col");

char *res0 = vs_expand(&se, vs, 0); // Uses index 0
char *res1 = vs_expand(&se, vs, 1); // Uses index 1
vs_exp_free(&se);
```

## 6. Build Helpers (CSV -> SQL)

`vs_add()` fills a variable with a C-string array and `vs_string_builder()`
appends an expansion to a string handle; `vs_read_fields()` reads one line into
a variable. That is enough to turn CSV into SQL inserts without `s_implode_q`:

```c
static const char *cols[] = {"id", "name", "cnt"};
int vs = vs_add(0, "cols", cols, 3);
int buf = vs_string_builder(0, vs, "insert into t ( $cols[*] ) values (");
for (;;) {
    int f = vs_read_fields(vs, "row", stdin, ",");
    if (vs_klen(f) == 0)
        break;
    vs_sql_string_builder(buf, vs, "$'row[*]' );");
    s_puts(buf);
}
```

---

[Back to Strings](strings.md) | [Back to Home](../../README.md)
