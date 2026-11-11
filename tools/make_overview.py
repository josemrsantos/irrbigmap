#!/usr/bin/env python3
"""Build the low-resolution "overview" images used for the distant terrain.

irrbigmap streams full-resolution 129x129 tiles around the camera. To see the
rest of the map from far away it also draws one coarse mesh of the whole map.
This script produces the two images that mesh is built from, by downsampling
the same tile zips the main program already uses:

  media/overview_height.png   8-bit grey, (cols*S+1) x (rows*S+1) heights
  media/overview_color.jpg    RGB, (cols*P) x (rows*P) colours

S = height samples per tile (default 8, i.e. one sample per ~1.4 km),
P = colour pixels per tile (default 32).

Requires: Python 3, numpy, Pillow.

Usage (from the repository root):
  python3 tools/make_overview.py [--media media] [--samples 8] [--pixels 32]
"""
import argparse
import io
import re
import sys
import zipfile

import numpy as np
from PIL import Image

TILE = 128  # tile step in pixels (tiles are 129x129 and overlap by 1 pixel)
NAME = re.compile(r"^(?:heightmap|color)_r(\d+)_c(\d+)\.(?:bmp|png)$")


def tile_index(zf):
    """Return {(row, col): member_name} with 0-based row/col."""
    tiles = {}
    for name in zf.namelist():
        m = NAME.match(name)
        if m:
            tiles[(int(m.group(1)) - 1, int(m.group(2)) - 1)] = name
    return tiles


def build_height(media, samples):
    zf = zipfile.ZipFile(f"{media}/heightmap.zip")
    tiles = tile_index(zf)
    rows = max(r for r, _ in tiles) + 1
    cols = max(c for _, c in tiles) + 1
    print(f"heightmap: {len(tiles)} tiles, {rows} rows x {cols} columns")

    full = np.zeros((rows * TILE + 1, cols * TILE + 1), dtype=np.uint8)
    for (r, c), name in tiles.items():
        img = Image.open(io.BytesIO(zf.read(name))).convert("L")
        a = np.asarray(img)
        full[r * TILE : r * TILE + a.shape[0], c * TILE : c * TILE + a.shape[1]] = a

    b = TILE // samples  # source pixels per overview cell
    h = full[: rows * TILE, : cols * TILE].astype(np.float32)
    cells = h.reshape(rows * samples, b, cols * samples, b)
    cmean = cells.mean(axis=(1, 3))
    cmin = cells.min(axis=(1, 3))

    def vertex(grid, op):
        # vertex grid is one larger than the cell grid: combine the (up to) 4
        # cells touching each vertex, replicating at the border
        p = np.pad(grid, 1, mode="edge")
        stack = np.stack([p[:-1, :-1], p[:-1, 1:], p[1:, :-1], p[1:, 1:]])
        return op(stack, axis=0)

    # Blend mean and min: the overview must not poke above the detailed tiles
    # that are drawn over it, so bias towards the lower envelope.
    vmean = vertex(cmean, np.mean)
    vmin = vertex(cmin, np.min)
    out = np.clip(np.rint(0.5 * vmean + 0.5 * vmin), 0, 255).astype(np.uint8)
    return out, rows, cols


def build_color(media, pixels, rows, cols):
    zf = zipfile.ZipFile(f"{media}/color.zip")
    tiles = tile_index(zf)
    print(f"color: {len(tiles)} tiles")
    mosaic = Image.new("RGB", (cols * pixels, rows * pixels))
    for i, ((r, c), name) in enumerate(tiles.items()):
        if r >= rows or c >= cols:
            continue
        img = Image.open(io.BytesIO(zf.read(name))).convert("RGB")
        img = img.crop((0, 0, TILE, TILE)).resize((pixels, pixels), Image.BOX)
        mosaic.paste(img, (c * pixels, r * pixels))
        if i % 2000 == 0:
            print(f"  {i}/{len(tiles)}", file=sys.stderr)
    return mosaic


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--media", default="media")
    ap.add_argument("--samples", type=int, default=8, help="height samples per tile (divides 128)")
    ap.add_argument("--pixels", type=int, default=32, help="colour pixels per tile")
    args = ap.parse_args()
    if TILE % args.samples:
        ap.error("--samples must divide 128")

    height, rows, cols = build_height(args.media, args.samples)
    Image.fromarray(height, "L").save(f"{args.media}/overview_height.png", optimize=True)
    print(f"wrote overview_height.png {height.shape[1]}x{height.shape[0]}")

    color = build_color(args.media, args.pixels, rows, cols)
    color.save(f"{args.media}/overview_color.jpg", quality=92)
    print(f"wrote overview_color.jpg {color.size[0]}x{color.size[1]}")


if __name__ == "__main__":
    main()
