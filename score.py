#!/usr/bin/env python3
"""score.py: compute the "nesting + pointer indirection" metric per C function.

Usage:  score.py FILE.c
        score.py --selftest

For every function found it prints the c(i) nesting histogram, the p(k)
pointer-indirection histogram, the two score factors, and the final score.

    score(f) = ( sum_{i>=1} c(i) * 2^(i-1) ) * ( prod_{k>=1} (k+1)^p(k) )

c(i) = number of code lines of f at nesting (brace) depth i.
       Blank/comment-only/lone-brace lines are excluded; depth-0 lines are
       ignored (the sum starts at i=1).
p(k) = number of pointer objects in f (params + locals) with indirection
       level k: `int *s` -> 1, `int **s` -> 2, `int ***p` -> 3.
"""

import sys

KEYWORDS = frozenset("""
auto break case char const continue default do double else enum extern float
for goto if inline int long register restrict return short signed sizeof
static struct switch typedef union unsigned void volatile while _Bool
""".split())

TYPE_TOKENS = frozenset(
    "void char short int long float double signed unsigned _Bool struct union enum".split())

QUALIFIERS = frozenset("const volatile restrict static extern register auto inline".split())


def tokenize(src):
    """List of (kind, value, line). Comments and preprocessor lines dropped;
    string/char literals are single tokens so braces inside them don't count."""
    toks = []
    i, n, line = 0, len(src), 1
    while i < n:
        c = src[i]
        if c == '\n':
            line += 1
            i += 1
            continue
        if c.isspace():
            i += 1
            continue
        if c == '#' or src.startswith('//', i):
            j = src.find('\n', i)
            if j == -1:
                break
            line += 1
            i = j + 1
            continue
        if src.startswith('/*', i):
            j = src.find('*/', i + 2)
            if j == -1:
                line += src[i:].count('\n')
                break
            line += src[i:j + 2].count('\n')
            i = j + 2
            continue
        if c in ('"', "'"):
            j = i + 1
            while j < n:
                if src[j] == '\\':
                    j += 2
                    continue
                if src[j] == c:
                    j += 1
                    break
                j += 1
            toks.append(('str', src[i:j], line))
            i = j
            continue
        if c.isalpha() or c == '_':
            j = i + 1
            while j < n and (src[j].isalnum() or src[j] == '_'):
                j += 1
            toks.append(('id', src[i:j], line))
            i = j
            continue
        if c.isdigit():
            j = i + 1
            while j < n and (src[j].isalnum() or src[j] in '._'):
                j += 1
            toks.append(('num', src[i:j], line))
            i = j
            continue
        toks.append(('op', c, line))
        i += 1
    return toks


def find_matching_close(toks, open_idx):
    depth = 1
    for j in range(open_idx + 1, len(toks)):
        if toks[j][1] == '{':
            depth += 1
        elif toks[j][1] == '}':
            depth -= 1
            if depth == 0:
                return j
    return None


def find_functions(toks):
    """(name, line, param_toks, body_toks) for each function definition."""
    funcs = []
    n = len(toks)
    i = 0
    while i < n:
        k, v, ln = toks[i]
        if k == 'id' and v not in KEYWORDS and i + 1 < n and toks[i + 1][1] == '(':
            j, depth = i + 2, 1
            while j < n and depth:
                if toks[j][1] == '(':
                    depth += 1
                elif toks[j][1] == ')':
                    depth -= 1
                j += 1
            if j < n and toks[j][1] == '{':
                close = find_matching_close(toks, j)
                if close is not None:
                    funcs.append((v, ln, toks[i + 2:j - 1], toks[j + 1:close]))
                    i = close + 1
                    continue
        i += 1
    return funcs


def split_top(toks, sep):
    parts, cur, depth = [], [], 0
    for t in toks:
        if t[1] == '(':
            depth += 1
        elif t[1] == ')':
            depth -= 1
        elif t[1] == sep and depth == 0:
            parts.append(cur)
            cur = []
            continue
        cur.append(t)
    parts.append(cur)
    return parts


def param_pointers(param_toks):
    levels = []
    for part in split_top(param_toks, ','):
        ids = [(i, t[1]) for i, t in enumerate(part) if t[0] == 'id']
        if not ids:
            continue
        pos, name = ids[-1]
        if name in KEYWORDS:  # 'void', 'char', ... : not a variable name
            continue
        stars = sum(1 for t in part[:pos] if t[0] == 'op' and t[1] == '*')
        if stars:
            levels.append(stars)
    return levels


def is_declaration(stmt):
    if not stmt:
        return False
    k, v = stmt[0][0], stmt[0][1]
    if k != 'id':
        return False
    if v in TYPE_TOKENS:
        return True
    if v in KEYWORDS:
        return False
    # ponytail: unknown leading id treated as a typedef'd type name, unless it
    # looks like a call/assignment. Ceiling: typedef'd function-pointer types
    # are still missed; the fix is a real C parser.
    if len(stmt) >= 2:
        v2 = stmt[1][1]
        if v2 in ('=', '(', ')', '[', ']', '{'):
            return False
    return True


def declarator_levels(stmt):
    levels = []
    for part in split_top(stmt, ','):
        decl = [t for t in part if t[1] != '=']  # drop initializers
        for i, t in enumerate(decl):
            if t[0] == 'id' and t[1] not in KEYWORDS:
                stars = sum(1 for x in decl[:i] if x[0] == 'op' and x[1] == '*')
                if stars:
                    levels.append(stars)
                break
    return levels


def local_pointers(body_toks):
    levels = []
    depth, start = 0, 0
    for i, (_, v, _) in enumerate(body_toks):
        if v == '(':
            depth += 1
        elif v == ')':
            depth -= 1
        elif v == ';' and depth == 0:
            stmt = body_toks[start:i]
            if is_declaration(stmt):
                levels.extend(declarator_levels(stmt))
            start = i + 1
    return levels


def metric(name, line, param_toks, body_toks):
    p = {}
    for lvl in param_pointers(param_toks) + local_pointers(body_toks):
        p[lvl] = p.get(lvl, 0) + 1

    lines = {}
    for k, v, ln in body_toks:
        lines.setdefault(ln, []).append((k, v))

    c = {}
    depth = 1  # body is inside the function's opening brace
    for ln in sorted(lines):
        toks = lines[ln]
        if all(t[1] in '{}' for t in toks):  # lone-brace line
            for _, tv in toks:
                depth += 1 if tv == '{' else -1
            continue
        if depth >= 1:
            c[depth] = c.get(depth, 0) + 1
        for _, tv in toks:
            if tv == '{':
                depth += 1
            elif tv == '}':
                depth -= 1

    sum_part = sum(v * (2 ** (i - 1)) for i, v in c.items())
    prod_part = 1
    for kk, vv in p.items():
        prod_part *= (kk + 1) ** vv
    return c, p, sum_part, prod_part, sum_part * prod_part


def fmt_c(c):
    if not c:
        return "0"
    return " + ".join(f"{v}*2^{i-1}" for i, v in sorted(c.items()))


def fmt_p(p):
    if not p:
        return "1"
    return " * ".join(f"{k+1}^{v}" for k, v in sorted(p.items()))


def report(path):
    with open(path) as fh:
        src = fh.read()
    funcs = find_functions(tokenize(src))
    if not funcs:
        print("no functions found")
        return
    for name, ln, ptoks, btoks in funcs:
        c, p, sp, pp, score = metric(name, ln, ptoks, btoks)
        print(f"{name} (line {ln})")
        print(f"  c(i): {dict(sorted(c.items())) if c else {}}")
        print(f"  p(k): {dict(sorted(p.items())) if p else {}}")
        print(f"  nesting term: {fmt_c(c)} = {sp}")
        print(f"  pointer term: {fmt_p(p)} = {pp}")
        print(f"  score: {score}")
        print()


SAMPLE = r"""
int *foo(char **a, int *b) {
    int **x;
    int y;
    if (y) {
        y++;
    }
    return a[0] ? *b : y;
}
"""


def selftest():
    funcs = find_functions(tokenize(SAMPLE))
    assert len(funcs) == 1, funcs
    name, ln, ptoks, btoks = funcs[0]
    assert name == 'foo' and ln == 2
    c, p, sp, pp, score = metric(name, ln, ptoks, btoks)
    assert c == {1: 4, 2: 1}, c
    assert p == {1: 1, 2: 2}, p
    assert sp == 6, sp            # 4*2^0 + 1*2^1
    assert pp == 18, pp           # 2^1 * 3^2
    assert score == 108, score
    print("selftest ok")


if __name__ == '__main__':
    if len(sys.argv) >= 2 and sys.argv[1] == '--selftest':
        selftest()
    elif len(sys.argv) == 2:
        report(sys.argv[1])
    else:
        print(__doc__)
        sys.exit(1)
