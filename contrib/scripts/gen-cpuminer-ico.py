#!/usr/bin/env python3
"""Generate res/cpuminer.ico from the Vericonomy verium-logo.svg (embedded PNG)."""

from __future__ import annotations

import base64
import io
import re
import sys
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
SVG = ROOT.parent / "vericonomy-web" / "public" / "img" / "vericonomy" / "verium-logo.svg"
OUT = ROOT / "res" / "cpuminer.ico"
SIZES = [(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)]


def main() -> int:
    if not SVG.is_file():
        print(f"logo not found: {SVG}", file=sys.stderr)
        return 1
    svg_text = SVG.read_text(encoding="utf-8")
    match = re.search(r"data:image/png;base64,([^\"]+)", svg_text)
    if not match:
        print("no embedded PNG in logo SVG", file=sys.stderr)
        return 1
    img = Image.open(io.BytesIO(base64.b64decode(match.group(1)))).convert("RGBA")
    base = img.resize((256, 256), Image.Resampling.LANCZOS)
    OUT.parent.mkdir(parents=True, exist_ok=True)
    base.save(OUT, format="ICO", sizes=SIZES)
    print(f"Wrote {OUT} ({OUT.stat().st_size} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
