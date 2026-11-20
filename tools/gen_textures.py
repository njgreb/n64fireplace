#!/usr/bin/env python3
"""
Procedural N64-native texture generator for HEARTH 64.
Generates TMEM-friendly (16x16, 32x32, 64x32) PNG textures with authentic
1990s N64 warmth and painterliness.
"""

import math
import os
import random
from PIL import Image

OUT_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "assets_raw", "textures")


def clamp(v, lo=0, hi=255):
    return max(lo, min(hi, int(round(v))))


def hash2d(x, y, seed=0):
    n = (x * 374761393 + y * 668265263 + seed * 1442695041) & 0xFFFFFFFF
    n = ((n ^ (n >> 13)) * 1274126177) & 0xFFFFFFFF
    return ((n ^ (n >> 16)) & 0xFFFF) / 65535.0


def tile_value_noise(x, y, period_x, period_y, seed=0):
    fx = x - math.floor(x)
    fy = y - math.floor(y)
    ix0 = int(math.floor(x)) % period_x
    iy0 = int(math.floor(y)) % period_y
    ix1 = (ix0 + 1) % period_x
    iy1 = (iy0 + 1) % period_y
    ux = fx * fx * (3.0 - 2.0 * fx)
    uy = fy * fy * (3.0 - 2.0 * fy)
    v00 = hash2d(ix0, iy0, seed)
    v10 = hash2d(ix1, iy0, seed)
    v01 = hash2d(ix0, iy1, seed)
    v11 = hash2d(ix1, iy1, seed)
    a = v00 + (v10 - v00) * ux
    b = v01 + (v11 - v01) * ux
    return a + (b - a) * uy


def fbm_tile(u, v, base_x, base_y, octaves=3, seed=0):
    val = 0.0
    amp = 0.5
    norm = 0.0
    for o in range(octaves):
        px = base_x * (1 << o)
        py = base_y * (1 << o)
        val += amp * tile_value_noise(u * px, v * py, px, py, seed + o * 31)
        norm += amp
        amp *= 0.5
    return val / norm


def gen_stone():
    """64x32 warm rustic cabin masonry stones with recessed mortar."""
    w, h = 64, 32
    img = Image.new("RGBA", (w, h))
    px = img.load()

    # Define 4 horizontal courses of irregular rustic stones (tileable!)
    courses = [
        (0, 8, [0, 18, 34, 52, 64]),
        (8, 16, [0, 10, 28, 44, 64]),
        (16, 24, [0, 22, 38, 56, 64]),
        (24, 32, [0, 14, 32, 48, 64]),
    ]
    stone_palettes = [
        (138, 122, 108),
        (124, 114, 104),
        (150, 128, 106),
        (114, 108, 106),
        (144, 118, 98),
        (130, 124, 116),
    ]

    for y in range(h):
        for x in range(w):
            u, v = x / w, y / h
            n = fbm_tile(u, v, 8, 4, octaves=3, seed=11)
            grain = fbm_tile(u, v, 16, 8, octaves=2, seed=29)

            # Find course and stone
            c_idx = 0
            s_idx = 0
            y0, y1, x0, x1 = 0, 8, 0, 16
            for ci, (cy0, cy1, xs) in enumerate(courses):
                if cy0 <= y < cy1:
                    c_idx = ci
                    y0, y1 = cy0, cy1
                    for si in range(len(xs) - 1):
                        if xs[si] <= x < xs[si + 1]:
                            s_idx = si
                            x0, x1 = xs[si], xs[si + 1]
                            break
                    break

            # Slight organic wobble on mortar seams
            wobble_y = (n - 0.5) * 1.4
            wobble_x = (grain - 0.5) * 1.4
            dy_top = (y - y0) + wobble_y
            dy_bot = (y1 - 1 - y) - wobble_y
            dx_left = (x - x0) + wobble_x
            dx_right = (x1 - 1 - x) - wobble_x
            edge = min(dy_top, dy_bot, dx_left, dx_right)

            if edge < 0.75:
                # Recessed mortar
                m = 0.75 + 0.35 * n
                r, g, b = 52 * m, 46 * m, 42 * m
            else:
                base = stone_palettes[(c_idx * 7 + s_idx * 3) % len(stone_palettes)]
                # Subtle bevel highlight on top/left, shadow on bottom/right
                bevel = 1.0
                if dy_top < 2.0:
                    bevel += 0.14 * (1.0 - dy_top / 2.0)
                if dy_bot < 2.0:
                    bevel -= 0.18 * (1.0 - dy_bot / 2.0)
                if dx_left < 2.0:
                    bevel += 0.08 * (1.0 - dx_left / 2.0)
                if dx_right < 2.0:
                    bevel -= 0.12 * (1.0 - dx_right / 2.0)
                shade = (0.80 + 0.36 * n + 0.12 * (grain - 0.5)) * bevel
                r = base[0] * shade
                g = base[1] * shade
                b = base[2] * shade
            px[x, y] = (clamp(r), clamp(g), clamp(b), 255)

    img.save(os.path.join(OUT_DIR, "stone.png"))


def gen_brick():
    """64x32 warm soot-kissed terracotta firebrick for inside the firebox."""
    w, h = 64, 32
    img = Image.new("RGBA", (w, h))
    px = img.load()
    for y in range(h):
        row = y // 8
        ry = y % 8
        offset = 8 if (row % 2 == 1) else 0
        for x in range(w):
            bx = (x + offset) % 16
            col = ((x + offset) // 16) % 4
            u, v = x / w, y / h
            n = fbm_tile(u, v, 8, 4, octaves=3, seed=43)
            soot = fbm_tile(u, v, 4, 2, octaves=2, seed=71)

            if ry == 0 or bx == 0:
                # Dark soot-filled mortar
                m = 0.7 + 0.4 * n
                r, g, b = 46 * m, 34 * m, 28 * m
            else:
                tint = hash2d(col, row, 19)
                r0 = 132 + 28 * tint
                g0 = 64 + 16 * tint
                b0 = 42 + 12 * tint
                bevel = 1.0
                if ry == 1:
                    bevel += 0.10
                elif ry == 7:
                    bevel -= 0.16
                # Soot patches for real fireplace character
                soot_mul = 0.62 + 0.48 * soot
                shade = (0.82 + 0.34 * n) * bevel * soot_mul
                r, g, b = r0 * shade, g0 * shade, b0 * shade
            px[x, y] = (clamp(r), clamp(g), clamp(b), 255)
    img.save(os.path.join(OUT_DIR, "brick.png"))


def gen_wood_mantle():
    """64x32 dark warm oak timber beam with horizontal wood grain."""
    w, h = 64, 32
    img = Image.new("RGBA", (w, h))
    px = img.load()
    for y in range(h):
        for x in range(w):
            u, v = x / w, y / h
            warp = fbm_tile(u, v, 4, 4, octaves=3, seed=101)
            rings = math.sin((v * 10.0 + warp * 2.4) * math.pi * 2.0) * 0.5 + 0.5
            fine = fbm_tile(u, v, 16, 4, octaves=2, seed=113)
            grain = 0.65 * rings + 0.35 * fine
            # Subtle horizontal plank bevel near edges
            edge_shade = 1.0
            if y < 2:
                edge_shade = 1.10
            elif y >= h - 2:
                edge_shade = 0.78
            r = (92 + 54 * grain) * edge_shade
            g = (52 + 34 * grain) * edge_shade
            b = (28 + 18 * grain) * edge_shade
            px[x, y] = (clamp(r), clamp(g), clamp(b), 255)
    img.save(os.path.join(OUT_DIR, "wood_mantle.png"))


def gen_floor():
    """64x32 warm cabin wood floorboards."""
    w, h = 64, 32
    img = Image.new("RGBA", (w, h))
    px = img.load()
    for y in range(h):
        plank = y // 8
        py = y % 8
        for x in range(w):
            u, v = x / w, y / h
            warp = fbm_tile(u, v, 4, 8, octaves=2, seed=211)
            rings = math.sin((v * 14.0 + warp * 1.8) * math.pi * 2.0) * 0.5 + 0.5
            tint = 0.86 + 0.28 * hash2d(plank, 3, 55)
            seam = (py == 0) or ((x + plank * 23) % 32 == 0)
            if seam:
                r, g, b = 36, 22, 14
            else:
                gval = 0.72 + 0.34 * rings
                r = 114 * gval * tint
                g = 72 * gval * tint
                b = 42 * gval * tint
            px[x, y] = (clamp(r), clamp(g), clamp(b), 255)
    img.save(os.path.join(OUT_DIR, "floor.png"))


def gen_bark():
    """32x32 rough charred firewood bark."""
    w, h = 32, 32
    img = Image.new("RGBA", (w, h))
    px = img.load()
    for y in range(h):
        for x in range(w):
            u, v = x / w, y / h
            ridge = fbm_tile(u, v, 8, 2, octaves=3, seed=307)
            cracks = fbm_tile(u, v, 16, 4, octaves=2, seed=317)
            # Deep vertical bark furrows
            val = 0.55 * ridge + 0.45 * cracks
            if val < 0.34:
                # Deep dark char crevice
                r, g, b = 26, 16, 12
            else:
                t = (val - 0.34) / 0.66
                r = 46 + 64 * t
                g = 30 + 40 * t
                b = 20 + 22 * t
            px[x, y] = (clamp(r), clamp(g), clamp(b), 255)
    img.save(os.path.join(OUT_DIR, "bark.png"))


def gen_log_end():
    """32x32 cut log cross-section with growth rings."""
    w, h = 32, 32
    img = Image.new("RGBA", (w, h))
    px = img.load()
    cx, cy = w * 0.5, h * 0.5
    for y in range(h):
        for x in range(w):
            dx = (x + 0.5 - cx) / (w * 0.5)
            dy = (y + 0.5 - cy) / (h * 0.5)
            dist = math.hypot(dx, dy)
            ang = math.atan2(dy, dx)
            n = fbm_tile(x / w, y / h, 4, 4, octaves=2, seed=401)
            if dist > 0.82:
                # Bark rim
                r, g, b = 52 + 24 * n, 34 + 14 * n, 22 + 10 * n
            else:
                ring = math.sin((dist * 10.0 + n * 0.6) * math.pi * 2.0) * 0.5 + 0.5
                # Slight radial crack
                crack = abs(math.sin(ang * 2.0 + 0.4))
                cmul = 0.65 if (crack < 0.06 and dist > 0.15) else 1.0
                shade = (0.78 + 0.28 * ring) * (1.0 - 0.18 * dist) * cmul
                r, g, b = 176 * shade, 126 * shade, 76 * shade
            px[x, y] = (clamp(r), clamp(g), clamp(b), 255)
    img.save(os.path.join(OUT_DIR, "log_end.png"))


def gen_coals():
    """32x32 glowing charcoal & ash bed with emissive orange fissures."""
    w, h = 32, 32
    img = Image.new("RGBA", (w, h))
    px = img.load()
    for y in range(h):
        for x in range(w):
            u, v = x / w, y / h
            c = fbm_tile(u, v, 6, 6, octaves=3, seed=503)
            d = fbm_tile(u, v, 12, 12, octaves=2, seed=521)
            val = 0.65 * c + 0.35 * d
            if val > 0.60:
                # Hot glowing fissure
                t = min(1.0, (val - 0.60) / 0.28)
                r = 210 + 45 * t
                g = 60 + 165 * (t ** 1.3)
                b = 10 + 60 * (t ** 2.2)
            elif val > 0.48:
                # Warm red ember edge
                t = (val - 0.48) / 0.12
                r = 55 + 155 * t
                g = 24 + 36 * t
                b = 18
            else:
                # Dark char & grey ash
                ash = val / 0.48
                r = 26 + 38 * ash
                g = 22 + 30 * ash
                b = 20 + 26 * ash
            px[x, y] = (clamp(r), clamp(g), clamp(b), 255)
    img.save(os.path.join(OUT_DIR, "coals.png"))


def gen_rug():
    """32x32 cozy woven cabin hearth rug (burgundy, cream, forest, gold)."""
    w, h = 32, 32
    img = Image.new("RGBA", (w, h))
    px = img.load()
    for y in range(h):
        for x in range(w):
            u, v = x / w, y / h
            weave = 0.90 + 0.20 * ((x + y) & 1)
            bx = min(x, w - 1 - x)
            by = min(y, h - 1 - y)
            bdist = min(bx, by)
            # Concentric rustic border + center diamond motif
            dx = abs(x - 15.5)
            dy = abs(y - 15.5)
            diam = dx + dy
            if bdist <= 1:
                col = (182, 152, 108)  # warm fringe/trim
            elif bdist <= 3:
                col = (44, 76, 54)     # deep forest green border
            elif bdist == 4:
                col = (196, 156, 86)   # gold stripe
            elif diam < 6.5 and diam > 4.5:
                col = (204, 174, 122)  # cream diamond
            elif diam <= 4.5:
                col = (48, 80, 58)     # forest diamond center
            else:
                col = (128, 38, 36)    # rich hearth burgundy
            n = 0.92 + 0.16 * hash2d(x, y, 601)
            r = col[0] * weave * n
            g = col[1] * weave * n
            b = col[2] * weave * n
            px[x, y] = (clamp(r), clamp(g), clamp(b), 255)
    img.save(os.path.join(OUT_DIR, "rug.png"))


def gen_iron():
    """32x32 hammered wrought iron."""
    w, h = 32, 32
    img = Image.new("RGBA", (w, h))
    px = img.load()
    for y in range(h):
        for x in range(w):
            n = fbm_tile(x / w, y / h, 8, 8, octaves=3, seed=701)
            v = 44 + 42 * n
            px[x, y] = (clamp(v * 1.04), clamp(v * 0.98), clamp(v * 0.95), 255)
    img.save(os.path.join(OUT_DIR, "iron.png"))


def gen_painting():
    """64x32 cozy 90s N64 alpine night painting for above the mantle."""
    w, h = 64, 32
    img = Image.new("RGBA", (w, h))
    px = img.load()
    rng = random.Random(64)
    stars = [(rng.randint(3, w - 4), rng.randint(2, 14), rng.uniform(0.6, 1.0)) for _ in range(18)]

    for y in range(h):
        for x in range(w):
            # Twilight-to-midnight sky gradient
            t = y / float(h)
            r = 18 + 38 * (t ** 1.3)
            g = 24 + 44 * (t ** 1.2)
            b = 58 + 54 * (1.0 - abs(t - 0.55))

            # Distant snowy peaks
            peak1 = 19 - 10 * max(0.0, 1.0 - abs(x - 20) / 14.0)
            peak2 = 20 - 12 * max(0.0, 1.0 - abs(x - 42) / 16.0)
            peak3 = 21 - 7 * max(0.0, 1.0 - abs(x - 57) / 11.0)
            mtn = min(peak1, peak2, peak3)
            if y >= mtn:
                # Mountain body + snowy cap
                if y < mtn + 3.2:
                    r, g, b = 210, 218, 232
                else:
                    r, g, b = 48, 56, 76

            # Foreground dark pine silhouettes
            tree_h = 25 - 4 * abs(math.sin(x * 0.45)) - 2 * math.sin(x * 1.1)
            if y >= tree_h:
                r, g, b = 18, 34, 28
            # Warm cabin window dot in valley
            if 28 <= x <= 31 and 26 <= y <= 28:
                r, g, b = 250, 186, 78

            # Crescent moon
            md = math.hypot(x - 11.5, y - 7.5)
            md2 = math.hypot(x - 13.5, y - 6.8)
            if md < 3.6 and md2 > 3.1:
                r, g, b = 248, 240, 198

            px[x, y] = (clamp(r), clamp(g), clamp(b), 255)

    for sx, sy, sb in stars:
        cr, cg, cb, _ = px[sx, sy]
        if sy < 14:
            px[sx, sy] = (clamp(cr + 190 * sb), clamp(cg + 190 * sb), clamp(cb + 210 * sb), 255)

    img.save(os.path.join(OUT_DIR, "painting.png"))


def gen_flame():
    """
    32x32 smooth soft-edged grayscale/alpha flame tongue sprite.
    Converted to IA8 so vertex colors can tint it to any theme with 256-level alpha!
    """
    w, h = 32, 32
    img = Image.new("RGBA", (w, h))
    px = img.load()
    for y in range(h):
        # v = 0 at top tip, 1 at bottom base
        v = y / (h - 1.0)
        for x in range(w):
            # Centered horizontal coordinate -1..1
            u = (x / (w - 1.0)) * 2.0 - 1.0
            # Slight graceful S-curve sway
            sway = 0.14 * math.sin(v * math.pi * 2.2) * (1.0 - v)
            ux = u - sway
            # Teardrop/flame width envelope: wide near base (v~0.75), tapers to point at top (v=0)
            width = 0.82 * (math.sin(min(1.0, v * 1.18) * math.pi) ** 0.72) * (0.25 + 0.75 * v)
            if width < 0.02:
                alpha = 0.0
                core = 0.0
            else:
                d = abs(ux) / width
                # Soft organic wisps via noise
                n = fbm_tile(x / w, y / h, 4, 4, octaves=2, seed=809)
                d_mod = d + (n - 0.5) * 0.28 * (1.0 - 0.5 * v)
                edge = max(0.0, 1.0 - d_mod * d_mod)
                # Fade cleanly to zero at all 4 texture edges so bilinear never clamps ugly borders
                border = min(1.0, v * 5.0) * min(1.0, (1.0 - v) * 4.5)
                alpha = (edge ** 1.35) * border
                # Bright hot core concentrated lower-center
                core_d = math.hypot(ux * 1.4, (v - 0.68) * 1.35)
                core = max(0.0, 1.0 - core_d) ** 1.6

            # White/grayscale intensity + smooth alpha
            lum = clamp(150 + 105 * min(1.0, core + 0.45 * alpha))
            a = clamp(255 * min(1.0, alpha))
            px[x, y] = (lum, lum, lum, a)
    img.save(os.path.join(OUT_DIR, "flame.png"))


def gen_glow():
    """32x32 ultra-smooth Gaussian radial halo for hearth & candle glow (IA8)."""
    w, h = 32, 32
    img = Image.new("RGBA", (w, h))
    px = img.load()
    for y in range(h):
        for x in range(w):
            dx = (x + 0.5 - w * 0.5) / (w * 0.5)
            dy = (y + 0.5 - h * 0.5) / (h * 0.5)
            r = math.hypot(dx, dy)
            if r >= 1.0:
                val = 0.0
            else:
                val = math.exp(-3.4 * r * r) * (1.0 - r * r)
            a = clamp(255 * val)
            px[x, y] = (255, 255, 255, a)
    img.save(os.path.join(OUT_DIR, "glow.png"))


def gen_spark():
    """16x16 bright ember/spark dot with soft halo (IA8)."""
    w, h = 16, 16
    img = Image.new("RGBA", (w, h))
    px = img.load()
    for y in range(h):
        for x in range(w):
            dx = (x + 0.5 - w * 0.5) / (w * 0.5)
            dy = (y + 0.5 - h * 0.5) / (h * 0.5)
            r = math.hypot(dx, dy)
            if r >= 1.0:
                val = 0.0
            else:
                val = (1.0 - r) ** 1.8
            a = clamp(255 * val)
            px[x, y] = (255, 255, 255, a)
    img.save(os.path.join(OUT_DIR, "spark.png"))


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    gen_stone()
    gen_brick()
    gen_wood_mantle()
    gen_floor()
    gen_bark()
    gen_log_end()
    gen_coals()
    gen_rug()
    gen_iron()
    gen_painting()
    gen_flame()
    gen_glow()
    gen_spark()
    print(f"Generated 13 N64 textures in {OUT_DIR}")


if __name__ == "__main__":
    main()
