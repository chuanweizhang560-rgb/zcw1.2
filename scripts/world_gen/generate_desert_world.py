#!/usr/bin/env python3
"""
Desert Wind Farm World Generator for Gazebo 11 Classic.

Generates a complete, configurable desert scene with:
  - Region-based terrain (basins, plains, mesas, ridges, wadis)
  - Wind turbines (random placement)
  - Transmission towers + cables
  - Base station, wind plugin
  - Objects placed at correct terrain elevation

Usage:
  python3 generate_desert_world.py --seed 42 --output /tmp/my_world
  python3 generate_desert_world.py --seed 123 --turbine-count 9
"""

import argparse
import math
import os
import random

import yaml
from jinja2 import Environment, FileSystemLoader

from terrain import generate_terrain_model

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_CONFIG = os.path.join(HERE, "default_config.yaml")
TEMPLATE_DIR = os.path.join(HERE, "templates")
TEMPLATE_NAME = "desert_world.sdf.j2"


def load_config(config_path, overrides=None):
    with open(config_path) as f:
        cfg = yaml.safe_load(f)
    if overrides:
        _deep_merge(cfg, overrides)
    return cfg


def _deep_merge(base, overrides, prefix=""):
    for key, val in overrides.items():
        if val is not None:
            if isinstance(val, dict):
                base.setdefault(key, {})
                _deep_merge(base[key], val, f"{prefix}.{key}")
            else:
                base[key] = val


def _terrain_z(x, y, pixels, h_max, terrain_size_m=500, terrain_res=257):
    """Sample terrain elevation (world z) at position (x, y) in meters."""
    half = terrain_size_m / 2
    px = (x + half) / terrain_size_m * terrain_res
    py = (half - y) / terrain_size_m * terrain_res
    ix = max(0, min(terrain_res - 1, int(round(px))))
    iy = max(0, min(terrain_res - 1, int(round(py))))
    return pixels[iy][ix] / 255 * h_max


def _random_placement(count, min_spacing, area_width, area_height,
                      center, rng, max_attempts=2000):
    """Poisson-disc-like random placement within area. Returns [(x, y), ...]."""
    cx, cy = center
    half_w, half_h = area_width / 2, area_height / 2
    positions = []
    attempts = 0
    while len(positions) < count and attempts < max_attempts:
        x = cx + rng.uniform(-half_w, half_w)
        y = cy + rng.uniform(-half_h, half_h)
        ok = True
        for p in positions:
            if math.hypot(x - p[0], y - p[1]) < min_spacing:
                ok = False
                break
        if ok:
            positions.append((round(x, 1), round(y, 1)))
        attempts += 1
    print(f"    Placed {len(positions)}/{count} (attempts={attempts})")
    return positions


def _place_turbines(cfg, pixels, h_max, terrain_size_m):
    t = cfg["turbines"]
    seed = cfg["seed"]
    rng = random.Random(seed + 100)
    area_frac = t.get("placement_area", 0.7)
    area_w = terrain_size_m * area_frac
    area_h = terrain_size_m * area_frac
    min_sp = t.get("min_spacing", 120)
    boundary_margin = t.get("boundary_margin", 60)
    min_elev_pct = t.get("min_elevation_percentile", 0.25)

    # Compute elevation histogram for filtering
    half = terrain_size_m / 2
    res = int(math.sqrt(len(pixels)) * 2) if isinstance(pixels, list) else 257
    elevs = []
    for py in range(len(pixels)):
        for px in range(len(pixels[0])):
            xw = px / len(pixels[0]) * terrain_size_m - half
            yw = py / len(pixels) * terrain_size_m - half
            z = _terrain_z(xw, yw, pixels, h_max, terrain_size_m)
            elevs.append(z)
    elevs.sort()
    min_elev = elevs[int(len(elevs) * min_elev_pct)]
    p95_elev = elevs[int(len(elevs) * 0.95)]

    def _acceptable(x, y):
        # Boundary check
        if abs(x) > half - boundary_margin or abs(y) > half - boundary_margin:
            return False
        z = _terrain_z(x, y, pixels, h_max, terrain_size_m)
        if z < min_elev or z > p95_elev:
            return False
        # Wide-area gradient check (30m radius) to avoid pits
        for step in [15, 30]:
            for dx, dy in [(step, 0), (-step, 0), (0, step), (0, -step),
                           (step, step), (-step, -step)]:
                nx, ny = x + dx, y + dy
                if abs(nx) > half - 10 or abs(ny) > half - 10:
                    continue
                nz = _terrain_z(nx, ny, pixels, h_max, terrain_size_m)
                if abs(nz - z) > 8:
                    return False
        return True

    count = t.get("count", 6)
    placement = t.get("placement", "random")

    if placement == "random":
        positions = []
        attempts = 0
        max_attempts = 5000
        while len(positions) < count and attempts < max_attempts:
            x = rng.uniform(-half + boundary_margin, half - boundary_margin)
            y = rng.uniform(-half + boundary_margin, half - boundary_margin)
            ok = _acceptable(x, y)
            if ok:
                for p in positions:
                    if math.hypot(x - p["x"], y - p["y"]) < min_sp:
                        ok = False
                        break
            if ok:
                z = round(_terrain_z(x, y, pixels, h_max, terrain_size_m), 1)
                scale = round(rng.uniform(t.get("scale_range", [0.15, 0.35])[0],
                                          t.get("scale_range", [0.15, 0.35])[1]), 2)
                positions.append({"x": round(x, 1), "y": round(y, 1),
                                  "z": z, "scale": scale})
            attempts += 1
        print(f"    Placed {len(positions)}/{count} (attempts={attempts})")
    elif placement == "grid":
        rows = t.get("grid_rows", 2)
        cols = t.get("grid_cols", 3)
        sp_x = t.get("spacing_x", 200)
        sp_y = t.get("spacing_y", 200)
        positions = []
        for r in range(rows):
            for c in range(cols):
                x = c * sp_x - (cols - 1) * sp_x / 2
                y = r * sp_y - (rows - 1) * sp_y / 2
                if _acceptable(x, y):
                    z = round(_terrain_z(x, y, pixels, h_max, terrain_size_m), 1)
                    scale = round(rng.uniform(t.get("scale_range", [0.15, 0.35])[0],
                                              t.get("scale_range", [0.15, 0.35])[1]), 2)
                    positions.append({"x": round(x, 1), "y": round(y, 1),
                                      "z": z, "scale": scale})

    print(f"  Turbines: {len(positions)} ({placement})")
    for p in positions:
        print(f"    ({p['x']:.0f}, {p['y']:.0f}) z={p['z']:.1f} scale={p['scale']:.2f}")
    return positions


def _place_towers(cfg, pixels, h_max, terrain_size_m):
    tx = cfg["transmission"]
    if not tx.get("enabled", False):
        return []
    seed = cfg["seed"]
    rng = random.Random(seed + 200)
    half = terrain_size_m / 2
    margin = 120  # TL model at 3x spans ~200m
    max_x = half - margin
    max_y = half - margin

    count = tx.get("count", 4)
    min_spacing = tx.get("min_spacing", 120)
    z_offset = tx.get("z_offset", 3.0)

    # TL model ground-contact points at 3x scale:
    # base_link (origin), poste2 at (50,0), poste3 at (69,45)
    POSTS_3X = [(0, 0), (150, 0), (207, 135)]

    towers = []
    attempts = 0
    max_attempts = 10000

    while len(towers) < count and attempts < max_attempts:
        # Origin must be placed so both posts land within terrain [-240, 240]
        x_min = -half + 10
        x_max = half - 10 - max(lx for lx, _ in POSTS_3X)
        y_min = -half + 10
        y_max = half - 10 - max(ly for _, ly in POSTS_3X)
        x = rng.uniform(x_min, x_max)
        y = rng.uniform(y_min, y_max)

        ok = True
        # All posts must be within terrain
        for lx, ly in POSTS_3X:
            wx, wy = x + lx, y + ly
            if abs(wx) > half - 10 or abs(wy) > half - 10:
                ok = False
                break
        if not ok:
            attempts += 1
            continue

        # Spacing from existing towers
        for t in towers:
            if math.hypot(x - t["x"], y - t["y"]) < min_spacing:
                ok = False
                break
        if not ok:
            attempts += 1
            continue

        # Sample terrain_z under both posts, use min
        post_zs = [_terrain_z(x + lx, y + ly,
                              pixels, h_max, terrain_size_m)
                   for lx, ly in POSTS_3X]

        # Gradient check between posts (207m span at 3x)
        if max(post_zs) - min(post_zs) > 20:
            ok = False

        if ok:
            z = round(min(post_zs) + z_offset, 1)
            towers.append({"name": f"tower_{len(towers)}",
                           "x": round(x, 1), "y": round(y, 1), "z": z})
        attempts += 1

    print(f"  Towers: {len(towers)} (attempts={attempts})")
    return towers


def _prepare_physics(cfg):
    return cfg.get("physics", {
        "gravity": [0, 0, -9.8066],
        "max_step_size": 0.004,
        "real_time_factor": 1.0,
    })


def _prepare_wind(cfg):
    return cfg.get("wind", {"enabled": False})


def _prepare_base_station(cfg, pixels, h_max, terrain_size_m):
    bs = cfg.get("base_station", {})
    if bs.get("enabled", False):
        pos = bs.get("position", [0, -60])
        z = round(_terrain_z(pos[0], pos[1], pixels, h_max,
                              terrain_size_m), 1)
        return {"x": pos[0], "y": pos[1], "z": z}
    return None


def _scatter_vegetation(cfg, pixels, h_max, terrain_size_m):
    veg = cfg.get("vegetation", {})
    if not veg.get("enabled", False):
        return []
    seed = cfg["seed"]
    rng = random.Random(seed + 300)
    half = terrain_size_m / 2
    margin = 20

    models = veg.get("models", ["bush_0", "bush_1", "palm_tree"])
    count = veg.get("count", 50)
    min_spacing = veg.get("min_spacing", 8)
    z_offset = veg.get("z_offset", 0.0)
    # Avoid placing in very low areas or very steep terrain
    min_elev = veg.get("min_elevation_percentile", 0.15)
    max_elev = veg.get("max_elevation_percentile", 0.85)

    # Pre-compute elevation percentile
    elevs = []
    for iy in range(257):
        for ix in range(257):
            elevs.append(pixels[iy][ix] / 255 * h_max)
    elevs.sort()
    lo = elevs[int(len(elevs) * min_elev)]
    hi = elevs[int(len(elevs) * max_elev)]

    placed = []
    attempts = 0
    max_attempts = 2000

    while len(placed) < count and attempts < max_attempts:
        x = rng.uniform(-half + margin, half - margin)
        y = rng.uniform(-half + margin, half - margin)
        z = _terrain_z(x, y, pixels, h_max, terrain_size_m)

        # Skip if too low/high
        if z < lo or z > hi:
            attempts += 1
            continue

        # Skip if too steep (gradient in 5m radius)
        steep = False
        for dx, dy in [(5, 0), (-5, 0), (0, 5), (0, -5)]:
            nx, ny = x + dx, y + dy
            if abs(nx) > half - 5 or abs(ny) > half - 5:
                continue
            nz = _terrain_z(nx, ny, pixels, h_max, terrain_size_m)
            if abs(nz - z) > 3:
                steep = True
                break
        if steep:
            attempts += 1
            continue

        # Spacing from other vegetation
        too_close = False
        for p in placed:
            if math.hypot(x - p["x"], y - p["y"]) < min_spacing:
                too_close = True
                break
        if too_close:
            attempts += 1
            continue

        model = rng.choice(models)
        placed.append({
            "name": f"veg_{len(placed)}",
            "model": model,
            "x": round(x, 1),
            "y": round(y, 1),
            "z": round(z + z_offset, 1),
            "yaw": round(rng.uniform(0, 6.283), 2),
        })
        attempts += 1

    print(f"  Vegetation: {len(placed)} (attempts={attempts})")
    return placed


def _scatter_rocks(cfg, pixels, h_max, terrain_size_m):
    rocks_cfg = cfg.get("rocks", {})
    if not rocks_cfg.get("enabled", False):
        return []
    seed = cfg["seed"]
    rng = random.Random(seed + 400)
    half = terrain_size_m / 2
    margin = 15

    count = rocks_cfg.get("count", 30)
    min_size = rocks_cfg.get("min_size", 1.0)
    max_size = rocks_cfg.get("max_size", 25.0)

    placed = []
    attempts = 0
    max_attempts = 2000

    rock_models = [f"rock_{i}" for i in range(8)]

    while len(placed) < count and attempts < max_attempts:
        x = rng.uniform(-half + margin, half - margin)
        y = rng.uniform(-half + margin, half - margin)
        z = _terrain_z(x, y, pixels, h_max, terrain_size_m)

        # Avoid very steep terrain
        steep = False
        for dx, dy in [(5, 0), (-5, 0), (0, 5), (0, -5)]:
            nx, ny = x + dx, y + dy
            if abs(nx) > half - 5 or abs(ny) > half - 5:
                continue
            nz = _terrain_z(nx, ny, pixels, h_max, terrain_size_m)
            if abs(nz - z) > 4:
                steep = True
                break
        if steep:
            attempts += 1
            continue

        # Power-law distribution: most rocks small, fewer large
        rn = rng.random()
        scale = (min_size + (max_size - min_size) * (rn ** 2.0)) * 0.2

        model = rng.choice(rock_models)
        placed.append({
            "name": f"rock_{len(placed)}",
            "mesh_uri": f"model://desert_rocks/{model}/meshes/{model}.obj",
            "x": round(x, 1),
            "y": round(y, 1),
            "z": round(z, 1),
            "scale": round(scale, 2),
            "yaw": round(rng.uniform(0, 6.283), 2),
        })
        attempts += 1

    print(f"  Rocks: {len(placed)} (attempts={attempts})")
    return placed


def generate_world(cfg, output_dir):
    os.makedirs(output_dir, exist_ok=True)
    seed = cfg["seed"]
    terrain_cfg = cfg["terrain"]
    h_max = terrain_cfg["height_max"]

    print(f"\n🌵 Desert world (seed={seed}) → {output_dir}")

    # 1. Generate terrain + get pixel data
    terrain_png, terrain_size, pixels = generate_terrain_model(
        output_dir, terrain_cfg, seed)
    terrain_size_m = terrain_size[0]  # X dimension in meters

    # 2. Turbines (with terrain z)
    turbines = _place_turbines(cfg, pixels, h_max, terrain_size_m)

    # 3. Towers (with terrain z, clamped to bounds)
    towers = _place_towers(cfg, pixels, h_max, terrain_size_m)

    # 4. Base station
    base_station = _prepare_base_station(cfg, pixels, h_max, terrain_size_m)
    physics = _prepare_physics(cfg)
    wind = _prepare_wind(cfg)

    # 5. Vegetation (scattered)
    vegetation = _scatter_vegetation(cfg, pixels, h_max, terrain_size_m)

    # 6. Rocks (procedural box shapes)
    rocks = _scatter_rocks(cfg, pixels, h_max, terrain_size_m)

    # 7. Render SDF
    env = Environment(loader=FileSystemLoader(TEMPLATE_DIR), trim_blocks=True, lstrip_blocks=True)
    template = env.get_template(TEMPLATE_NAME)
    world_sdf = template.render(
        world_name=cfg.get("world_name", "desert_windfarm"),
        terrain_png_path=os.path.abspath(terrain_png),
        terrain_size=terrain_size,
        turbines=turbines,
        towers=towers,
        base_station=base_station,
        vegetation=vegetation,
        rocks=rocks,
        physics=physics,
        wind=wind,
    )

    world_path = os.path.join(output_dir, f"{cfg['world_name']}.world")
    with open(world_path, "w") as f:
        f.write(world_sdf)
    print(f"\n✅ World: {world_path}")
    print(f"   gzserver {world_path} &")
    print(f"   gzclient --verbose &")


def main():
    parser = argparse.ArgumentParser(
        description="Generate configurable desert wind farm world for Gazebo 11")
    parser.add_argument("--config", default=DEFAULT_CONFIG, help="YAML config")
    parser.add_argument("--output", default="/home/travis/zcw/1.2/assets/worlds/generated",
                        help="Output dir")
    parser.add_argument("--seed", type=int, help="Random seed")
    parser.add_argument("--turbine-count", type=int, help="# wind turbines")
    parser.add_argument("--terrain-height", type=float,
                        help="Max terrain elevation (m)", dest="terrain_height")
    parser.add_argument("--tower-pairs", type=int, help="# tower pairs")
    parser.add_argument("--no-wind", action="store_true", help="Disable wind")
    parser.add_argument("--no-base", action="store_true", help="No base station")
    args = parser.parse_args()

    overrides = {}
    if args.seed is not None:
        overrides["seed"] = args.seed
    if args.turbine_count is not None:
        overrides["turbines"] = {"count": args.turbine_count,
                                 "placement": "random"}
    if args.terrain_height is not None:
        overrides["terrain"] = {"height_max": args.terrain_height}
    if args.tower_pairs is not None:
        overrides["transmission"] = {"tower_pairs": args.tower_pairs}
    if args.no_wind:
        overrides["wind"] = {"enabled": False}
    if args.no_base:
        overrides["base_station"] = {"enabled": False}

    cfg = load_config(args.config, overrides)
    generate_world(cfg, args.output)


if __name__ == "__main__":
    main()
