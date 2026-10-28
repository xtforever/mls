#!/usr/bin/env python3
"""Terminal-style code animation renderer.

Reads a simple line-oriented *script* describing one or more scenes:

    # a comment
    clear                 start a new scene (screen is cleared)
    pause 2.5s            hold the current screen for N seconds
    text                  begin an explanatory-text block (until "end")
    ...
    end
    code                  begin a C code block (until "end")
    ...
    end
    highlight 3           outline code line 3 (replaces the previous outline)
    highlight off         remove the current outline

Text is rendered in warm off-white bold; code is rendered in green with
subtle syntax highlighting and automatic line numbers. Both reveal
character by character and fade from dark to bright. Frames stream to
ffmpeg as raw RGB24 -> MP4.

Run `python3 render.py --selftest` for a self-contained check.
Entry point: see show.py.
"""
from __future__ import annotations

import argparse
import math
import os
import subprocess
import sys
import unicodedata
from dataclasses import dataclass, field
from typing import NamedTuple

from PIL import Image, ImageDraw, ImageFilter, ImageFont
from pygments.lexers.c_cpp import CLexer
from pygments.token import Token

# --- Colors ------------------------------------------------------------------

DARK = (0, 40, 0)            # code fade start (dark green)
BASE = (0, 200, 60)          # default code green
TEXT = (235, 235, 215)       # explanatory text (warm off-white)
TEXT_DIM = (40, 40, 32)      # text fade start

# Ordered most-specific first. `ttype in parent` matches subtokens too.
_COLOR_MAP = [
    (Token.Comment.Preproc, (140, 220, 90)),
    (Token.Keyword.Type,    (120, 240, 150)),
    (Token.Keyword,         (150, 255, 170)),
    (Token.Name.Function,   (170, 255, 200)),
    (Token.Literal.String,  (80, 200, 120)),
    (Token.Number,          (100, 230, 140)),
    (Token.Comment,         (30, 120, 60)),
    (Token.Name,            BASE),
    (Token.Operator,        BASE),
    (Token.Punctuation,     BASE),
]


def color_for(ttype):
    for parent, color in _COLOR_MAP:
        if ttype in parent:
            return color
    return BASE


def lerp(a, b, t):
    return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(3))


def ramp(age, fade_time):
    """Clamped smoothstep fade: 0 at age<=0, 1 at age>=fade_time."""
    if age <= 0.0:
        return 0.0
    if age >= fade_time:
        return 1.0
    x = age / fade_time
    return x * x * (3.0 - 2.0 * x)


# --- Config / content model --------------------------------------------------

@dataclass
class Config:
    width: int = 1920
    height: int = 1080
    fps: int = 30
    margin: int = 90
    font_path: str = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"
    bold_font_path: str = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf"
    font_size: int = 40
    fade_time: float = 0.28
    tail_hold: float = 1.0
    inter_scene_pause: float = 0.6
    tabstop: int = 4
    block_gap: int = 1           # blank lines between blocks of a scene
    bloom: bool = False
    cursor: bool = False
    text_color: tuple = TEXT
    text_dim: tuple = TEXT_DIM
    text_bold: bool = True
    code_char_delay: float = 0.021
    code_line_delay: float = 0.09
    text_char_delay: float = 0.030
    text_line_delay: float = 0.12
    gutter_color: tuple = (70, 125, 80)
    highlight_color: tuple = (255, 200, 40)
    highlight_width: int = 2
    highlight_pad: int = 3


@dataclass
class Block:
    kind: str                     # "code" | "text"
    lines: list[str]


@dataclass
class Pause:
    seconds: float


@dataclass
class Highlight:
    line: int | None              # None = clear the current outline


@dataclass
class Scene:
    items: list = field(default_factory=list)

    def has_content(self) -> bool:
        return any(isinstance(it, Block) and it.lines for it in self.items)

    def add_pause(self, seconds: float) -> None:
        self.items.append(Pause(seconds))


# --- Script parser -----------------------------------------------------------

def parse_duration(text: str) -> float:
    s = text.strip().lower()
    if s.endswith("s"):
        s = s[:-1].strip()
    try:
        v = float(s)
    except ValueError:
        raise ValueError(f"invalid duration {text!r}") from None
    if not math.isfinite(v) or v < 0:
        raise ValueError(f"invalid duration {text!r}")
    return v


def _clean_block_lines(lines: list[str]) -> list[str]:
    while lines and not lines[-1].strip():
        lines.pop()
    while lines and not lines[0].strip():
        lines.pop(0)
    return lines


def parse_script_text(text: str, cfg: Config) -> list[Scene]:
    scenes: list[Scene] = []
    cur = Scene()
    kind: str | None = None
    buf: list[str] = []
    for lineno, raw in enumerate(text.splitlines(), 1):
        if kind is not None:
            if raw.strip() == "end":
                lines = _clean_block_lines(buf)
                if lines:
                    cur.items.append(Block(kind, lines))
                kind, buf = None, []
            else:
                buf.append(raw)
            continue
        s = raw.strip()
        if not s or s.startswith("#"):
            continue
        low = s.lower()
        if low == "clear":
            if cur.has_content():
                scenes.append(cur)
                cur = Scene()
        elif low in ("code", "text"):
            kind, buf = low, []
        elif low == "pause" or low.startswith("pause "):
            rest = s[len("pause"):].strip()
            if not rest:
                raise ValueError(f"line {lineno}: pause needs seconds")
            cur.add_pause(parse_duration(rest))
        elif low == "unhighlight":
            cur.items.append(Highlight(None))
        elif low == "highlight" or low.startswith("highlight "):
            rest = s[len("highlight"):].strip().lower()
            if rest in ("off", "none", "0"):
                cur.items.append(Highlight(None))
            elif rest.isdigit():
                cur.items.append(Highlight(int(rest)))
            else:
                raise ValueError(
                    f"line {lineno}: highlight needs a line number or 'off'")
        else:
            raise ValueError(f"line {lineno}: unknown directive {s!r}")
    if kind is not None:
        raise ValueError("unterminated block (missing 'end')")
    if cur.has_content():
        scenes.append(cur)
    if not scenes:
        raise ValueError("script contains no content")
    return scenes


def parse_script(path: str, cfg: Config) -> list[Scene]:
    with open(path, "r", encoding="utf-8") as fh:
        return parse_script_text(fh.read(), cfg)


# --- Normalization + lexing --------------------------------------------------

def normalize(text: str, tabstop: int) -> str:
    """Strip BOM/CR, expand tabs, reject wide/combining glyphs.

    ponytail: no East-Asian-width / grapheme layout; single-cell monospace only.
    Latin-1 etc. are fine (DejaVu Sans Mono has them).
    """
    if text.startswith("\ufeff"):
        text = text[1:]
    text = text.replace("\r\n", "\n").replace("\r", "\n")
    for i, ch in enumerate(text):
        if ch in "\n\t":
            continue
        if unicodedata.combining(ch):
            raise ValueError(f"combining character {ch!r} at offset {i}")
        if unicodedata.east_asian_width(ch) in ("W", "F"):
            raise ValueError(f"wide character {ch!r} at offset {i}")
    return text.expandtabs(tabstop)


def lex_lines(text: str):
    """Lex a C buffer once; return per-line chars and per-line colors."""
    lexer = CLexer(ensurenl=False, stripnl=False, stripall=False, tabsize=0)
    colors = [BASE] * len(text)
    for pos, ttype, value in lexer.get_tokens_unprocessed(text):
        col = color_for(ttype)
        for k in range(len(value)):
            idx = pos + k
            if 0 <= idx < len(colors):
                colors[idx] = col
    chars: list[list[str]] = [[]]
    cols: list[list[tuple]] = [[]]
    for ch, col in zip(text, colors):
        if ch == "\n":
            chars.append([])
            cols.append([])
        else:
            chars[-1].append(ch)
            cols[-1].append(col)
    return chars, cols


def _prepare_block(block: Block, cfg: Config):
    text = normalize("\n".join(block.lines), cfg.tabstop)
    if block.kind == "code":
        return lex_lines(text)
    chars = [list(line) for line in text.split("\n")]
    cols = [[cfg.text_color] * len(line) for line in chars]
    return chars, cols


# --- Timeline ----------------------------------------------------------------

class Glyph(NamedTuple):
    line: int
    col: int
    ch: str
    color: tuple
    reveal: float
    bold: bool
    dim: tuple


@dataclass
class SceneInfo:
    glyphs: list[Glyph]
    duration: float
    max_cols: int
    n_lines: int
    # (visual_row, line_no, first_reveal, first_col, last_col); first_col=-1 if blank
    code_lines: list
    # (time, code_line_no or None); None clears the outline
    highlight_events: list
    max_code_line: int


def build_scene(scene: Scene, cfg: Config) -> SceneInfo:
    if cfg.fade_time <= 0:
        raise ValueError("fade_time must be > 0")
    glyphs: list[Glyph] = []
    code_lines: list[tuple[int, int, float, int, int]] = []
    highlight_events: list[tuple[float, int | None]] = []
    t = 0.0
    last_reveal = 0.0
    line_cursor = 0
    max_cols = 1
    first_block = True
    code_no = 0

    for item in scene.items:
        if isinstance(item, Pause):
            t += item.seconds
            continue
        if isinstance(item, Highlight):
            highlight_events.append((t, item.line))
            continue
        chars, cols = _prepare_block(item, cfg)
        if not chars:
            continue
        if not first_block:
            line_cursor += cfg.block_gap
        is_text = item.kind == "text"
        cd = cfg.text_char_delay if is_text else cfg.code_char_delay
        ld = cfg.text_line_delay if is_text else cfg.code_line_delay
        if cd <= 0:
            raise ValueError("char delay must be > 0")
        for li, line_chars in enumerate(chars):
            max_cols = max(max_cols, len(line_chars))
            row = line_cursor + li
            line_first = t
            if not is_text:
                code_no += 1
            for ci, ch in enumerate(line_chars):
                last_reveal = t
                if ch != " ":
                    if is_text:
                        color, dim, bold = cfg.text_color, cfg.text_dim, cfg.text_bold
                    else:
                        color, dim, bold = cols[li][ci], DARK, False
                    glyphs.append(Glyph(row, ci, ch, color, t, bold, dim))
                t += cd
            t += ld
            if not is_text:
                inked = [i for i, c in enumerate(line_chars) if c != " "]
                first_col = inked[0] if inked else -1
                last_col = inked[-1] if inked else -1
                code_lines.append((row, code_no, line_first, first_col, last_col))
        line_cursor += len(chars)
        first_block = False

    clock_end = t
    duration = max(last_reveal + cfg.fade_time, clock_end) + cfg.tail_hold
    if not glyphs:
        raise ValueError("scene has no drawable content")

    total_code = len(code_lines)
    for _, line in highlight_events:
        if line is not None and not (1 <= line <= total_code):
            raise ValueError(
                f"highlight line {line} out of range "
                f"(scene has {total_code} code lines)")
    return SceneInfo(glyphs, duration, max_cols, line_cursor,
                     code_lines, highlight_events, total_code)


def check_timeline(infos: list[SceneInfo], cfg: Config) -> float:
    for info in infos:
        prev = -1.0
        for g in info.glyphs:
            if not math.isfinite(g.reveal) or not (g.reveal > prev):
                raise AssertionError("reveal times not finite/strictly increasing")
            prev = g.reveal
        if info.duration <= 0 or not math.isfinite(info.duration):
            raise AssertionError("invalid scene duration")
        prev_hl = -1.0
        for et, line in info.highlight_events:
            if not math.isfinite(et) or et < prev_hl:
                raise AssertionError("highlight times not finite/non-decreasing")
            prev_hl = et
            if line is not None and not (1 <= line <= info.max_code_line):
                raise AssertionError("highlight line out of range")
    total = sum(i.duration for i in infos)
    total += cfg.inter_scene_pause * max(0, len(infos) - 1)
    if not math.isfinite(total) or total <= 0:
        raise AssertionError("invalid total duration")
    return total


def global_timeline(infos, cfg):
    starts = []
    t = 0.0
    for idx, info in enumerate(infos):
        starts.append(t)
        t += info.duration
        if idx < len(infos) - 1:
            t += cfg.inter_scene_pause
    return t, starts, max(1, math.ceil(t * cfg.fps - 1e-9))


# --- Layout / rendering ------------------------------------------------------

@dataclass
class Layout:
    font: ImageFont.FreeTypeFont
    bold_font: ImageFont.FreeTypeFont
    cell_w: float
    cell_h: int
    ascent: int
    x0: int
    gutter_cols: int

    def pos(self, y0: int, line: int, col: int):
        return int(self.x0 + col * self.cell_w), int(y0 + line * self.cell_h + self.ascent)

    def number_pos(self, y0: int, row: int, number: int):
        digits = len(str(number))
        x = self.x0 - (digits + 1) * self.cell_w
        y = y0 + row * self.cell_h + self.ascent
        return int(x), int(y)


def _load_font(path: str, size: int) -> ImageFont.FreeTypeFont:
    if not os.path.exists(path):
        raise FileNotFoundError(f"font not found: {path}")
    return ImageFont.truetype(path, size)


def choose_layout(cfg: Config, max_cols: int, n_lines: int, gutter_cols: int) -> Layout:
    max_cols, n_lines = max(1, max_cols), max(1, n_lines)
    total_cols = gutter_cols + max_cols
    usable_w = cfg.width - 2 * cfg.margin
    usable_h = cfg.height - 2 * cfg.margin

    font = _load_font(cfg.font_path, cfg.font_size)
    if abs(font.getlength("M") - font.getlength("i")) > 0.01:
        raise ValueError(f"font is not monospace: {cfg.font_path}")
    ascent, descent = font.getmetrics()
    cell_w = font.getlength("M")
    cell_h = ascent + descent
    scale = min(1.0, usable_w / (total_cols * cell_w), usable_h / (n_lines * cell_h))
    size = int(cfg.font_size * scale)
    if size < 14:
        raise ValueError(f"font would be {size}px (below 14px); shorten lines/lines")

    font = _load_font(cfg.font_path, size)
    bold_path = cfg.bold_font_path if os.path.exists(cfg.bold_font_path) else cfg.font_path
    bold_font = _load_font(bold_path, size)
    ascent, descent = font.getmetrics()
    cell_w = font.getlength("M")
    x0 = cfg.margin + int(round(gutter_cols * cell_w))
    return Layout(font, bold_font, cell_w, ascent + descent, ascent, x0, gutter_cols)


def _y0(cfg: Config, layout: Layout, n_lines: int) -> int:
    return int((cfg.height - n_lines * layout.cell_h) / 2)


def render_frame(info: SceneInfo, layout: Layout, cfg: Config, t: float,
                 base: Image.Image, drawn: set[int]) -> Image.Image:
    y0 = _y0(cfg, layout, info.n_lines)

    # Stamp finished glyphs into the persistent base (age >= fade_time).
    bd = ImageDraw.Draw(base)
    for gi, g in enumerate(info.glyphs):
        if t - g.reveal >= cfg.fade_time and gi not in drawn:
            x, y = layout.pos(y0, g.line, g.col)
            bd.text((x, y), g.ch, font=layout.bold_font if g.bold else layout.font,
                    fill=g.color, anchor="ls")
            drawn.add(gi)

    img = base.copy()
    d = ImageDraw.Draw(img)

    # Line numbers (dim, appear when their line starts revealing).
    for row, number, first_reveal, _fc, _lc in info.code_lines:
        if t >= first_reveal:
            nx, ny = layout.number_pos(y0, row, number)
            d.text((nx, ny), str(number), font=layout.font,
                   fill=cfg.gutter_color, anchor="ls")

    for gi, g in enumerate(info.glyphs):
        age = t - g.reveal
        if age < 0 or age >= cfg.fade_time:
            continue
        x, y = layout.pos(y0, g.line, g.col)
        d.text((x, y), g.ch, font=layout.bold_font if g.bold else layout.font,
               fill=lerp(g.dim, g.color, ramp(age, cfg.fade_time)), anchor="ls")

    hl = _active_highlight(info.highlight_events, t)
    if hl is not None:
        _draw_highlight(d, layout, cfg, y0, info.code_lines[hl - 1])

    if cfg.bloom:
        img = Image.blend(img, img.filter(ImageFilter.GaussianBlur(4)), 0.5)
    if cfg.cursor:
        _draw_cursor(img, info, layout, cfg, t, y0)
    return img


def _active_highlight(events, t):
    """Last highlight event at or before t (None clears it)."""
    active = None
    for et, line in events:
        if et <= t:
            active = line
        else:
            break
    return active


def _draw_highlight(d, layout, cfg, y0, code_line):
    row, _number, _reveal, first_col, last_col = code_line
    if first_col < 0:
        return  # all-blank line: nothing to outline
    pad = cfg.highlight_pad
    x0 = layout.x0 + first_col * layout.cell_w - pad
    x1 = layout.x0 + (last_col + 1) * layout.cell_w + pad
    ytop = y0 + row * layout.cell_h - pad
    ybot = ytop + layout.cell_h + 2 * pad
    d.rectangle([int(x0), int(ytop), int(x1), int(ybot)],
                outline=cfg.highlight_color, width=cfg.highlight_width)


def _draw_cursor(img, info, layout, cfg, t, y0):
    visible = [g for g in info.glyphs if g.reveal <= t]
    if not visible or int(t * 2) % 2:
        return
    g = visible[-1]
    x, y = layout.pos(y0, g.line, g.col)
    ImageDraw.Draw(img).rectangle(
        [x, y - layout.ascent, x + int(layout.cell_w) - 2, y + 2],
        fill=(120, 255, 150))


# --- Encoding ----------------------------------------------------------------

def encode_frames(infos, layout, cfg, starts, frame_count, out_path):
    cmd = [
        "ffmpeg", "-y", "-loglevel", "error",
        "-f", "rawvideo", "-pix_fmt", "rgb24",
        "-s", f"{cfg.width}x{cfg.height}", "-framerate", str(cfg.fps),
        "-i", "-", "-r", str(cfg.fps), "-an",
        "-c:v", "libx264", "-preset", "medium", "-crf", "18",
        "-pix_fmt", "yuv420p",
        "-colorspace", "bt709", "-color_primaries", "bt709", "-color_trc", "bt709",
        "-movflags", "+faststart", out_path,
    ]
    log_path = out_path + ".ffmpeg.log"
    with open(log_path, "w") as log:
        proc = subprocess.Popen(cmd, stdin=subprocess.PIPE, stderr=log)
        bases = [Image.new("RGB", (cfg.width, cfg.height), (0, 0, 0)) for _ in infos]
        drawn = [set() for _ in infos]
        try:
            for i in range(frame_count):
                img = _frame_at(infos, bases, drawn, layout, cfg, starts, i / cfg.fps)
                proc.stdin.write(img.tobytes())
        except BrokenPipeError:
            pass
        finally:
            try:
                proc.stdin.close()
            except BrokenPipeError:
                pass
        rc = proc.wait()
    if rc != 0:
        with open(log_path) as log:
            tail = log.read()[-2000:]
        raise RuntimeError(f"ffmpeg failed ({rc}):\n{tail}")
    if os.path.getsize(log_path) == 0:
        os.remove(log_path)


def _frame_at(infos, bases, drawn, layout, cfg, starts, t_global):
    for idx, info in enumerate(infos):
        start = starts[idx]
        if start <= t_global < start + info.duration:
            return render_frame(info, layout, cfg, t_global - start, bases[idx], drawn[idx])
        if t_global < start:
            break
    return Image.new("RGB", (cfg.width, cfg.height), (0, 0, 0))


# --- Public API --------------------------------------------------------------

def make_layout(infos, cfg):
    max_cols = max(i.max_cols for i in infos)
    max_lines = max(i.n_lines for i in infos)
    gmax = max(i.max_code_line for i in infos)
    gutter_cols = (len(str(gmax)) + 1) if gmax > 0 else 0
    return choose_layout(cfg, max_cols, max_lines, gutter_cols)


def run(scenes, cfg: Config, out_path: str | None = None, timeline: bool = False):
    if not scenes:
        raise ValueError("no scenes")
    infos = [build_scene(s, cfg) for s in scenes]
    check_timeline(infos, cfg)
    total, starts, frame_count = global_timeline(infos, cfg)
    layout = make_layout(infos, cfg)

    if timeline:
        print(f"total={total:.3f}s fps={cfg.fps} frames={frame_count} "
              f"font={layout.font.size}px cell={layout.cell_w:.1f}x{layout.cell_h}")
        for idx, (info, start) in enumerate(zip(infos, starts)):
            n_text = sum(1 for s in scenes[idx].items if isinstance(s, Block) and s.kind == "text")
            n_code = sum(1 for s in scenes[idx].items if isinstance(s, Block) and s.kind == "code")
            print(f"scene {idx}: {start:7.3f}s .. {start + info.duration:7.3f}s  "
                  f"text={n_text} code={n_code} lines={info.n_lines} cols={info.max_cols}")
            for et, line in info.highlight_events:
                what = "off" if line is None else f"line {line}"
                print(f"    highlight {what:8s} at {start + et:7.3f}s")
        _smoke(infos, layout, cfg)
        return

    if out_path is None:
        return
    encode_frames(infos, layout, cfg, starts, frame_count, out_path)
    print(f"wrote {out_path} ({total:.2f}s, {frame_count} frames)")


def _smoke(infos, layout, cfg):
    info = infos[0]
    img = Image.new("RGB", (cfg.width, cfg.height), (0, 0, 0))
    final = render_frame(info, layout, cfg, info.duration - cfg.tail_hold, img, set())
    assert final.size == (cfg.width, cfg.height)
    assert _nonblack(final) > 0, "final frame is blank"

    # A future glyph must stay unpainted before its reveal time.
    last = info.glyphs[-1]
    t0 = info.glyphs[0].reveal + 1e-3
    early = render_frame(info, layout, cfg, t0,
                         Image.new("RGB", (cfg.width, cfg.height), (0, 0, 0)), set())
    y0 = _y0(cfg, layout, info.n_lines)
    x, y = layout.pos(y0, last.line, last.col)
    assert _region_black(early, x, y - layout.ascent,
                         x + int(layout.cell_w), y - layout.ascent + layout.cell_h)
    print("smoke checks passed")


def _nonblack(img) -> int:
    return sum(1 for p in img.getdata() if p != (0, 0, 0))


def _region_black(img, x0, y0, x1, y1) -> bool:
    x0, y0 = max(0, x0), max(0, y0)
    x1, y1 = min(img.width, x1), min(img.height, y1)
    for yy in range(y0, y1):
        for xx in range(x0, x1):
            if img.getpixel((xx, yy)) != (0, 0, 0):
                return False
    return True


# --- Self-test ---------------------------------------------------------------

def selftest():
    cfg = Config(width=640, height=360, margin=20, font_size=20)
    script = """\
text
An explanatory line that is warm white.
end
pause 0.2
code
int x = 1;
m_free(x);
end
highlight 2
pause 0.2
highlight off
"""
    scenes = parse_script_text(script, cfg)
    run(scenes, cfg, timeline=True)

    # Invalid highlight line must fail loudly.
    try:
        bad = parse_script_text("code\nint x;\nend\nhighlight 5\n", cfg)
        build_scene(bad[0], cfg)
    except ValueError:
        pass
    else:
        raise AssertionError("highlight out of range was not rejected")
    print("selftest ok")


if __name__ == "__main__":
    if "--selftest" in sys.argv:
        selftest()
    else:
        print("render.py is a library. Run show.py, or use --selftest.")
