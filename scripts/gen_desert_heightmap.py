#!/usr/bin/env python3
"""Generate a procedural desert dune heightmap for Gazebo 11 Classic.

Output: 16-bit grayscale PNG, (2^n+1) x (2^n+1) pixels.
"""
import math
import struct
import zlib
import sys
import os

SIZE = 257  # 2^8 + 1
MAX_HEIGHT = 8.0  # meters of vertical relief


def smooth_noise(x, y):
    """Simple pseudo-noise using sine combinations (no external deps)."""
    return (
        math.sin(x * 1.7 + y * 2.3) * 0.4
        + math.sin(x * 3.1 - y * 4.7 + 1.2) * 0.25
        + math.sin(x * 7.5 + y * 6.3 + 3.8) * 0.15
        + math.sin(x * 13.9 - y * 11.7 + 5.4) * 0.1
        + math.sin(x * 25.3 + y * 27.1 + 7.2) * 0.05
        + math.sin(x * 50.1 - y * 48.9 + 9.8) * 0.025
    )


def generate_heightmap(size, max_height_m):
    pixels = []
    for y in range(size):
        row = []
        for x in range(size):
            nx = x / size
            ny = y / size
            # Rolling desert dunes
            h = smooth_noise(nx * 4, ny * 4)
            # Stretch dunes in one direction for wind effect
            h += smooth_noise(nx * 2, ny * 6) * 0.3
            # Larger undulations
            h += math.sin(nx * 1.5 + ny * 0.8) * 0.2
            # Flatter valleys
            h = (h + 1.0) / 2.0  # normalize to 0-1
            h = max(0.0, min(1.0, h))
            # 16-bit value
            val = int(h * 65535)
            row.append(val)
        pixels.append(row)
    return pixels


def write_png(filename, pixels, size):
    """Write 16-bit grayscale PNG manually."""
    # PNG signature
    sig = b'\x89PNG\r\n\x1a\n'
    # IHDR chunk (color type 0 = grayscale, 16-bit)
    ihdr_data = struct.pack('>IIBBBBB', size, size, 16, 0, 0, 0, 0)
    ihdr_crc = zlib.crc32(b'IHDR' + ihdr_data) & 0xffffffff
    ihdr = struct.pack('>I', 13) + b'IHDR' + ihdr_data + struct.pack('>I', ihdr_crc)

    # IDAT chunk (raw pixel data)
    raw = b''
    for row in pixels:
        raw += b'\x00'  # filter byte (none)
        for val in row:
            raw += struct.pack('>H', val)

    compressed = zlib.compress(raw)
    idat_crc = zlib.crc32(b'IDAT' + compressed) & 0xffffffff
    idat = struct.pack('>I', len(compressed)) + b'IDAT' + compressed + struct.pack('>I', idat_crc)

    # IEND chunk
    iend_crc = zlib.crc32(b'IEND') & 0xffffffff
    iend = struct.pack('>I', 0) + b'IEND' + struct.pack('>I', iend_crc)

    with open(filename, 'wb') as f:
        f.write(sig + ihdr + idat + iend)


def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else "/tmp"
    os.makedirs(out_dir, exist_ok=True)
    png_path = os.path.join(out_dir, "desert_heightmap.png")

    pixels = generate_heightmap(SIZE, MAX_HEIGHT)
    write_png(png_path, pixels, SIZE)

    center_h = pixels[SIZE // 2][SIZE // 2] / 65535 * MAX_HEIGHT
    min_h = min(min(r) for r in pixels) / 65535 * MAX_HEIGHT
    max_h = max(max(r) for r in pixels) / 65535 * MAX_HEIGHT
    print(f"Desert heightmap {SIZE}x{SIZE} -> {png_path}")
    print(f"  Elevation: min={min_h:.2f}m center={center_h:.2f}m max={max_h:.2f}m")
    print(f"  Relief: {max_h - min_h:.2f}m (configured {MAX_HEIGHT}m)")


if __name__ == "__main__":
    main()
