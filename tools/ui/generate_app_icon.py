#!/usr/bin/env python3
"""Generate the Windows ICO from the approved NextGen NG raster master.

Usage:
  python tools/ui/generate_app_icon.py --source <ng-master.png-or-jpg> \
      --output src/app/resources/nextgen.ico

Requires Pillow:
  python -m pip install pillow
"""

from __future__ import annotations

import argparse
from pathlib import Path
from PIL import Image


SIZES = [(16,16), (24,24), (32,32), (48,48), (64,64), (96,96), (128,128), (256,256)]


def white_to_alpha(image: Image.Image) -> Image.Image:
    image = image.convert("RGBA")
    px = image.load()
    for y in range(image.height):
        for x in range(image.width):
            r, g, b, a = px[x, y]
            lo = min(r, g, b)
            spread = max(r, g, b) - lo
            if lo >= 238 and spread < 18:
                alpha = max(0, min(255, int((252 - lo) / 14.0 * 255.0)))
                px[x, y] = (r, g, b, alpha)
    return image


def prepare_master(source: Path) -> Image.Image:
    img = white_to_alpha(Image.open(source))
    bbox = img.getchannel("A").getbbox()
    if bbox:
        img = img.crop(bbox)

    side = max(img.size)
    pad = max(8, int(side * 0.08))
    canvas = Image.new("RGBA", (side + pad * 2, side + pad * 2), (0, 0, 0, 0))
    canvas.alpha_composite(img, ((canvas.width - img.width)//2, (canvas.height - img.height)//2))
    return canvas.resize((1024, 1024), Image.Resampling.LANCZOS)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--master-output", type=Path)
    args = parser.parse_args()

    master = prepare_master(args.source)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    master.save(args.output, format="ICO", sizes=SIZES)

    if args.master_output:
        args.master_output.parent.mkdir(parents=True, exist_ok=True)
        master.save(args.master_output, format="PNG", optimize=True)

    print(f"Wrote {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
