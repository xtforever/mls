#!/usr/bin/env python3
"""Build one readable PDF from the You Do Not Know C papers.

Combines:
    draft-paper-you-do-not-know-c.md   (thesis)
    contra-you-do-not-know-c.md        (antithesis)
    handles-paper.md                   (manifesto)

into you-do-not-know-c.pdf with a cover, table of contents, running
headers, page numbers, and PDF bookmarks.

Requires: python3-markdown, weasyprint.
"""
import pathlib
import re

import markdown
from markdown.extensions.toc import TocExtension
from weasyprint import HTML

ROOT = pathlib.Path(__file__).resolve().parent
OUT = ROOT / "you-do-not-know-c.pdf"

DOCS = [
    ("I", "Thesis", "draft-paper-you-do-not-know-c.md"),
    ("II", "Antithesis", "contra-you-do-not-know-c.md"),
    ("III", "Manifesto", "handles-paper.md"),
]


def prefix_tokens(tokens, prefix):
    for t in tokens:
        t["id"] = f"{prefix}-{t['id']}"
        prefix_tokens(t["children"], prefix)


def render(md_path, prefix):
    md = markdown.Markdown(extensions=["extra", "sane_lists", TocExtension()])
    html = md.convert(md_path.read_text(encoding="utf-8"))
    tokens = md.toc_tokens
    prefix_tokens(tokens, prefix)
    # Prefix the heading ids in the HTML so they match the (prefixed) tokens.
    html = re.sub(
        r'(<h[1-6]\b[^>]*\bid=")([^"]+)"',
        lambda m: f'{m.group(1)}{prefix}-{m.group(2)}"',
        html,
    )
    # Mark the document title so the running header follows it.
    html = re.sub(r"<h1\b", '<h1 class="docTitle"', html, count=1)
    return html, tokens


def flatten(tokens):
    out = []
    for t in tokens:
        out.append((t["level"], t["id"], t["name"]))
        out.extend(flatten(t["children"]))
    return out


cover = """
<section class="cover">
  <div class="kicker">A Debate and a Manifesto</div>
  <h1 class="cover-title">You Do Not Know C</h1>
  <p class="cover-sub">The memory-safety debate, the hostile reply,<br>
     and the case for handles</p>
  <div class="cover-rule"></div>
  <p class="cover-parts">
    <strong>I&nbsp;&middot;&nbsp;Thesis</strong> &mdash; You Do Not Know C<br>
    <strong>II&nbsp;&middot;&nbsp;Antithesis</strong> &mdash; The Carriage Manifesto<br>
    <strong>III&nbsp;&middot;&nbsp;Manifesto</strong> &mdash; Pointers Considered Harmful
  </p>
  <p class="cover-meta">Draft &middot; 2026</p>
</section>
"""

parts = []
toc_rows = []
for i, (num, label, name) in enumerate(DOCS, 1):
    prefix = f"p{i}"
    html, tokens = render(ROOT / name, prefix)
    for level, ident, title in flatten(tokens):
        if level <= 2:
            cls = "lvl1" if level == 1 else "lvl2"
            toc_rows.append(f'<li class="{cls}"><a href="#{ident}">{title}</a></li>')
    first = " first" if i == 1 else ""
    parts.append(
        f'<section class="part{first}">'
        f'<div class="part-label">Part {num} &middot; {label}</div>'
        f"{html}</section>"
    )

CSS = """\
@page {
  size: A4;
  margin: 20mm 18mm 18mm 18mm;
  @top-center { content: string(doctitle); font-size: 8.5pt; color: #999; letter-spacing: .4px; }
  @bottom-center { content: counter(page); font-size: 9pt; color: #888; }
}
@page :first { @top-center { content: none; } @bottom-center { content: none; } }

body { font-family: Georgia, "DejaVu Serif", serif; font-size: 11pt; line-height: 1.55;
       color: #1b1b1b; margin: 0; }
p { margin: 0 0 8pt; text-align: justify; hyphens: auto; }
h1, h2, h3, h4 { font-family: "DejaVu Sans", Arial, sans-serif; color: #111; line-height: 1.25; }
h1 { font-size: 21pt; margin: 0 0 12pt; bookmark-level: 1; bookmark-label: content(text); }
h2 { font-size: 14.5pt; margin: 18pt 0 6pt; padding-bottom: 3pt; border-bottom: 1px solid #ddd;
     bookmark-level: 2; bookmark-label: content(text); break-after: avoid; }
h3 { font-size: 12.5pt; margin: 14pt 0 4pt; bookmark-level: 3; bookmark-label: content(text);
     break-after: avoid; }
ul, ol { margin: 0 0 8pt; padding-left: 18pt; }
li { margin-bottom: 3pt; }
a { color: #1a4d8f; text-decoration: none; }
code { font-family: "DejaVu Sans Mono", monospace; font-size: 9.5pt;
       background: #f2f2f2; padding: 1px 3px; border-radius: 3px; }
pre { background: #f6f8fa; border: 1px solid #e1e4e8; border-radius: 5px;
      padding: 8pt 10pt; font-size: 9pt; break-inside: avoid; }
pre code { background: none; padding: 0; }
blockquote { margin: 10pt 0; padding: 4pt 12pt; border-left: 3px solid #c8c8c8;
             background: #fafafa; color: #333; break-inside: avoid; }
table { border-collapse: collapse; width: 100%; margin: 10pt 0; font-size: 9pt;
        break-inside: avoid; }
th, td { border: 1px solid #ccc; padding: 4pt 6pt; text-align: left; vertical-align: top; }
th { background: #f0f0f0; }
hr { border: 0; border-top: 1px solid #ddd; margin: 14pt 0; }
img { max-width: 100%; }

.docTitle { string-set: doctitle content(); bookmark-level: 1; bookmark-label: content(text); }

.cover { break-after: page; text-align: center; padding-top: 55mm; }
.cover .kicker { font-family: "DejaVu Sans", Arial, sans-serif; font-size: 10pt;
                 letter-spacing: 3px; text-transform: uppercase; color: #888; }
.cover .cover-title { font-size: 36pt; margin: 8pt 0 6pt; bookmark-level: none; }
.cover .cover-sub { font-size: 13pt; color: #555; font-style: italic; }
.cover .cover-rule { width: 55mm; height: 3px; background: #333; margin: 20pt auto; }
.cover .cover-parts { font-size: 11pt; color: #333; line-height: 1.9; }
.cover .cover-meta { margin-top: 45pt; font-size: 10pt; color: #777; }

.toc { break-after: page; }
.toc h1 { bookmark-level: none; }
.toc ol { list-style: none; padding-left: 0; }
.toc li { margin: 0.5pt 0; }
.toc .lvl1 { font-family: "DejaVu Sans", Arial, sans-serif; font-weight: bold;
             margin-top: 8pt; font-size: 11pt; }
.toc .lvl2 { padding-left: 18pt; font-size: 9.3pt; color: #444; }

.part { break-before: page; }
.part.first { break-before: avoid; }
.part-label { font-family: "DejaVu Sans", Arial, sans-serif; font-size: 9.5pt;
              letter-spacing: 2px; text-transform: uppercase; color: #999;
              border-bottom: 2px solid #333; padding-bottom: 5pt; margin-bottom: 12pt; }
"""

html = f"""<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<title>You Do Not Know C &mdash; A Debate and a Manifesto</title>
<style>{CSS}</style></head>
<body>
{cover}
<section class="toc"><h1>Contents</h1><ol>
{chr(10).join(toc_rows)}
</ol></section>
{"".join(parts)}
</body></html>
"""

HTML(string=html, base_url=str(ROOT)).write_pdf(str(OUT))
print(f"wrote {OUT} ({OUT.stat().st_size // 1024} KiB)")
