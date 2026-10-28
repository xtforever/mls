#!/usr/bin/env python3
"""Render a code-animation video from a .script file.

    python3 show.py you-dont-know-c.script --timeline   # check + smoke test
    python3 show.py you-dont-know-c.script               # -> you-dont-know-c.mp4
    python3 show.py my.script --out my.mp4
"""
import argparse
import os

import render

CFG = render.Config(
    width=1920,
    height=1080,
    fps=30,
    margin=90,
    font_size=42,
    fade_time=0.28,
    tail_hold=1.0,
    inter_scene_pause=0.6,
)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("script", nargs="?", default="you-dont-know-c.script")
    ap.add_argument("--out", default=None)
    ap.add_argument("--timeline", action="store_true")
    args = ap.parse_args()

    scenes = render.parse_script(args.script, CFG)
    out = args.out or (os.path.splitext(args.script)[0] + ".mp4")
    render.run(scenes, CFG, out_path=out, timeline=args.timeline)


if __name__ == "__main__":
    main()
