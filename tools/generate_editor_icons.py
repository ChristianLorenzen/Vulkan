#!/usr/bin/env python3
"""Generates the stylized asset-explorer icons under assets/editor/icons/.

Look: painterly stylized -- gouache illustration, not textured craft paper.
Form comes from broad soft light and shadow planes plus visible brush strokes;
silhouettes carry only a whisper of hand-drawn waver and the ink line stays
confident and even. Deliberately no speckle or grain.

Everything is drawn at 5x and downsampled, so it stays clean at 16-32px.
"""

import os
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

OUT = 200
S = 5
N = OUT * S

OUTLINE = (58, 45, 52, 255)
OUTLINE_SOFT = (88, 70, 76, 220)
LW = 0.019 * N

INK_LIGHT = (255, 250, 234)
INK_SHADE = (104, 82, 100)   # violet-leaning shadow, the way gouache shades warm hues

# Silhouettes want only a hint of hand -- past this it starts reading as craft paper.
WOBBLE = 0.40

# Global brushwork damping. Visible strokes tip these from stylized toward
# straight illustration, so the marks stay just under the threshold of notice.
BRUSH = 0.55


# --------------------------------------------------------------------------
# Canvas / paint helpers
# --------------------------------------------------------------------------
def px(p):
    return (p[0] * N, p[1] * N)


def poly(pts):
    return [px(p) for p in pts]


def new_canvas():
    return Image.new("RGBA", (N, N), (0, 0, 0, 0))


def lin_grad(c0, c1, angle_deg=90.0):
    a = np.deg2rad(angle_deg)
    y, x = np.mgrid[0:N, 0:N].astype(np.float32) / (N - 1)
    t = x * np.cos(a) + y * np.sin(a)
    t = (t - t.min()) / (t.max() - t.min())
    t = t[..., None]
    rgb = np.array(c0, np.float32) * (1 - t) + np.array(c1, np.float32) * t
    out = np.dstack([rgb, np.full((N, N, 1), 255, np.float32)])
    return Image.fromarray(out.astype(np.uint8), "RGBA")


def noise(cells, seed, blur=0.0):
    rng = np.random.RandomState(seed)
    a = (rng.rand(cells, cells) * 255).astype(np.uint8)
    img = Image.fromarray(a, "L").resize((N, N), Image.BICUBIC)
    if blur:
        img = img.filter(ImageFilter.GaussianBlur(blur * S))
    return img


def mask_from(draw_fn):
    m = Image.new("L", (N, N), 0)
    draw_fn(ImageDraw.Draw(m))
    return m


def paint(base, mask, fill):
    if isinstance(fill, Image.Image):
        base.paste(fill, (0, 0), mask)
    else:
        if len(fill) == 3:
            fill = fill + (255,)
        base.paste(Image.new("RGBA", (N, N), fill), (0, 0), mask)


def _L(arr):
    return Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8), "L")


def rim(base, mask, color, amount, radius, dx, dy):
    """Painted band just inside an edge of `mask`.

    The blurred copy is nudged by (dx, dy); whatever the shifted copy fails to
    cover lights up. Positive dx/dy therefore lights the top-left edges.
    """
    m = np.asarray(mask, np.float32) / 255.0
    b = np.asarray(mask.filter(ImageFilter.GaussianBlur(radius * N)), np.float32) / 255.0
    b = np.roll(np.roll(b, int(dy * N), axis=0), int(dx * N), axis=1)
    band = m * np.clip(1.0 - b, 0, 1) * amount * 255.0
    paint(base, _L(band), color)


def edge_paint(base, mask, light=0.40, shade=0.34, radius=0.020, d=0.058, pool=0.20):
    """Painted form: a light plane top-left, a shadow plane bottom-right.

    Wide `d` with a tight `radius` is what makes this read as paint: the tone
    covers a broad side of the form but keeps a discernible edge, the way a
    loaded brush leaves a shadow shape rather than an airbrushed falloff.
    `pool` is the darker rim gouache leaves where pigment settles at a boundary.
    """
    rim(base, mask, INK_LIGHT, light, radius, d, d)
    rim(base, mask, INK_SHADE, shade, radius, -d, -d)
    if pool:
        rim(base, mask, INK_SHADE, pool, 0.009, 0.0, 0.0)


def shade(base, mask, color, amount, blur, offset=(0, 0)):
    g = mask.filter(ImageFilter.GaussianBlur(blur * S))
    if offset != (0, 0):
        g = g.transform(
            (N, N), Image.AFFINE, (1, 0, -offset[0] * S, 0, 1, -offset[1] * S)
        )
    g = _L(np.asarray(g, np.float32) * amount)
    g = Image.composite(g, Image.new("L", (N, N), 0), mask)
    paint(base, g, color)


def texture_pass(base, mask, seed, cells=26, strength=26, dark=(58, 46, 44)):
    """Broad tonal drift across a fill.

    Frequency is clamped low on purpose: fine noise here is exactly what made
    the icons look like textured paper instead of paint.
    """
    cells = max(3, min(cells, 8))
    n = np.asarray(noise(cells, seed, blur=2.2), np.float32) / 255.0
    m = _L(np.clip((n - 0.45) * 1.6, 0, 1) * strength * 0.55)
    paint(base, Image.composite(m, Image.new("L", (N, N), 0), mask), dark)


def tint(c, f):
    """Lighten (f > 0) or deepen (f < 0) a colour, keeping it in the same family."""
    c = np.array(c, np.float32)
    t = np.array((255, 250, 236), np.float32) if f > 0 else np.array((58, 44, 52),
                                                                    np.float32)
    return tuple(int(v) for v in c + (t - c) * abs(f))


def strokes(base, mask, seed, strength=14, count=5, angle=38.0, base_color=None,
            light=None, dark=None):
    """Loose brush strokes laid across a fill, alternating light and dark loads.

    Replaces the old speckle pass: painters leave directional marks, not grain.
    """
    bbox = mask.getbbox()
    if bbox is None:
        return
    strength *= BRUSH
    if base_color is not None:
        light = tint(base_color, 0.30)
        dark = tint(base_color, -0.22)
    light = light or (255, 252, 240)
    dark = dark or (150, 130, 130)
    x0, y0, x1, y1 = bbox
    rng = np.random.RandomState(seed)
    a = np.deg2rad(angle)
    ux, uy = np.cos(a), np.sin(a)
    empty = Image.new("L", (N, N), 0)
    for i in range(count):
        cx = rng.uniform(x0, x1)
        cy = rng.uniform(y0, y1)
        ln = rng.uniform(0.22, 0.52) * max(x1 - x0, y1 - y0)
        w = rng.uniform(0.05, 0.11) * max(x1 - x0, y1 - y0)
        p0 = (cx - ux * ln / 2, cy - uy * ln / 2)
        p1 = (cx + ux * ln / 2, cy + uy * ln / 2)
        m = mask_from(lambda d, a=p0, b=p1, w=w: d.line([a, b], fill=255,
                                                        width=max(1, int(w))))
        m = m.filter(ImageFilter.GaussianBlur(w * 0.50))
        m = Image.composite(_L(np.asarray(m, np.float32) * (strength / 255.0) * 2.6),
                            empty, mask)
        paint(base, m, light if i % 2 == 0 else dark)


# --------------------------------------------------------------------------
# Hand-drawn path helpers
# --------------------------------------------------------------------------
def densify(pts, closed=True, step=0.010):
    """Resample a path to roughly even spacing so wobble reads smoothly."""
    src = list(pts) + ([pts[0]] if closed else [])
    out = []
    for a, b in zip(src[:-1], src[1:]):
        ln = np.hypot(b[0] - a[0], b[1] - a[1])
        n = max(2, int(ln / step))
        for i in range(n):
            t = i / n
            out.append((a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t))
    if not closed:
        out.append(src[-1])
    return out


def _smooth1d(n, ctrl, seed, loop):
    rng = np.random.RandomState(seed)
    c = rng.uniform(-1.0, 1.0, ctrl + 1)
    if loop:
        c[-1] = c[0]
    x = np.linspace(0, ctrl, n)
    i = np.clip(np.floor(x).astype(int), 0, ctrl - 1)
    t = x - i
    t = t * t * (3 - 2 * t)
    return c[i] * (1 - t) + c[i + 1] * t


def hand(pts, amp=0.006, seed=0, closed=True, ctrl=7, step=0.010):
    """Nudge a path along its normals with smooth low-frequency noise."""
    amp *= WOBBLE
    p = np.array(densify(pts, closed, step), np.float64)
    n = len(p)
    prev = np.roll(p, 1, axis=0) if closed else np.vstack([p[:1], p[:-1]])
    nxt = np.roll(p, -1, axis=0) if closed else np.vstack([p[1:], p[-1:]])
    tan = nxt - prev
    ln = np.hypot(tan[:, 0], tan[:, 1])
    ln[ln == 0] = 1.0
    nx, ny = -tan[:, 1] / ln, tan[:, 0] / ln
    w = _smooth1d(n, ctrl, seed, closed) * amp
    p[:, 0] += nx * w
    p[:, 1] += ny * w
    return [tuple(q) for q in p]


def rr_pts(L, T, R, B, r, steps=10):
    """Rounded-rectangle as a point path."""
    out = []
    corners = [
        (R - r, B - r, 0),
        (L + r, B - r, 90),
        (L + r, T + r, 180),
        (R - r, T + r, 270),
    ]
    for cx, cy, a0 in corners:
        for i in range(steps + 1):
            a = np.deg2rad(a0 + 90 * i / steps)
            out.append((cx + r * np.cos(a), cy + r * np.sin(a)))
    return out


def circle_pts(cx, cy, rx, ry=None, steps=72, start=0.0, sweep=360.0):
    ry = rx if ry is None else ry
    return [
        (cx + rx * np.cos(np.deg2rad(start + sweep * i / steps)),
         cy + ry * np.sin(np.deg2rad(start + sweep * i / steps)))
        for i in range(steps + (0 if sweep == 360.0 else 1))
    ]


def fill_path(img, path, fill, seed=None, cells=24, strength=20):
    m = mask_from(lambda d: d.polygon(poly(path), fill=255))
    paint(img, m, fill)
    if seed is not None:
        texture_pass(img, m, seed=seed, cells=cells, strength=strength)
    return m


def ink(img, path, color=OUTLINE, w=None, closed=True, seed=0, density=0.12):
    """Brush-ish outline: the line mask is modulated so ink density varies."""
    w = LW if w is None else w
    pts = list(path) + ([path[0]] if closed else [])
    m = mask_from(
        lambda d: d.line(poly(pts), fill=255, width=max(1, int(w)), joint="curve")
    )
    n = np.asarray(noise(22, seed + 77, blur=2.0), np.float32) / 255.0
    # keep the line solid; the noise only nudges its weight along the stroke
    a = np.asarray(m, np.float32) * ((1.0 - density) + density * 2.0 * n)
    paint(img, _L(a), color)


def finish(img, name):
    img.resize((OUT, OUT), Image.LANCZOS).save(os.path.join(ICON_DIR, name))
    print("wrote", name)


# --------------------------------------------------------------------------
# Shared document base
# --------------------------------------------------------------------------
PAPER = ((252, 246, 232), (218, 202, 176))
PAPER_L, PAPER_R, PAPER_T, PAPER_B = 0.235, 0.785, 0.095, 0.905
FOLD = 0.185


def paper_sheet(paper=PAPER, seed=3):
    """Cream sheet with a folded corner. Returns (img, finish_outline_fn)."""
    img = new_canvas()
    L, R, T, B = PAPER_L, PAPER_R, PAPER_T, PAPER_B
    body = hand(
        [
            (L, T + 0.035),
            (L + 0.035, T),
            (R - FOLD, T),
            (R, T + FOLD),
            (R, B - 0.035),
            (R - 0.035, B),
            (L + 0.035, B),
            (L, B - 0.035),
        ],
        amp=0.0075,
        seed=seed,
        ctrl=6,
    )
    m = fill_path(img, body, lin_grad(paper[0], paper[1], 78))
    texture_pass(img, m, seed=seed, cells=6, strength=18)
    strokes(img, m, seed=seed + 40, strength=16, count=6, angle=64,
            base_color=(238, 228, 208))
    edge_paint(img, m, light=0.34, shade=0.30, radius=0.048, d=0.060)

    corner = hand(
        [(R - FOLD, T), (R, T + FOLD), (R - FOLD + 0.012, T + FOLD - 0.012)],
        amp=0.005,
        seed=seed + 11,
        ctrl=4,
    )
    # the fold drops a little shade onto the page beneath it
    cm = mask_from(lambda d: d.polygon(poly(corner), fill=255))
    shade(img, m, (128, 104, 92, 255), 0.0, 0.0)  # no-op keeps signature honest
    drop = cm.filter(ImageFilter.GaussianBlur(0.022 * N))
    drop = np.roll(np.roll(np.asarray(drop, np.float32), int(0.018 * N), axis=0),
                   int(-0.012 * N), axis=1)
    drop = Image.composite(_L(drop * 0.30), Image.new("L", (N, N), 0), m)
    paint(img, drop, (128, 100, 88))

    paint(img, cm, lin_grad((236, 224, 202), (188, 170, 143), 40))
    strokes(img, cm, seed=seed + 60, strength=11, count=3, angle=40,
            base_color=(212, 198, 174))
    rim(img, cm, INK_LIGHT, 0.45, 0.02, 0.008, 0.008)

    def close():
        ink(img, body, OUTLINE, LW, seed=seed)
        ink(img, corner, OUTLINE_SOFT, LW * 0.8, seed=seed + 5)

    return img, close


def glyph(img, path, color, seed=None, closed=True, cells=40, strength=24):
    m = fill_path(img, path, color if len(color) == 4 else color + (255,),
                  seed=seed, cells=cells, strength=strength)
    return m


def bar(x0, y0, x1, y1, r=None, seed=0, amp=0.0035):
    r = (y1 - y0) / 2 if r is None else r
    return hand(rr_pts(x0, y0, x1, y1, r), amp=amp, seed=seed, ctrl=5)


# --------------------------------------------------------------------------
# Script / code
# --------------------------------------------------------------------------
def make_script():
    img, close = paper_sheet(seed=3)
    rows = [
        (0.315, 0.30, (108, 162, 156)),
        (0.375, 0.22, (214, 138, 108)),
        (0.355, 0.16, (126, 158, 194)),
        (0.315, 0.26, (108, 162, 156)),
        (0.375, 0.18, (166, 154, 176)),
    ]
    y = 0.355
    for i, (x0, w, col) in enumerate(rows):
        p = bar(x0, y, x0 + w, y + 0.050, seed=i * 13 + 1)
        m = glyph(img, p, col, seed=i * 9, cells=40, strength=28)
        rim(img, m, INK_LIGHT, 0.35, 0.012, 0.005, 0.005)
        y += 0.084
    close()
    finish(img, "icon_script.png")


# --------------------------------------------------------------------------
# Model / mesh (shared cube construction)
# --------------------------------------------------------------------------
CUBE_T = (0.50, 0.105)
CUBE_RT = (0.860, 0.300)
CUBE_RB = (0.860, 0.700)
CUBE_B = (0.50, 0.895)
CUBE_LB = (0.140, 0.700)
CUBE_LT = (0.140, 0.300)
CUBE_C = (0.50, 0.500)


def cube_paths(seed=0, amp=0.006):
    """Wobbled cube edges, shared between faces so seams never crack."""
    def e(a, b, s):
        return hand([a, b], amp=amp, seed=seed + s, closed=False, ctrl=3)

    top_r = e(CUBE_T, CUBE_RT, 1)
    right_e = e(CUBE_RT, CUBE_RB, 2)
    bot_r = e(CUBE_RB, CUBE_B, 3)
    bot_l = e(CUBE_B, CUBE_LB, 4)
    left_e = e(CUBE_LB, CUBE_LT, 5)
    top_l = e(CUBE_LT, CUBE_T, 6)
    spoke_l = e(CUBE_LT, CUBE_C, 7)
    spoke_r = e(CUBE_RT, CUBE_C, 8)
    spoke_b = e(CUBE_B, CUBE_C, 9)

    # each face walks its boundary in order, reusing the shared edge polylines
    top = top_r + spoke_r + spoke_l[::-1] + top_l
    left = spoke_l + spoke_b[::-1] + bot_l + left_e
    right = spoke_r + spoke_b[::-1] + bot_r[::-1] + right_e[::-1]
    silhouette = top_r + right_e[1:] + bot_r[1:] + bot_l[1:] + left_e[1:] + top_l[1:]
    return top, left, right, silhouette, (spoke_l, spoke_r, spoke_b)


def draw_cube(img, top_c, left_c, right_c, seed=0):
    top, left, right, sil, spokes = cube_paths(seed=seed)
    masks = []
    for pts, (c0, c1), ang, s in (
        (top, top_c, 100, 0),
        (left, left_c, 70, 1),
        (right, right_c, 60, 2),
    ):
        m = fill_path(img, pts, lin_grad(c0, c1), seed=seed + s * 7, cells=22,
                      strength=22)
        masks.append(m)
    # painted light pooling near the top edge of each face
    rim(img, masks[0], INK_LIGHT, 0.36, 0.018, 0.045, 0.050)
    rim(img, masks[1], INK_LIGHT, 0.22, 0.016, 0.042, 0.046)
    rim(img, masks[2], INK_SHADE, 0.30, 0.018, -0.042, -0.048)
    for m in masks:
        rim(img, m, INK_SHADE, 0.16, 0.008, 0.0, 0.0)
    for i, m in enumerate(masks):
        c0, c1 = (top_c, left_c, right_c)[i]
        mid = tuple((a + b) // 2 for a, b in zip(c0, c1))
        strokes(img, m, seed=seed + 31 + i * 5, strength=15, count=4,
                angle=(20, 64, 116)[i], base_color=mid)
    return sil, spokes, masks


def ink_cube(img, sil, spokes, seed=0):
    ink(img, sil, OUTLINE, LW, seed=seed)
    for i, sp in enumerate(spokes):
        ink(img, sp, OUTLINE_SOFT, LW * 0.85, closed=False, seed=seed + i * 3)


def make_model():
    img = new_canvas()
    sil, spokes, _ = draw_cube(
        img,
        ((196, 226, 176), (146, 198, 148)),
        ((112, 164, 128), (70, 120, 100)),
        ((76, 124, 112), (46, 88, 84)),
        seed=17,
    )
    ink_cube(img, sil, spokes, seed=17)
    finish(img, "icon_model.png")


def make_prefab():
    img = new_canvas()
    sil, spokes, _ = draw_cube(
        img,
        ((202, 222, 242), (164, 192, 222)),
        ((152, 178, 212), (116, 146, 186)),
        ((124, 152, 192), (86, 114, 156)),
        seed=29,
    )
    for p in (CUBE_T, CUBE_RT, CUBE_RB, CUBE_B, CUBE_LB, CUBE_LT):
        node = hand(circle_pts(p[0], p[1], 0.034, steps=26), amp=0.004,
                    seed=int(p[0] * 400 + p[1] * 90))
        glyph(img, node, (250, 248, 240))
        ink(img, node, OUTLINE_SOFT, LW * 0.6, seed=int(p[1] * 300))
    ink_cube(img, sil, spokes, seed=29)
    finish(img, "icon_prefab.png")


def make_cubemap():
    img = new_canvas()
    sil, spokes, masks = draw_cube(
        img,
        ((200, 228, 242), (158, 200, 228)),
        ((152, 192, 222), (110, 152, 192)),
        ((124, 164, 202), (84, 122, 166)),
        seed=41,
    )
    top_m, left_m, right_m = masks
    sun = mask_from(lambda d: d.polygon(
        poly(hand(circle_pts(0.50, 0.295, 0.062, steps=30), amp=0.004, seed=5)),
        fill=255))
    paint(img, Image.composite(sun, Image.new("L", (N, N), 0), top_m),
          (252, 234, 172))
    for face, clouds in (
        (left_m, [(0.250, 0.545, 0.078), (0.320, 0.568, 0.056), (0.285, 0.685, 0.052)]),
        (right_m, [(0.660, 0.600, 0.068), (0.725, 0.622, 0.050)]),
    ):
        for cx, cy, r in clouds:
            c = hand(circle_pts(cx, cy, r, r * 0.72, steps=30), amp=0.005,
                     seed=int(cx * 700 + cy * 130))
            m = mask_from(lambda d, p=c: d.polygon(poly(p), fill=255))
            paint(img, Image.composite(m, Image.new("L", (N, N), 0), face),
                  (247, 250, 252, 238))
    ink_cube(img, sil, spokes, seed=41)
    finish(img, "icon_cubemap.png")


# --------------------------------------------------------------------------
# Texture
# --------------------------------------------------------------------------
def make_texture():
    img = new_canvas()
    L, T, R, B = 0.125, 0.145, 0.875, 0.855
    frame_p = hand(rr_pts(L, T, R, B, 0.085), amp=0.006, seed=51, ctrl=8)
    frame = fill_path(img, frame_p, lin_grad((246, 238, 222), (222, 210, 190), 90))

    cells = 4
    cw, ch = (R - L) / cells, (B - T) / cells
    empty = Image.new("L", (N, N), 0)
    for iy in range(cells):
        for ix in range(cells):
            x0, y0 = L + ix * cw, T + iy * ch
            p = hand(
                [(x0, y0), (x0 + cw, y0), (x0 + cw, y0 + ch), (x0, y0 + ch)],
                amp=0.005, seed=ix * 31 + iy * 7, ctrl=4,
            )
            m = mask_from(lambda d, q=p: d.polygon(poly(q), fill=255))
            m = Image.composite(m, empty, frame)
            if (ix + iy) % 2:
                paint(img, m, lin_grad((128, 160, 178), (96, 128, 150), 70))
                texture_pass(img, m, seed=ix * 13 + iy, cells=18, strength=24)
            else:
                paint(img, m, lin_grad((246, 239, 224), (226, 214, 192), 80))
                texture_pass(img, m, seed=ix * 5 + iy * 3, cells=20, strength=16)

    strokes(img, frame, seed=77, strength=10, count=5, angle=32,
            base_color=(198, 200, 198))
    edge_paint(img, frame, light=0.30, shade=0.28, radius=0.050, d=0.062)
    ink(img, frame_p, OUTLINE, LW, seed=51)
    finish(img, "icon_texture.png")


# --------------------------------------------------------------------------
# Material
# --------------------------------------------------------------------------
def make_material():
    img = new_canvas()
    cx, cy, r = 0.50, 0.50, 0.378
    ball_p = hand(circle_pts(cx, cy, r, steps=88), amp=0.0055, seed=63, ctrl=6)
    sphere = fill_path(img, ball_p, lin_grad((240, 180, 118), (142, 86, 82), 62))

    # painted variation: broad tonal drift plus curved brush loads
    texture_pass(img, sphere, seed=11, cells=5, strength=30, dark=(112, 62, 62))
    strokes(img, sphere, seed=27, strength=20, count=8, angle=58,
            base_color=(196, 134, 100))

    shade(img, sphere, (78, 44, 52, 255), 0.55, 0.09, offset=(0.10 * N, 0.09 * N))
    rim(img, sphere, (250, 214, 168), 0.50, 0.014, -0.026, -0.024)
    rim(img, sphere, (126, 74, 78), 0.20, 0.008, 0.0, 0.0)

    spec = mask_from(lambda d: d.polygon(
        poly(hand(circle_pts(0.395, 0.315, 0.098, 0.076, steps=40), amp=0.006,
                  seed=71)), fill=255))
    spec = Image.composite(spec.filter(ImageFilter.GaussianBlur(0.013 * N)),
                           Image.new("L", (N, N), 0), sphere)
    paint(img, _L(np.asarray(spec, np.float32) * 0.80), (255, 248, 232))
    glint = mask_from(lambda d: d.polygon(
        poly(hand(circle_pts(0.372, 0.298, 0.040, 0.030, steps=28), amp=0.005,
                  seed=73)), fill=255))
    paint(img, glint.filter(ImageFilter.GaussianBlur(0.004 * N)), (255, 253, 246))

    ink(img, ball_p, OUTLINE, LW, seed=63)
    finish(img, "icon_material.png")


# --------------------------------------------------------------------------
# Document-family glyphs
# --------------------------------------------------------------------------
def make_text():
    img, close = paper_sheet(seed=7)
    y = 0.335
    for i, w in enumerate((0.42, 0.42, 0.36, 0.42, 0.30)):
        m = glyph(img, bar(0.305, y, 0.305 + w, y + 0.040, seed=i * 17), (152, 146, 152))
        rim(img, m, INK_LIGHT, 0.30, 0.010, 0.004, 0.004)
        y += 0.073
    close()
    finish(img, "icon_text.png")


def make_config():
    img, close = paper_sheet(seed=11)
    cx, cy, teeth, r_out, r_in = 0.512, 0.560, 8, 0.205, 0.152
    pts = []
    for i in range(teeth):
        a0 = 2 * np.pi * i / teeth
        span = 2 * np.pi / teeth
        for a, rr in ((0.10, r_in), (0.23, r_out), (0.52, r_out), (0.65, r_in)):
            ang = a0 + span * a
            pts.append((cx + rr * np.cos(ang), cy + rr * np.sin(ang)))
    gear = hand(pts, amp=0.004, seed=23, ctrl=9, step=0.008)
    m = glyph(img, gear, (128, 148, 168), seed=17, cells=26, strength=22)
    rim(img, m, INK_LIGHT, 0.40, 0.016, 0.007, 0.007)
    rim(img, m, INK_SHADE, 0.30, 0.016, -0.007, -0.007)

    hub = hand(circle_pts(cx, cy, 0.064, steps=34), amp=0.004, seed=31)
    paint(img, mask_from(lambda d: d.polygon(poly(hub), fill=255)),
          lin_grad((250, 245, 234), (226, 215, 196), 78))
    ink(img, hub, OUTLINE_SOFT, LW * 0.7, seed=31)
    ink(img, gear, OUTLINE_SOFT, LW * 0.62, seed=23, density=0.7)
    close()
    finish(img, "icon_config.png")


def make_data():
    img, close = paper_sheet(seed=13)
    L, R, T, B = 0.300, 0.720, 0.330, 0.745
    rows, cols = 3, 3
    head = 0.098
    rh = (B - T - head) / rows
    cw = (R - L) / cols

    table = hand([(L, T), (R, T), (R, B), (L, B)], amp=0.005, seed=37, ctrl=5)
    fill_path(img, table, (250, 246, 236, 255))
    hdr = hand([(L, T), (R, T), (R, T + head), (L, T + head)], amp=0.004, seed=39,
               ctrl=4)
    m = glyph(img, hdr, (104, 158, 152), seed=21, cells=30, strength=22)
    rim(img, m, INK_LIGHT, 0.32, 0.012, 0.005, 0.005)

    for r in range(rows):
        for c in range(cols):
            x0, y0 = L + c * cw, T + head + r * rh
            glyph(img, bar(x0 + 0.024, y0 + 0.030, x0 + cw - 0.024, y0 + rh - 0.030,
                           r=0.010, seed=r * 7 + c * 3), (198, 191, 178))

    def grid(d):
        for c in range(1, cols):
            d.line(poly(hand([(L + c * cw, T + head), (L + c * cw, B)], amp=0.003,
                             seed=c * 5, closed=False, ctrl=3)),
                   fill=OUTLINE_SOFT, width=max(1, int(LW * 0.5)), joint="curve")
        for r in range(1, rows):
            d.line(poly(hand([(L, T + head + r * rh), (R, T + head + r * rh)],
                             amp=0.003, seed=r * 9, closed=False, ctrl=3)),
                   fill=OUTLINE_SOFT, width=max(1, int(LW * 0.5)), joint="curve")

    img.alpha_composite(_draw_layer(grid))
    ink(img, table, OUTLINE_SOFT, LW * 0.85, seed=37)
    close()
    finish(img, "icon_data.png")


def _draw_layer(fn):
    layer = new_canvas()
    fn(ImageDraw.Draw(layer))
    return layer


def make_font():
    img, close = paper_sheet(seed=17)
    stroke = int(0.058 * N)
    ink_col = (90, 78, 86, 255)

    def draw(d):
        d.line(poly(hand([(0.365, 0.745), (0.512, 0.325)], amp=0.005, seed=43,
                         closed=False, ctrl=3)), fill=ink_col, width=stroke,
               joint="curve")
        d.line(poly(hand([(0.512, 0.325), (0.658, 0.745)], amp=0.005, seed=47,
                         closed=False, ctrl=3)), fill=ink_col, width=stroke,
               joint="curve")
        d.line(poly(hand([(0.420, 0.618), (0.600, 0.618)], amp=0.004, seed=53,
                         closed=False, ctrl=3)), fill=ink_col,
               width=int(stroke * 0.78), joint="curve")

    layer = _draw_layer(draw)
    n = np.asarray(noise(40, 91, blur=0.8), np.float32) / 255.0
    a = np.asarray(layer.split()[3], np.float32) * (0.72 + 0.5 * n)
    layer.putalpha(_L(a))
    img.alpha_composite(layer)
    close()
    finish(img, "icon_font.png")


def make_audio():
    img, close = paper_sheet(seed=19)
    for i, (x, h) in enumerate(((0.300, 0.070), (0.352, 0.130), (0.404, 0.092))):
        glyph(img, bar(x, 0.555 - h, x + 0.030, 0.555 + h, r=0.015, seed=i * 11),
              (196, 205, 201))
    head = hand(circle_pts(0.507, 0.663, 0.072, 0.058, steps=40), amp=0.005, seed=57)
    m = glyph(img, head, (108, 152, 168), seed=33, cells=30, strength=22)
    rim(img, m, INK_LIGHT, 0.38, 0.014, 0.006, 0.006)
    stem = hand([(0.567, 0.660), (0.572, 0.318)], amp=0.004, seed=59, closed=False,
                ctrl=3)
    img.alpha_composite(_draw_layer(lambda d: d.line(
        poly(stem), fill=(108, 152, 168, 255), width=int(0.042 * N), joint="curve")))
    flag = hand([(0.567, 0.312), (0.706, 0.372), (0.706, 0.462), (0.567, 0.402)],
                amp=0.005, seed=61, ctrl=4)
    glyph(img, flag, (108, 152, 168), seed=35, cells=30, strength=20)
    ink(img, head, OUTLINE_SOFT, LW * 0.55, seed=57, density=0.75)
    close()
    finish(img, "icon_audio.png")


def make_video():
    img, close = paper_sheet(seed=23)
    disc = hand(circle_pts(0.510, 0.560, 0.196, steps=64), amp=0.005, seed=67)
    m = glyph(img, disc, (198, 122, 106), seed=45, cells=26, strength=24)
    rim(img, m, INK_LIGHT, 0.40, 0.022, 0.010, 0.010)
    rim(img, m, INK_SHADE, 0.24, 0.020, -0.010, -0.010)
    tri = hand([(0.455, 0.455), (0.610, 0.562), (0.455, 0.668)], amp=0.004, seed=69,
               ctrl=4)
    glyph(img, tri, (252, 246, 234))
    ink(img, disc, OUTLINE_SOFT, LW * 0.8, seed=67)
    close()
    finish(img, "icon_video.png")


def make_shader():
    img, close = paper_sheet(seed=29)
    tri = hand([(0.512, 0.290), (0.742, 0.725), (0.282, 0.725)], amp=0.006, seed=73,
               ctrl=5)
    m = fill_path(img, tri, lin_grad((110, 204, 198), (198, 150, 210), 58))
    texture_pass(img, m, seed=57, cells=24, strength=12)
    rim(img, m, INK_LIGHT, 0.35, 0.024, 0.010, 0.010)
    ink(img, tri, OUTLINE_SOFT, LW * 0.85, seed=73)
    for i, p in enumerate(((0.512, 0.290), (0.742, 0.725), (0.282, 0.725))):
        node = hand(circle_pts(p[0], p[1], 0.036, steps=26), amp=0.004, seed=79 + i)
        glyph(img, node, (250, 245, 234))
        ink(img, node, OUTLINE_SOFT, LW * 0.6, seed=83 + i)
    close()
    finish(img, "icon_shader.png")


def make_animation():
    img, close = paper_sheet(seed=31)
    glyph(img, bar(0.295, 0.528, 0.725, 0.572, r=0.022, seed=87), (198, 190, 178))
    for i, x in enumerate((0.345, 0.512, 0.678)):
        r = 0.090 if i == 1 else 0.074
        dia = hand([(x, 0.550 - r), (x + r * 0.82, 0.550), (x, 0.550 + r),
                    (x - r * 0.82, 0.550)], amp=0.004, seed=89 + i * 5, ctrl=4)
        col = (216, 144, 114) if i == 1 else (106, 156, 152)
        m = glyph(img, dia, col, seed=i * 9)
        rim(img, m, INK_LIGHT, 0.34, 0.012, 0.005, 0.005)
        ink(img, dia, OUTLINE_SOFT, LW * 0.7, seed=97 + i)
    arc = hand(circle_pts(0.510, 0.545, 0.198, 0.152, steps=40, start=196, sweep=148),
               amp=0.005, seed=101, closed=False, ctrl=4)
    ink(img, arc, (150, 142, 150, 235), LW * 1.05, closed=False, seed=101, density=0.6)
    close()
    finish(img, "icon_animation.png")


def make_unknown():
    img, close = paper_sheet(seed=37)
    cx, cy, r = 0.512, 0.418, 0.116
    hook = [(cx + r * np.cos(a), cy + r * np.sin(a))
            for a in np.linspace(np.deg2rad(200), np.deg2rad(385), 18)]
    hook += [(0.548, 0.548), (0.512, 0.590), (0.512, 0.648)]
    hook = hand(hook, amp=0.004, seed=103, closed=False, ctrl=4)
    ink(img, hook, (146, 138, 146, 255), 0.060 * N, closed=False, seed=103,
        density=0.45)
    dot = hand(circle_pts(0.512, 0.733, 0.033, steps=24), amp=0.004, seed=107)
    glyph(img, dot, (146, 138, 146))
    close()
    finish(img, "icon_unknown.png")


# --------------------------------------------------------------------------
# Object-family icons
# --------------------------------------------------------------------------
def make_folder():
    img = new_canvas()
    L, R, B = 0.105, 0.895, 0.805
    back = hand(
        [(L, 0.300), (L + 0.020, 0.205), (0.390, 0.205), (0.455, 0.300),
         (R, 0.300), (R, B), (L, B)],
        amp=0.006, seed=113, ctrl=7,
    )
    bm = fill_path(img, back, lin_grad((220, 170, 102), (176, 128, 74), 80),
                   seed=63, cells=20, strength=22)
    rim(img, bm, INK_LIGHT, 0.30, 0.018, 0.050, 0.050)
    rim(img, bm, INK_SHADE, 0.18, 0.009, 0.0, 0.0)
    ink(img, back, OUTLINE, LW, seed=113)

    front = hand(rr_pts(L, 0.360, R, B, 0.062), amp=0.006, seed=117, ctrl=7)
    fm = fill_path(img, front, lin_grad((250, 206, 128), (206, 150, 82), 82),
                   seed=71, cells=24, strength=20)
    strokes(img, fm, seed=121, strength=18, count=6, angle=28,
           base_color=(228, 182, 112))
    edge_paint(img, fm, light=0.38, shade=0.34, radius=0.050, d=0.062)
    ink(img, front, OUTLINE, LW, seed=117)
    finish(img, "icon_folder.png")


def make_folder_open():
    img = new_canvas()
    L, R = 0.105, 0.860
    back = hand(
        [(L, 0.285), (L + 0.018, 0.190), (0.375, 0.190), (0.440, 0.285),
         (R, 0.285), (R, 0.700), (L, 0.700)],
        amp=0.006, seed=127, ctrl=7,
    )
    bm = fill_path(img, back, lin_grad((214, 162, 96), (172, 124, 70), 80),
                   seed=63, cells=20, strength=22)
    rim(img, bm, INK_LIGHT, 0.26, 0.018, 0.050, 0.050)
    rim(img, bm, INK_SHADE, 0.18, 0.009, 0.0, 0.0)
    # ink the back panel BEFORE the flap so the flap paints over the hidden lines
    ink(img, back, OUTLINE, LW, seed=127)

    flap = hand([(0.048, 0.815), (0.232, 0.398), (0.952, 0.398), (0.796, 0.815)],
                amp=0.006, seed=131, ctrl=6)
    fm = fill_path(img, flap, lin_grad((251, 208, 134), (204, 148, 80), 84),
                   seed=71, cells=24, strength=20)
    strokes(img, fm, seed=133, strength=18, count=6, angle=28,
           base_color=(228, 180, 110))
    edge_paint(img, fm, light=0.38, shade=0.30, radius=0.050, d=0.062)
    ink(img, flap, OUTLINE, LW, seed=131)
    finish(img, "icon_folder_open.png")


def make_archive():
    img = new_canvas()
    L, T, R, B = 0.145, 0.165, 0.855, 0.865
    box_p = hand(rr_pts(L, T, R, B, 0.058), amp=0.006, seed=137, ctrl=8)
    box = fill_path(img, box_p, lin_grad((216, 176, 128), (164, 126, 90), 82),
                    seed=81, cells=18, strength=26)
    strokes(img, box, seed=139, strength=18, count=6, angle=30,
            base_color=(190, 150, 108))

    lid_p = hand([(L, T), (R, T), (R, 0.345), (L, 0.345)], amp=0.005, seed=141, ctrl=5)
    lid = Image.composite(mask_from(lambda d: d.polygon(poly(lid_p), fill=255)),
                          Image.new("L", (N, N), 0), box)
    paint(img, lid, lin_grad((234, 200, 154), (198, 160, 116), 80))
    strokes(img, lid, seed=143, strength=12, count=3, angle=20,
            base_color=(216, 180, 134))

    track = hand(rr_pts(0.452, 0.345, 0.548, 0.720, 0.030), amp=0.004, seed=147, ctrl=6)
    glyph(img, track, (150, 158, 166))
    for i in range(6):
        y = 0.368 + i * 0.058
        glyph(img, hand([(0.460, y), (0.540, y), (0.540, y + 0.026), (0.460, y + 0.026)],
                        amp=0.003, seed=151 + i, ctrl=3), (208, 214, 216))
    pull = hand(rr_pts(0.443, 0.678, 0.557, 0.782, 0.026), amp=0.004, seed=157, ctrl=6)
    m = glyph(img, pull, (172, 180, 186))
    rim(img, m, INK_LIGHT, 0.40, 0.012, 0.006, 0.006)
    tab = hand(circle_pts(0.500, 0.800, 0.036, 0.040, steps=28), amp=0.004, seed=159)
    glyph(img, tab, (172, 180, 186))
    glyph(img, hand(circle_pts(0.500, 0.800, 0.015, steps=20), amp=0.003, seed=163),
          (168, 130, 94))

    edge_paint(img, box, light=0.34, shade=0.32, radius=0.050, d=0.060)
    ink(img, box_p, OUTLINE, LW, seed=137)
    ink(img, hand([(L, 0.345), (R, 0.345)], amp=0.004, seed=167, closed=False, ctrl=3),
        OUTLINE_SOFT, LW * 0.7, closed=False, seed=167)
    ink(img, pull, OUTLINE_SOFT, LW * 0.55, seed=157, density=0.7)
    finish(img, "icon_archive.png")


def make_scene():
    img = new_canvas()
    L, T, R, B = 0.100, 0.180, 0.900, 0.835
    card_p = hand(rr_pts(L, T, R, B, 0.080), amp=0.006, seed=173, ctrl=8)
    card = fill_path(img, card_p, lin_grad((188, 220, 234), (242, 228, 202), 90))
    empty = Image.new("L", (N, N), 0)

    def clipped(path, fill, seed=None):
        m = Image.composite(mask_from(lambda d: d.polygon(poly(path), fill=255)),
                            empty, card)
        paint(img, m, fill)
        if seed is not None:
            texture_pass(img, m, seed=seed, cells=22, strength=18)
        return m

    sun = clipped(hand(circle_pts(0.680, 0.318, 0.078, steps=34), amp=0.005, seed=177),
                  (250, 226, 168, 255))
    rim(img, sun, (255, 248, 214), 0.5, 0.014, 0.006, 0.006)
    clipped(hand(circle_pts(0.320, 0.800, 0.300, 0.300, steps=60), amp=0.008, seed=181),
            lin_grad((152, 198, 148), (110, 158, 116), 70), seed=91)
    clipped(hand(circle_pts(0.700, 0.870, 0.320, 0.300, steps=60), amp=0.008, seed=185),
            lin_grad((126, 174, 130), (86, 132, 100), 70), seed=93)
    clipped(hand([(0.296, 0.610), (0.328, 0.610), (0.330, 0.730), (0.294, 0.730)],
                 amp=0.004, seed=189, ctrl=3), (128, 100, 78, 255))
    crown = clipped(hand(circle_pts(0.312, 0.560, 0.072, 0.078, steps=36), amp=0.008,
                         seed=193), (98, 148, 112, 255))
    rim(img, crown, (168, 208, 150), 0.45, 0.016, 0.008, 0.008)

    strokes(img, card, seed=197, strength=10, count=5, angle=18,
            base_color=(196, 206, 196))
    rim(img, card, INK_LIGHT, 0.24, 0.018, 0.048, 0.048)
    rim(img, card, INK_SHADE, 0.16, 0.009, 0.0, 0.0)
    ink(img, card_p, OUTLINE, LW, seed=173)
    finish(img, "icon_scene.png")


def make_skeleton():
    img = new_canvas()
    p0, p1 = (0.285, 0.720), (0.715, 0.280)
    w, r = 0.052, 0.076
    dx, dy = p1[0] - p0[0], p1[1] - p0[1]
    ln = np.hypot(dx, dy)
    ux, uy = dx / ln, dy / ln
    nx, ny = -uy, ux

    shaft = hand([(p0[0] + nx * w, p0[1] + ny * w), (p1[0] + nx * w, p1[1] + ny * w),
                  (p1[0] - nx * w, p1[1] - ny * w), (p0[0] - nx * w, p0[1] - ny * w)],
                 amp=0.005, seed=199, ctrl=5)
    lobes = []
    for p, s in ((p0, -1), (p1, 1)):
        for side in (-1, 1):
            cx = p[0] + s * ux * r * 0.30 + side * nx * r * 0.78
            cy = p[1] + s * uy * r * 0.30 + side * ny * r * 0.78
            lobes.append(hand(circle_pts(cx, cy, r, steps=34), amp=0.005,
                              seed=int(cx * 900 + cy * 130)))

    body = mask_from(lambda d: (
        d.polygon(poly(shaft), fill=255),
        [d.polygon(poly(l), fill=255) for l in lobes],
    ))
    paint(img, body, lin_grad((250, 246, 236), (222, 213, 198), 66))
    strokes(img, body, seed=211, strength=14, count=4, angle=-45,
            base_color=(236, 230, 216))
    edge_paint(img, body, light=0.45, shade=0.38, radius=0.035, d=0.040)

    # single silhouette outline: ink the body edge, not each sub-shape
    b = np.asarray(body, np.float32)
    er = np.asarray(body.filter(ImageFilter.GaussianBlur(0.006 * N)), np.float32)
    edge = _L((b / 255.0) * np.clip(1.0 - er / 255.0, 0, 1) * 3.2 * 255.0)
    edge = edge.filter(ImageFilter.GaussianBlur(0.0016 * N))
    n = np.asarray(noise(34, 217, blur=1.0), np.float32) / 255.0
    paint(img, _L(np.asarray(edge, np.float32) * (0.55 + 0.9 * n)), OUTLINE)
    finish(img, "icon_skeleton.png")


def make_particle():
    img = new_canvas()
    core_p = hand(circle_pts(0.500, 0.500, 0.132, steps=48), amp=0.006, seed=223)
    core = mask_from(lambda d: d.polygon(poly(core_p), fill=255))
    halo = _L(np.asarray(core.filter(ImageFilter.GaussianBlur(0.062 * N)),
                         np.float32) * 0.68)
    paint(img, halo, (250, 224, 158))
    paint(img, core, lin_grad((255, 246, 214), (244, 196, 126), 60))
    rim(img, core, (255, 252, 236), 0.55, 0.018, 0.008, 0.008)
    ink(img, core_p, OUTLINE_SOFT, LW * 0.8, seed=223)

    sparks = [
        (0.238, 0.272, 0.058, (232, 168, 122)),
        (0.736, 0.246, 0.046, (240, 206, 140)),
        (0.774, 0.664, 0.052, (198, 176, 216)),
        (0.576, 0.796, 0.044, (232, 168, 122)),
        (0.266, 0.734, 0.038, (150, 196, 206)),
        (0.186, 0.506, 0.032, (240, 206, 140)),
        (0.506, 0.186, 0.032, (198, 176, 216)),
    ]
    for i, (cx, cy, r, col) in enumerate(sparks):
        p = hand(circle_pts(cx, cy, r, steps=30), amp=0.005, seed=227 + i * 7)
        m = mask_from(lambda d, q=p: d.polygon(poly(q), fill=255))
        soft = _L(np.asarray(m.filter(ImageFilter.GaussianBlur(0.020 * N)),
                             np.float32) * 0.45)
        paint(img, soft, col)
        paint(img, m, col)
        rim(img, m, INK_LIGHT, 0.40, 0.010, 0.004, 0.004)
        ink(img, p, OUTLINE_SOFT, LW * 0.6, seed=233 + i)
    finish(img, "icon_particle.png")


if __name__ == "__main__":
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    ICON_DIR = os.path.join(root, "assets", "editor", "icons")
    os.makedirs(ICON_DIR, exist_ok=True)
    for fn in (
        make_script, make_model, make_texture, make_material,
        make_text, make_config, make_data, make_font, make_audio, make_video,
        make_shader, make_animation, make_unknown,
        make_folder, make_folder_open, make_archive, make_scene, make_prefab,
        make_cubemap, make_skeleton, make_particle,
    ):
        fn()
