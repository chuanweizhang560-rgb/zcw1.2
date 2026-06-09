"""Procedural desert terrain - primarily smooth sand dunes.

Terrain type distribution:
  ~80% dune (smooth rolling sand dunes, varying sizes and slopes)
  ~10% basin (flat low areas for infrastructure)
  ~5%  wadi (shallow dry river channels)
  ~5%  mesa/cliff (occasional sharp rock features)

Uses coordinate warping for organic dune shapes.
"""

import math
import struct
import zlib


# ── Hash-based value noise (seeded) ──────────────────────────────────

def _hash(x, y, seed):
    h = seed + x * 374761393 + y * 668265263
    h = (h ^ (h >> 13)) * 1274126177
    return (h ^ (h >> 16)) & 0x7fffffff


def _smooth_noise(x, y, seed):
    ix, iy = int(math.floor(x)), int(math.floor(y))
    fx, fy = x - ix, y - iy
    sx = fx * fx * (3 - 2 * fx)
    sy = fy * fy * (3 - 2 * fy)
    n00 = _hash(ix, iy, seed) / 0x7fffffff
    n10 = _hash(ix + 1, iy, seed) / 0x7fffffff
    n01 = _hash(ix, iy + 1, seed) / 0x7fffffff
    n11 = _hash(ix + 1, iy + 1, seed) / 0x7fffffff
    nx0 = n00 + (n10 - n00) * sx
    nx1 = n01 + (n11 - n01) * sx
    return nx0 + (nx1 - nx0) * sy


def _fbm(x, y, seed, octaves=6, persistence=0.5, lacunarity=2.0):
    v, amp, freq, total = 0.0, 1.0, 1.0, 0.0
    for o in range(octaves):
        v += _smooth_noise(x * freq, y * freq, seed + o * 97) * amp
        total += amp
        amp *= persistence
        freq *= lacunarity
    return v / total


# ── Region classification ────────────────────────────────────────────

def _region_map(x_world, y_world, seed):
    r = _fbm(x_world / 120, y_world / 120, seed + 1000,
             octaves=3, persistence=0.6)
    cliff = _fbm(x_world / 80, y_world / 80,
                 seed + 1100, octaves=2, persistence=0.5)
    return r, cliff


def classify_region(noise, cliff):
    if noise < 0.20:
        return "basin"
    elif noise < 0.28:
        return "plains"
    elif noise < 0.78:
        return "dune"
    elif noise < 0.85:
        if cliff > 0.1:
            return "wadi"
        return "dune"
    else:
        if cliff > 0.0:
            return "mesa"
        return "dune"


# ── Height functions ─────────────────────────────────────────────────

def _warp(x, y, seed, strength=30, scale=80):
    dx = _fbm(x / scale, y / scale, seed + 300, octaves=2) * strength
    dy = _fbm(x / scale, y / scale, seed + 400, octaves=2) * strength
    return x + dx, y + dy


def _basin_height(x, y, seed, scale=80):
    n = _fbm(x / (scale * 0.4), y / (scale * 0.4), seed + 2000,
             octaves=3, persistence=0.3) * 0.03
    return 0.05 + n


def _plains_height(x, y, seed, scale=80):
    n = _fbm(x / (scale * 0.5), y / (scale * 0.5), seed + 2500,
             octaves=3, persistence=0.4) * 0.06
    return 0.10 + n


def _dune_height(x, y, seed, scale=60):
    wx, wy = _warp(x, y, seed + 3000, strength=50, scale=80)
    # Very large dune shapes (scale ~150m)
    huge = _fbm(wx / 200, wy / 200, seed + 3050,
                octaves=3, persistence=0.35)
    # Large dune shapes (scale ~100m)
    large = _fbm(wx / (scale * 2.0), wy / (scale * 2.0),
                 seed + 3100, octaves=4, persistence=0.40)
    # Medium dunes (scale ~50m)
    medium = _fbm(wx / scale, wy / scale,
                  seed + 3200, octaves=4, persistence=0.50)
    # Small surface ripples (scale ~15m)
    small = _fbm(wx / (scale * 0.25), wy / (scale * 0.25),
                 seed + 3300, octaves=2, persistence=0.30) * 0.03
    # Blend: multi-scale dunes create varied sizes
    h = huge * 0.20 + large * 0.35 + medium * 0.35 + small
    return max(0.01, (h + 1) * 0.5 * 0.85 + 0.08)


def _wadi_height(x, y, seed, scale=80):
    wx, wy = _warp(x, y, seed + 4000, strength=50, scale=100)
    base = _fbm(wx / scale, wy / scale, seed + 4100,
                octaves=3, persistence=0.5)
    # Carve channel
    channel = _fbm(x / (scale * 0.6) + y / (scale * 0.4),
                   y / (scale * 0.6) - x / (scale * 0.4),
                   seed + 4200, octaves=3, persistence=0.5)
    channel = (channel + 1) * 0.5
    channel = math.pow(channel, 2.0) * (-0.2)
    h = _dune_height(x, y, seed + 4300) + channel
    return max(0.01, h)


def _mesa_height(x, y, seed, cliff_factor=0.0, scale=80):
    n = _fbm(x / (scale * 0.6), y / (scale * 0.6), seed + 5000,
             octaves=3, persistence=0.5)
    n = (n + 1) * 0.5
    sharp = math.pow(n, 4.0)
    sharp += cliff_factor * 0.2 * max(0, sharp * (1 - sharp)) * 8
    h_offset = _fbm(x / (scale * 0.3), y / (scale * 0.3),
                    seed + 5500, octaves=2) * 0.08
    return 0.55 + sharp * 0.25 + h_offset


REGION_FUNCS = {
    "basin": _basin_height,
    "plains": _plains_height,
    "dune": _dune_height,
    "wadi": _wadi_height,
    "mesa": _mesa_height,
}


# ── Main heightmap generator ─────────────────────────────────────────

def generate_heightmap(size, height_max, seed, world_size_m=500,
                       flat_center_radius=0.0):
    pixels_raw = []
    region_counts = {}

    for y in range(size):
        row = []
        for x in range(size):
            x_world = x / size * world_size_m - world_size_m / 2
            y_world = world_size_m / 2 - y / size * world_size_m
            r, cliff = _region_map(x_world, y_world, seed)
            region = classify_region(r, cliff)
            region_counts[region] = region_counts.get(region, 0) + 1

            fn = REGION_FUNCS[region]
            if region == "mesa":
                h = fn(x_world, y_world, seed, cliff)
            else:
                h = fn(x_world, y_world, seed)
            row.append(h)
        pixels_raw.append(row)

    all_h = [v for row in pixels_raw for v in row]
    h_min, h_max_raw = min(all_h), max(all_h)
    h_range = h_max_raw - h_min

    pixels = []
    for y in range(size):
        row = []
        for x in range(size):
            h = (pixels_raw[y][x] - h_min) / h_range
            h = max(0.0, min(1.0, h))
            if flat_center_radius > 0:
                nx, ny = x / size, y / size
                dist = math.hypot(nx - 0.5, ny - 0.5)
                if dist < flat_center_radius:
                    center_h = h
                    blend = dist / flat_center_radius
                    h = center_h * (1 - blend) + h * blend
            row.append(int(h * 255))
        pixels.append(row)

    return pixels, region_counts


def write_png_8bit(filename, pixels, size):
    sig = b'\x89PNG\r\n\x1a\n'
    ihdr_data = struct.pack('>IIBBBBB', size, size, 8, 0, 0, 0, 0)
    ihdr_crc = zlib.crc32(b'IHDR' + ihdr_data) & 0xffffffff
    ihdr = struct.pack('>I', 13) + b'IHDR' + ihdr_data + struct.pack('>I', ihdr_crc)
    raw = b''
    for row in pixels:
        raw += b'\x00'
        raw += bytes(row)
    compressed = zlib.compress(raw)
    idat_crc = zlib.crc32(b'IDAT' + compressed) & 0xffffffff
    idat = struct.pack('>I', len(compressed)) + b'IDAT' + compressed + struct.pack('>I', idat_crc)
    iend_crc = zlib.crc32(b'IEND') & 0xffffffff
    iend = struct.pack('>I', 0) + b'IEND' + struct.pack('>I', iend_crc)
    with open(filename, 'wb') as f:
        f.write(sig + ihdr + idat + iend)


def generate_terrain_model(output_dir, terrain_cfg, seed):
    size = terrain_cfg.get("resolution", 257)
    h_max = terrain_cfg["height_max"]
    size_x, size_y = terrain_cfg["size_m"]

    pixels, region_counts = generate_heightmap(
        size=size,
        height_max=h_max,
        seed=seed,
        world_size_m=max(size_x, size_y),
        flat_center_radius=terrain_cfg.get("flat_center_radius", 0.0),
    )

    png_path = f"{output_dir}/heightmap.png"
    write_png_8bit(png_path, pixels, size)

    vals = [v for row in pixels for v in row]
    mn = min(vals) / 255 * h_max
    mx = max(vals) / 255 * h_max
    print(f"  Terrain: {size}x{size} → {png_path}")
    print(f"    Elevation: {mn:.1f}–{mx:.1f}m  relief={mx - mn:.0f}m")
    print(f"    Regions: {dict(sorted(region_counts.items(), key=lambda x: -x[1]))}")

    return png_path, (size_x, size_y, h_max), pixels
