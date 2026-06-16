"""
Generates TX_Tileset_Stone_Ground.png and TX_Tileset_Wall.png
8x8 grid of 32x32 tiles = 256x256 px each.
Uses only stdlib (struct, zlib) — no Pillow needed.
"""
import struct, zlib, random

def write_png(path, width, height, pixels):
    """pixels: list of (r,g,b,a) tuples, row-major."""
    def chunk(name, data):
        c = struct.pack('>I', len(data)) + name + data
        return c + struct.pack('>I', zlib.crc32(name + data) & 0xFFFFFFFF)

    raw = b''
    for y in range(height):
        row = b'\x00'  # filter type None
        for x in range(width):
            r, g, b, a = pixels[y * width + x]
            row += struct.pack('BBBB', r, g, b, a)
        raw += row

    ihdr = struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0)  # RGB, but we use RGBA
    ihdr = struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0)  # 6 = RGBA
    compressed = zlib.compress(raw, 9)

    png  = b'\x89PNG\r\n\x1a\n'
    png += chunk(b'IHDR', ihdr)
    png += chunk(b'IDAT', compressed)
    png += chunk(b'IEND', b'')

    with open(path, 'wb') as f:
        f.write(png)
    print(f"Written: {path}  ({width}x{height})")


def clamp(v): return max(0, min(255, int(v)))
def lerp(a, b, t): return a + (b - a) * t


# ── Stone Ground Tileset ──────────────────────────────────────────────────────
# 8x8 grid, 32x32 per cell = 256x256
GRID = 8
CELL = 32
W = GRID * CELL

rng = random.Random(42)

ground_pixels = [(0,0,0,255)] * (W * W)

def px_idx(x, y): return y * W + x

# Base stone color variants for each cell
STONE_VARIANTS = [
    (85, 82, 78),   # cell 0: medium gray
    (78, 76, 72),   # cell 1: darker gray
    (92, 88, 84),   # cell 2: lighter gray
    (70, 68, 66),   # cell 3: dark
    (95, 91, 86),   # cell 4: warm light
    (80, 77, 73),   # cell 5: neutral
    (88, 84, 79),   # cell 6: warm mid
    (74, 72, 70),   # cell 7: cool dark
]

def draw_stone_floor(gx, gy, base_r, base_g, base_b, rng):
    px0, py0 = gx * CELL, gy * CELL
    for ly in range(CELL):
        for lx in range(CELL):
            # Base noise
            noise = rng.randint(-8, 8)
            r = clamp(base_r + noise)
            g = clamp(base_g + noise)
            b = clamp(base_b + noise)

            # Mortar lines: thin 1px lines at cell borders (2px inset)
            mx = lx % 8
            my = ly % 8
            if mx == 0 or my == 0:
                r = clamp(r - 20)
                g = clamp(g - 20)
                b = clamp(b - 20)

            # Occasional crack: small diagonal lines
            if rng.random() < 0.005:
                r = clamp(r - 30)
                g = clamp(g - 30)
                b = clamp(b - 30)

            ground_pixels[px_idx(px0 + lx, py0 + ly)] = (r, g, b, 255)

for row_i in range(GRID):
    for col_i in range(GRID):
        idx = row_i * GRID + col_i
        br, bg, bb = STONE_VARIANTS[idx % len(STONE_VARIANTS)]
        # Slight variation per cell
        br += rng.randint(-4, 4); bg += rng.randint(-4, 4); bb += rng.randint(-4, 4)
        draw_stone_floor(col_i, row_i, clamp(br), clamp(bg), clamp(bb), rng)

write_png("Assets/tilesets/TX_Tileset_Stone_Ground.png", W, W, ground_pixels)


# ── Wall Tileset ──────────────────────────────────────────────────────────────

wall_pixels = [(0,0,0,255)] * (W * W)

WALL_VARIANTS = [
    (55, 52, 48),   # cell 0: dark stone
    (48, 46, 44),   # cell 1: darker stone
    (62, 58, 54),   # cell 2: brick warm
    (58, 54, 50),   # cell 3: brick darker
    (52, 50, 48),   # cell 4: panel/metal
    (46, 44, 44),   # cell 5: panel dark
    (60, 56, 50),   # cell 6: mixed
    (44, 42, 40),   # cell 7: very dark
]

def draw_wall_stone(gx, gy, base_r, base_g, base_b, rng):
    """Large stone blocks pattern."""
    px0, py0 = gx * CELL, gy * CELL
    BW, BH = 16, 10  # block size
    for ly in range(CELL):
        for lx in range(CELL):
            # Which block are we in (brick offset pattern)
            row_b = ly // BH
            off = (row_b % 2) * (BW // 2)
            col_b = (lx + off) // BW

            noise = rng.randint(-5, 5)
            r = clamp(base_r + noise + col_b * 2)
            g = clamp(base_g + noise)
            b = clamp(base_b + noise - col_b)

            # Mortar joints
            bx_local = (lx + off) % BW
            by_local = ly % BH
            if bx_local == 0 or by_local == 0 or bx_local == BW-1 or by_local == BH-1:
                r = clamp(r - 22)
                g = clamp(g - 22)
                b = clamp(b - 22)
            # Edge highlight on top of each block
            elif by_local == 1:
                r = clamp(r + 10)
                g = clamp(g + 10)
                b = clamp(b + 10)

            wall_pixels[px_idx(px0 + lx, py0 + ly)] = (r, g, b, 255)

def draw_wall_brick(gx, gy, base_r, base_g, base_b, rng):
    """Smaller brick pattern."""
    px0, py0 = gx * CELL, gy * CELL
    BW, BH = 8, 5
    for ly in range(CELL):
        for lx in range(CELL):
            row_b = ly // BH
            off = (row_b % 2) * (BW // 2)
            bx_local = (lx + off) % BW
            by_local = ly % BH

            noise = rng.randint(-4, 4)
            r = clamp(base_r + noise)
            g = clamp(base_g + noise)
            b = clamp(base_b + noise)

            if bx_local == 0 or by_local == 0:
                r = clamp(r - 18)
                g = clamp(g - 18)
                b = clamp(b - 18)
            elif by_local == 1:
                r = clamp(r + 8)
                g = clamp(g + 8)
                b = clamp(b + 8)

            wall_pixels[px_idx(px0 + lx, py0 + ly)] = (r, g, b, 255)

def draw_wall_panel(gx, gy, base_r, base_g, base_b, rng):
    """Metal panel / sci-fi wall."""
    px0, py0 = gx * CELL, gy * CELL
    for ly in range(CELL):
        for lx in range(CELL):
            noise = rng.randint(-3, 3)
            r = clamp(base_r + noise)
            g = clamp(base_g + noise)
            b = clamp(base_b + noise)

            # Horizontal panel seams every 8px
            if ly % 8 == 0 or ly % 8 == 7:
                r = clamp(r - 15)
                g = clamp(g - 15)
                b = clamp(b - 15)
            elif ly % 8 == 1:
                r = clamp(r + 12)
                g = clamp(g + 12)
                b = clamp(b + 12)

            # Rivet dots
            if (lx % 16 == 2 and ly % 8 == 3) or (lx % 16 == 13 and ly % 8 == 3):
                r = clamp(r + 30)
                g = clamp(g + 30)
                b = clamp(b + 30)

            wall_pixels[px_idx(px0 + lx, py0 + ly)] = (r, g, b, 255)

WALL_DRAWERS = [
    draw_wall_stone,
    draw_wall_stone,
    draw_wall_brick,
    draw_wall_brick,
    draw_wall_panel,
    draw_wall_panel,
    draw_wall_stone,
    draw_wall_brick,
]

for row_i in range(GRID):
    for col_i in range(GRID):
        idx = row_i * GRID + col_i
        br, bg, bb = WALL_VARIANTS[idx % len(WALL_VARIANTS)]
        br += rng.randint(-3, 3); bg += rng.randint(-3, 3); bb += rng.randint(-3, 3)
        WALL_DRAWERS[idx % len(WALL_DRAWERS)](col_i, row_i, clamp(br), clamp(bg), clamp(bb), rng)

write_png("Assets/tilesets/TX_Tileset_Wall.png", W, W, wall_pixels)

print("Done.")
