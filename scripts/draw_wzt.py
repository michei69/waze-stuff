#!/usr/bin/env python3
"""
Waze Tile Viewer - renders decompressed .wzt tiles with full detail.

Usage:
    python draw_wzt.py <file.wzt>
    python draw_wzt.py <file.wzdf>   (auto-decompresses)
    python draw_wzt.py               (prompts for file)

Controls:
    Mouse drag       - pan
    Mouse wheel      - zoom in/out
    Arrow keys / WASD - pan
    +/-              - zoom
    R                - reset view
    N                - toggle street name labels
    P                - toggle point nodes
    L                - toggle road type legend
    B                - toggle broken/fake point markers
    I                - toggle tile info overlay
    ESC / Q          - quit

Requirements:
    pip install pygame
"""

import sys
import math
from collections import defaultdict

import pygame

from waze_tile_classes import WazeTile

CFCC_COLORS = {
    12: (170, 210, 140),   # Land / ground
    15: (210, 195, 170),   # Building / venue
    16: (240, 240, 230),   # City tile background (entire tile - skip or draw last)
    20: (140, 200, 240),   # Water
    # extras seen in the wild, unchecked:
    10: (80,  160, 230),   # Stream / river
    11: (60,  140, 220),   # River
    13: (50,  130, 210),   # Reservoir / canal
    14: (30,  100, 190),   # Ocean / sea
    21: (140, 200, 100),   # Forest
    22: (120, 190, 80),    # Golf
    23: (100, 170, 60),    # Cemetery
    30: (220, 210, 190),   # Generic building
    31: (200, 190, 170),   # Commercial
    40: (230, 225, 210),   # Built-up area
    50: (180, 170, 160),   # Airport
}
CFCC_LABELS = {
    12: "Land",
    15: "Building / Venue",
    16: "City",
    20: "Water",
    10: "Stream / River",
    11: "River",
    13: "Reservoir / Canal",
    14: "Ocean / Sea",
    21: "Forest",
    22: "Golf",
    23: "Cemetery",
    30: "Generic Building",
    31: "Commercial",
    40: "Built-up Area",
    50: "Airport",
}
DEFAULT_POLYGON_COLOR = (220, 195, 160)

# Road type -> colour and label
ROAD_TYPE_COLORS = {
    0:  (180, 180, 180),   # unknown (sometimes boulevard??)
    1:  (255, 60,  40),    # freeway - bold red
    2:  (255, 140, 40),    # major highway - orange
    3:  (255, 200, 50),    # minor highway / boulevard - gold
    4:  (255, 160, 100),   # ramp - light orange
    5:  (255, 240, 80),    # primary street - yellow
    6:  (230, 230, 180),   # passage way - pale
    7:  (200, 200, 200),   # street - light gray
    8:  (140, 200, 140),   # non-routable pedestrian - green
    9:  (180, 150, 110),   # dirt path - brown
    10: (160, 210, 160),   # routable pedestrian - green
    11: (160, 160, 180),   # stairway - gray-purple
    12: (140, 120, 140),   # railway - dark purple
    13: (180, 180, 200),   # runway/taxiway - light blue-gray
    14: (100, 140, 200),   # ferry - blue
    15: (220, 160, 120),   # private road - tan
    16: (180, 180, 190),   # parking lot road - blue-gray
}

ROAD_LABELS = {
    1: "FREEWAY", 2: "HIGHWAY", 3: "BOULEVARD", 4: "RAMP", 5: "PRIMARY",
    6: "PASSAGE", 7: "STREET", 8: "PEDESTRIAN", 9: "DIRT", 10: "PATH",
    11: "STAIRWAY", 12: "RAILWAY", 13: "RUNWAY", 14: "FERRY", 15: "PRIVATE", 16: "PARKING",
}

DEFAULT_LINE_COLOR = (100, 100, 100)
FAKE_POINT_COLOR = (255, 50, 50)       # tile-border split point
REAL_POINT_COLOR = (50, 50, 220)        # real intersection
ROUNDABOUT_COLOR = (255, 180, 50)

# ref at zoom 1x
ROAD_WIDTHS = defaultdict(lambda: 2, {
    0: 1, 1: 8, 2: 6, 3: 5, 4: 4, 5: 3,
    6: 2, 7: 3, 8: 1, 9: 2, 10: 1, 11: 1,
    12: 2, 13: 5, 14: 2, 15: 2, 16: 2,
})


# ---------------------------------------------------------------------------
# GEOMETRY HELPERS
# ---------------------------------------------------------------------------

def _world_to_screen(wx, wy, ox, oy, zoom):
    return int((wx - ox) * zoom), int((oy - wy) * zoom)


def _screen_to_world(sx, sy, ox, oy, zoom):
    return sx / zoom + ox, oy - sy / zoom


def _darken(color, factor):
    return tuple(max(0, int(c * factor)) for c in color)


def _lighten(color, factor):
    return tuple(min(255, int(c + (255 - c) * (1 - factor))) for c in color)


# ---------------------------------------------------------------------------
# DRAWING
# ---------------------------------------------------------------------------

def _draw_polygons(surface, tile, ox, oy, zoom):
    """Draw filled polygons layered by CFCC priority.

    Draw order (back → front):
      1. CFCC 16 - city tile / land base    (lowest)
      2. CFCC 12 - land / ground
      3. CFCC 20 - water (on top of land)
      4. everything else - parks, forest, etc.
      5. CFCC 15 - buildings                (highest)
    """
    # Sort by draw priority (lowest value drawn first = back)
    CFCC_PRIORITY = {16: 0, 12: 1, 20: 2, 15: 99}

    def _poly_priority(poly):
        return CFCC_PRIORITY.get(poly.cfcc, 50)

    sorted_polys = sorted(tile.polygons, key=_poly_priority)
    for poly in sorted_polys:
        color = CFCC_COLORS.get(poly.cfcc, DEFAULT_POLYGON_COLOR)
        pts = [_world_to_screen(p.x, p.y, ox, oy, zoom) for p in poly.points]
        if len(pts) >= 3:
            try:
                pygame.draw.polygon(surface, color, pts)
                pygame.draw.polygon(surface, _darken(color, 0.65), pts, 1)
            except ValueError:
                pass


def _draw_all_roads(surface, tile, ox, oy, zoom, show_names, font_factory):
    """Draw all road segments sorted by road type priority.

    Roads with shape chains are drawn as polylines; straight roads as single
    segments.
    """
    entries = []
    for i, line in enumerate(tile.lines):
        if line.first_point_idx >= len(tile.points):
            continue
        if line.second_point_idx >= len(tile.points):
            continue
        priority = -line.road_type
        entries.append((priority, i))

    entries.sort(key=lambda e: e[0])

    for _, i in entries:
        line = tile.lines[i]
        color = ROAD_TYPE_COLORS.get(line.road_type, DEFAULT_LINE_COLOR) if line.attributes == 0 else (255,0,0)
        width = max(1, ROAD_WIDTHS[line.road_type])

        if (line.attributes != 0): print(line.street.name, line.attributes)

        if line.shape_chain:
            pts = line.shape_chain
            screen_pts = [_world_to_screen(x, y, ox, oy, zoom) for (x, y) in pts]
            # Add the to-point for the final straight segment
            p_to = tile.points[line.second_point_idx]
            screen_pts.append(_world_to_screen(p_to.x, p_to.y, ox, oy, zoom))
            if len(screen_pts) >= 2:
                pygame.draw.lines(surface, color, False, screen_pts, width)
        else:
            p1 = tile.points[line.first_point_idx]
            p2 = tile.points[line.second_point_idx]
            sx1, sy1 = _world_to_screen(p1.x, p1.y, ox, oy, zoom)
            sx2, sy2 = _world_to_screen(p2.x, p2.y, ox, oy, zoom)
            pygame.draw.line(surface, color, (sx1, sy1), (sx2, sy2), width)

        if show_names:
            p1 = tile.points[line.first_point_idx]
            p2 = tile.points[line.second_point_idx]
            sx1, sy1 = _world_to_screen(p1.x, p1.y, ox, oy, zoom)
            sx2, sy2 = _world_to_screen(p2.x, p2.y, ox, oy, zoom)
            shape_pts = line.shape_chain if line.shape_chain else None
            _draw_line_label(surface, line, p1, p2, sx1, sy1, sx2, sy2,
                             ox, oy, zoom, font_factory, chain_pts=shape_pts)


_font_cache = {}

def _get_scaled_font(zoom):
    """Return a font whose pixel size scales with zoom. Cached per size."""
    # base size 14 at 1x zoom → ~9px rendered height; scales up/down with zoom
    size = max(8, min(56, int(14 * zoom)))
    if size not in _font_cache:
        try:
            _font_cache[size] = pygame.font.SysFont("sans-serif", size)
        except Exception:
            _font_cache[size] = pygame.font.Font(None, size)
    return _font_cache[size]


def _draw_line_label(surface, line, p1, p2, sx1, sy1, sx2, sy2, ox, oy, zoom,
                    font_factory, chain_pts=None):
    """Draw the street name and/or road-type badge at the line midpoint.

    If *chain_pts* is given, the label sits at the midpoint of the shape
    curve instead of the straight-line midpoint.
    """
    if chain_pts:
        screen_pts = [_world_to_screen(x, y, ox, oy, zoom) for (x, y) in chain_pts]
        # Also include to-point for full path length
        screen_pts.append((sx2, sy2))
        seg_len = 0.0
        for j in range(1, len(screen_pts)):
            seg_len += math.hypot(screen_pts[j][0] - screen_pts[j - 1][0],
                                   screen_pts[j][1] - screen_pts[j - 1][1])
        if seg_len < 30:
            return
        half = seg_len / 2
        acc = 0.0
        mx = my = 0
        dx = dy = 0
        for j in range(1, len(screen_pts)):
            d = math.hypot(screen_pts[j][0] - screen_pts[j - 1][0],
                            screen_pts[j][1] - screen_pts[j - 1][1])
            if acc + d >= half:
                t = (half - acc) / d if d > 0 else 0
                mx = int(screen_pts[j - 1][0] + t * (screen_pts[j][0] - screen_pts[j - 1][0]))
                my = int(screen_pts[j - 1][1] + t * (screen_pts[j][1] - screen_pts[j - 1][1]))
                # Use world-space direction of this segment for text angle
                pt_a = chain_pts[j - 1] if j - 1 < len(chain_pts) else chain_pts[-1]
                pt_b = chain_pts[j] if j < len(chain_pts) else (p2.x, p2.y)
                dx = pt_b[0] - pt_a[0]
                dy = pt_b[1] - pt_a[1]
                break
            acc += d
        else:
            return
    else:
        seg_len = math.hypot(sx2 - sx1, sy2 - sy1)
        if seg_len < 30:
            return
        mx, my = (sx1 + sx2) // 2, (sy1 + sy2) // 2
        dx, dy = p2.x - p1.x, p2.y - p1.y

    # Prefer street name; fall back to road-type abbreviation
    label = ""
    if line.street and line.street.name.strip():
        label = line.street.name.strip()
    elif line.road_type in ROAD_LABELS:
        label = ROAD_LABELS[line.road_type]

    if not label:
        return

    try:
        font = font_factory(zoom)
        txt = font.render(label, True, (50, 50, 50))
        angle = math.degrees(math.atan2(dy, dx))
        # Keep upright: flip 180° if reading upside-down
        if angle < -90:
            angle += 180
        elif angle > 90:
            angle -= 180
        rotated = pygame.transform.rotate(txt, angle)
        rw, rh = rotated.get_size()
        surface.blit(rotated, (mx - rw // 2, my - rh // 2))
    except Exception:
        pass


def _draw_broken_markers(surface, tile, ox, oy, zoom):
    """Highlight fake (tile-border) point endpoints with purple rings.

    Fake flag (0x8000) is on the LineData from/to fields, NOT on PointData.
    """
    raw_lines = tile.raw_line_data
    if not raw_lines:
        return
    seen = set()
    for i, line in enumerate(tile.lines):
        line_off = i * 8
        if line_off + 4 > len(raw_lines):
            continue
        for pt_idx, byte_off in ((line.first_point_idx, 0),
                                  (line.second_point_idx, 2)):
            if pt_idx >= len(tile.points) or pt_idx in seen:
                continue
            raw_val = int.from_bytes(
                raw_lines[line_off + byte_off:line_off + byte_off + 2], "little")
            if raw_val & 0x8000:
                seen.add(pt_idx)
                p = tile.points[pt_idx]
                sx, sy = _world_to_screen(p.x, p.y, ox, oy, zoom)
                pygame.draw.circle(surface, (200, 80, 200), (sx, sy), 6, 2)


def _draw_points(surface, tile, ox, oy, zoom):
    """Draw all point nodes - red=fake (tile border), blue=real.

    Fake flag comes from LineData's from/to fields, not from PointData coords.
    """
    # Build set of fake point indices from raw line data
    fake_pts = set()
    raw_lines = tile.raw_line_data
    if raw_lines:
        for i in range(min(len(tile.lines), len(raw_lines) // 8)):
            off = i * 8
            for b in (0, 2):  # from, to
                if off + b + 2 <= len(raw_lines):
                    val = int.from_bytes(raw_lines[off + b:off + b + 2], "little")
                    if val & 0x8000:
                        fake_pts.add(val & 0x7fff)

    for i, pt in enumerate(tile.points):
        color = FAKE_POINT_COLOR if i in fake_pts else REAL_POINT_COLOR
        sx, sy = _world_to_screen(pt.x, pt.y, ox, oy, zoom)
        r = 5 if i in fake_pts else 3
        pygame.draw.circle(surface, color, (sx, sy), r)


def _draw_roundabouts(surface, tile, ox, oy, zoom):
    """Highlight roundabout nodes with orange rings."""
    for line in tile.lines:
        if line.road_type != 10:
            continue
        if line.first_point_idx < len(tile.points):
            p = tile.points[line.first_point_idx]
            sx, sy = _world_to_screen(p.x, p.y, ox, oy, zoom)
            pygame.draw.circle(surface, ROUNDABOUT_COLOR, (sx, sy), 12, 3)


# ---------------------------------------------------------------------------
# UI OVERLAYS
# ---------------------------------------------------------------------------

def _draw_info_panel(surface, tile, zoom, ox, oy, font):
    """Tile metadata in top-left corner."""
    lines = [
        f"Tile {tile.square_data.tile_id}  "
        f"scale={tile.square_data.actual_scale}  "
        f"({tile.square_data.latitude:.4f}, {tile.square_data.longitude:.4f})",
        f"entries={tile.entry_count}  mask={tile.mask_bits}"
        f"  lines={len(tile.lines)}  pts={len(tile.points)}",
        f"streets={len(tile.streets)}  polygons={len(tile.polygons)}",
        f"zoom={zoom:.2f}x",
    ]
    y = 8
    for txt in lines:
        s = font.render(txt, True, (255, 255, 255), (0, 0, 0))
        surface.blit(s, (8, y))
        y += 15


def _draw_legend(surface, tile, font):
    """Road-type colour legend in the bottom-left corner."""
    # Figure out which types are actually used
    used = set(line.road_type for line in tile.lines)
    if not used:
        return

    items = sorted((rt for rt in ROAD_TYPE_COLORS if rt in used),
                   key=lambda rt: rt)
    if not items:
        return

    # Background
    n = len(items)
    box_w, box_h = 185, n * 16 + 12
    x, y = 8, surface.get_height() - box_h - 8
    legend_bg = pygame.Surface((box_w, box_h))
    legend_bg.set_alpha(210)
    legend_bg.fill((240, 240, 240))
    surface.blit(legend_bg, (x, y))

    for i, rt in enumerate(items):
        ly = y + 8 + i * 16
        color = ROAD_TYPE_COLORS[rt]
        label = ROAD_LABELS.get(rt, f"t{rt}")
        pygame.draw.line(surface, color, (x + 6, ly + 7), (x + 36, ly + 7),
                         ROAD_WIDTHS[rt])
        txt = font.render(f"{label}", True, (0, 0, 0))
        surface.blit(txt, (x + 44, ly + 2))

    # Polygon legend
    poly_used = set(p.cfcc for p in tile.polygons if p.cfcc != 16)
    if poly_used:
        px = x + box_w + 8
        py = surface.get_height() - len(poly_used) * 16 - 12
        poly_bg = pygame.Surface((140, len(poly_used) * 16 + 12))
        poly_bg.set_alpha(210)
        poly_bg.fill((240, 240, 240))
        surface.blit(poly_bg, (px, py))
        for j, cfcc in enumerate(sorted(poly_used)):
            color = CFCC_COLORS.get(cfcc, DEFAULT_POLYGON_COLOR)
            pygame.draw.rect(surface, color, (px + 6, py + 8 + j * 16, 14, 14))
            pygame.draw.rect(surface, _darken(color, 0.6),
                             (px + 6, py + 8 + j * 16, 14, 14), 1)
            txt = font.render(CFCC_LABELS[cfcc] if cfcc in CFCC_LABELS else f"CFCC {cfcc}", True, (0, 0, 0))
            surface.blit(txt, (px + 26, py + 8 + j * 16))


# ---------------------------------------------------------------------------
# MAIN
# ---------------------------------------------------------------------------

def main(data: bytes, verbose: bool = False):
    tile = WazeTile(data)

    if verbose:
        print(f"Tile {tile.square_data.tile_id}  scale={tile.square_data.actual_scale}")
        print(f"  entry_count={tile.entry_count}  mask_bits={tile.mask_bits}")
        print(f"  lines={len(tile.lines)}  points={len(tile.points)}")
        print(f"  streets={len(tile.streets)}  polygons={len(tile.polygons)}")
        print(f"  shape_bytes={len(tile.shapes)}  shape_count={tile.shapes_count}")
        if tile.points:
            xs = [p.x for p in tile.points]
            ys = [p.y for p in tile.points]
            print(f"  point_range: x=[{min(xs)}, {max(xs)}]  y=[{min(ys)}, {max(ys)}]")
        if tile.streets:
            names = [s.name for s in tile.streets if s.name.strip()]
            if names:
                print(f"  street names: {names[:5]}{'...' if len(names) > 5 else ''}")
        types = set(l.road_type for l in tile.lines)
        print(f"  road_types: {sorted(types)}")

    pygame.init()
    info = pygame.display.Info()
    # Fall back to 1280×800 if display info unavailable (headless / tty)
    dw = info.current_w if info.current_w > 0 else 1280
    dh = info.current_h if info.current_h > 0 else 800
    W, H = min(dw - 100, 1500), min(dh - 100, 950)
    if W < 400: W = 1280
    if H < 300: H = 800
    screen = pygame.display.set_mode((W, H), pygame.RESIZABLE)
    pygame.display.set_caption(
        f"Waze Tile {tile.square_data.tile_id}  "
        f"({tile.square_data.latitude:.4f}, {tile.square_data.longitude:.4f})"
    )

    clock = pygame.time.Clock()
    font_small = pygame.font.SysFont("monospace", 12)
    font_tiny = pygame.font.SysFont("monospace", 10)

    # Compute world bounds from points only (polygon points can be outliers)
    xs = [p.x for p in tile.points]
    ys = [p.y for p in tile.points]
    if not xs:
        xs, ys = [0, 100], [0, 100]
    min_x, max_x = min(xs), max(xs)
    min_y, max_y = min(ys), max(ys)
    ww, wh = max_x - min_x or 100, max_y - min_y or 100
    pad_x, pad_y = ww * 0.15, wh * 0.15
    min_x -= pad_x; max_x += pad_x; min_y -= pad_y; max_y += pad_y

    zoom = min(W / (max_x - min_x), H / (max_y - min_y)) * 0.92
    ox = (min_x + max_x) / 2 - W / 2 / zoom
    oy = (min_y + max_y) / 2 + H / 2 / zoom

    if verbose:
        print(f"  view: {W}x{H}  zoom={zoom:.4f}x  origin=({ox:.0f},{oy:.0f})")
        print(f"  world_bounds: ({min_x:.0f},{min_y:.0f})-({max_x:.0f},{max_y:.0f})")

    # Toggles
    show_names = True
    show_points = False
    show_legend = True
    show_info = True
    show_broken = True

    running = True
    dragging = False
    drag_start = (0, 0)
    drag_ox = drag_oy = 0.0

    while running:
        dt = max(clock.tick(60) / 1000.0, 0.001)

        for ev in pygame.event.get():
            if ev.type == pygame.QUIT:
                running = False

            elif ev.type == pygame.KEYDOWN:
                if ev.key in (pygame.K_ESCAPE, pygame.K_q):
                    running = False
                elif ev.key == pygame.K_r:
                    zoom = min(W / (max_x - min_x), H / (max_y - min_y)) * 0.92
                    ox = (min_x + max_x) / 2 - W / 2 / zoom
                    oy = (min_y + max_y) / 2 + H / 2 / zoom
                elif ev.key == pygame.K_n:
                    show_names = not show_names
                elif ev.key == pygame.K_p:
                    show_points = not show_points
                elif ev.key == pygame.K_l:
                    show_legend = not show_legend
                elif ev.key == pygame.K_i:
                    show_info = not show_info
                elif ev.key == pygame.K_b:
                    show_broken = not show_broken
                elif ev.key in (pygame.K_PLUS, pygame.K_EQUALS, pygame.K_KP_PLUS):
                    zoom *= 1.25
                elif ev.key in (pygame.K_MINUS, pygame.K_KP_MINUS):
                    zoom /= 1.25

            elif ev.type == pygame.MOUSEWHEEL:
                mx, my = pygame.mouse.get_pos()
                wx, wy = _screen_to_world(mx, my, ox, oy, zoom)
                zoom *= 1.2 if ev.y > 0 else 1 / 1.2
                ox = wx - mx / zoom
                oy = wy + my / zoom

            elif ev.type == pygame.MOUSEBUTTONDOWN:
                if ev.button == 1:
                    dragging = True
                    drag_start = ev.pos
                    drag_ox, drag_oy = ox, oy

            elif ev.type == pygame.MOUSEBUTTONUP:
                if ev.button == 1:
                    dragging = False

            elif ev.type == pygame.MOUSEMOTION:
                if dragging:
                    dx = ev.pos[0] - drag_start[0]
                    dy = ev.pos[1] - drag_start[1]
                    ox = drag_ox - dx / zoom
                    oy = drag_oy + dy / zoom

            elif ev.type == pygame.VIDEORESIZE:
                W, H = ev.size
                screen = pygame.display.set_mode((W, H), pygame.RESIZABLE)

        # Keyboard pan
        keys = pygame.key.get_pressed()
        speed = 350 / zoom * dt
        if keys[pygame.K_LEFT] or keys[pygame.K_a]:
            ox -= speed
        if keys[pygame.K_RIGHT] or keys[pygame.K_d]:
            ox += speed
        if keys[pygame.K_UP] or keys[pygame.K_w]:
            oy += speed
        if keys[pygame.K_DOWN] or keys[pygame.K_s]:
            oy -= speed

        # ---- Draw ----
        screen.fill((252, 252, 248))

        # Layer 1: polygons (land base → water → parks → buildings)
        _draw_polygons(screen, tile, ox, oy, zoom)

        # Layer 2: all roads sorted by road type priority
        _draw_all_roads(screen, tile, ox, oy, zoom, show_names, _get_scaled_font)

        # Roundabout markers
        _draw_roundabouts(screen, tile, ox, oy, zoom)

        # Broken/fake point markers
        if show_broken:
            _draw_broken_markers(screen, tile, ox, oy, zoom)

        # Point nodes (debug)
        if show_points:
            _draw_points(screen, tile, ox, oy, zoom)

        # Overlays
        if show_info:
            _draw_info_panel(screen, tile, zoom, ox, oy, font_small)
        if show_legend:
            _draw_legend(screen, tile, font_tiny)

        pygame.display.flip()

    pygame.quit()


if __name__ == "__main__":
    verbose = False
    args = [a for a in sys.argv[1:] if not a.startswith("-")]
    if "--verbose" in sys.argv or "-v" in sys.argv:
        verbose = True

    if args:
        path = args[0]
    else:
        path = input("File: ").strip()

    with open(path, "rb") as f:
        raw = f.read()

    if path.endswith(".wzdf"):
        from decompress_wzdf import decompress_raw
        raw = decompress_raw(raw)

    main(raw, verbose=verbose)
