# Powerful String Expansion with `se_string`

The `mls` library includes a sophisticated string expansion system that allows you to embed variables and array elements directly into your strings. This is similar to string interpolation in modern languages like Python or Ruby.

## 1. Basic Variable Expansion

Use the `$` prefix followed by a variable name to insert its value.

```c
int vs = v_init();
v_set(vs, "user", "Alice", 1);

char *res = se_string(vs, "Welcome, $user!");
// Result: "Welcome, Alice!"
```

## 2. Array Indexing

If a variable contains multiple values (rows), you can access them using `[index]`.

```c
int vs = v_init();
v_set(vs, "colors", "red", 1);
v_set(vs, "colors", "green", 2);
v_set(vs, "colors", "blue", 3);

char *res = se_string(vs, "My favorite color is $colors[1].");
// Result: "My favorite color is green."
// Note: [0] refers to the current row context, [1] to the first explicit row, etc.
```

## 3. Quoting and Escaping

Use `$'var'` to automatically wrap the expanded value in single quotes and escape any special characters (like quotes or backslashes) within the value.

```c
int vs = v_init();
v_set(vs, "input", "it's a \"test\"", 1);

char *res = se_string(vs, "Data: $'input'");
// Result: "Data: 'it\'s a \"test\"'"
```

To include a literal `$` sign, escape it with a backslash: `\$`.

```c
char *res = se_string(vs, "Total: \$100");
// Result: "Total: $100"
```

## 4. Full Array Expansion

Use `[*]` to expand all values in a variable as a comma-separated list.

```c
int vs = v_init();
v_set(vs, "tags", "c", 1);
v_set(vs, "tags", "linux", 2);
v_set(vs, "tags", "mls", 3);

char *res = se_string(vs, "Tags: $tags[*]");
// Result: "Tags: c,linux,mls"
```

## 5. Working with Rows

When using `se_expand` directly, you can pass a `row` parameter to determine which index `$var` (without brackets) should use.

```c
str_exp_t se;
se_init(&se);
se_parse(&se, "Row value: $col");

char *res0 = se_expand(&se, vs, 0); // Uses index 0
char *res1 = se_expand(&se, vs, 1); // Uses index 1
```

---

[Back to Strings](strings.md) | [Back to Home](../README.md)
