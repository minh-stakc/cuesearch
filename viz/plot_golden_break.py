#!/usr/bin/env python3
"""Render the golden-break search as a 4-panel figure for the README.

Reads:
    docs/golden_grid.csv     full grid scores from `build/golden_break`
    docs/golden_best.txt     the winning parameters (best-in-grid)

Writes:
    docs/golden_break.png    4-panel figure: (1) cueX-cueZ heatmap at
                             optimal aim/speed/spin, (2) speed-aimDz
                             heatmap at optimal cue position/spin,
                             (3) overhead table cartoon with the
                             optimal cue/rack/aim drawn, (4) tip
                             diagram showing the optimal (a, b) hit
                             with the miscue circle.
"""
import csv
import sys
from pathlib import Path


def load_grid(path):
    rows = []
    with open(path) as f:
        for r in csv.DictReader(f):
            for k in r:
                r[k] = float(r[k]) if k not in ("trials", "legal", "gold") else int(r[k])
            r["pGold"] = r["gold"] / r["trials"] if r["trials"] else 0.0
            r["pLeg"]  = r["legal"] / r["trials"] if r["trials"] else 0.0
            rows.append(r)
    return rows


def load_best(path):
    out = {}
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            k, v = line.split()
            out[k] = float(v)
    return out


def slice_at(rows, fixed, tol=1e-5):
    """rows where every (k -> v) in fixed matches within tol."""
    return [r for r in rows
            if all(abs(r[k] - v) <= tol for k, v in fixed.items())]


def heatmap_2d(ax, rows, xkey, ykey, vkey, title, xlabel, ylabel,
               star=None, xfmt=None, yfmt=None, vmax=None):
    """Heatmap on the (xkey, ykey) marginal of `rows`. xfmt/yfmt are
    optional (value -> string) functions for tick labels (e.g. divide
    by R to express in ball radii). `star` is plotted in DATA coords."""
    import numpy as np
    xs = sorted({r[xkey] for r in rows})
    ys = sorted({r[ykey] for r in rows})
    Z = np.full((len(ys), len(xs)), float("nan"))
    for r in rows:
        Z[ys.index(r[ykey]), xs.index(r[xkey])] = r[vkey]
    im = ax.imshow(
        Z * 100, origin="lower", aspect="auto",
        extent=[xs[0], xs[-1], ys[0], ys[-1]],
        cmap="viridis", vmin=0, vmax=vmax)
    ax.set_xticks(xs)
    ax.set_yticks(ys)
    ax.set_xticklabels([(xfmt or "{:.2f}".format)(x) for x in xs],
                       fontsize=7)
    ax.set_yticklabels([(yfmt or "{:.2f}".format)(y) for y in ys],
                       fontsize=7)
    ax.set_xlabel(xlabel)
    ax.set_ylabel(ylabel)
    ax.set_title(title, fontsize=9)
    if star is not None:
        ax.plot([star[0]], [star[1]], marker="*",
                color="white", markersize=14, markeredgecolor="black",
                markeredgewidth=1.0, zorder=5)
    return im


def draw_table(ax, best, rows, table_xMax=2.54, table_zMax=1.27,
               R=0.028575, vmax_pct=None):
    """Top-down table cartoon with the cueX-cueZ P(gold) heatmap
    overlaid on the kitchen region. The heatmap directly answers the
    question 'where should I put the cue ball?' in table coordinates."""
    import numpy as np
    head_string_x = 0.25 * table_xMax
    foot_x = 0.75 * table_xMax

    # Felt + border
    ax.add_patch(plt.Rectangle((-0.05, -0.05),
                               table_xMax + 0.10, table_zMax + 0.10,
                               facecolor="#5a3a1d", zorder=0))
    ax.add_patch(plt.Rectangle((0, 0), table_xMax, table_zMax,
                               facecolor="#0a6b3b", zorder=1))

    # Heatmap overlay in the kitchen: P(gold) at the optimal aim/speed/spin
    # as a function of where you place the cue.
    fixed = {k: best[k] for k in ("aimDz", "speed", "a", "b")}
    s = [r for r in rows
         if all(abs(r[k] - v) <= 1e-5 for k, v in fixed.items())]
    xs = sorted({r["cueX"] for r in s})
    zs = sorted({r["cueZ"] for r in s})
    Z = np.full((len(zs), len(xs)), float("nan"))
    for r in s:
        Z[zs.index(r["cueZ"]), xs.index(r["cueX"])] = r["pGold"] * 100
    if len(xs) >= 2 and len(zs) >= 2:
        ax.imshow(
            Z, origin="lower", aspect="auto",
            extent=[xs[0], xs[-1], zs[0], zs[-1]],
            cmap="viridis", alpha=0.75, vmin=0, vmax=vmax_pct, zorder=2)
    # Pockets
    pr = 2.0 * R
    for px in (0, table_xMax / 2, table_xMax):
        for pz in (0, table_zMax):
            if px == table_xMax / 2 and pz not in (0, table_zMax):
                continue
            ax.add_patch(plt.Circle((px, pz), pr, color="black", zorder=2))

    # Diamonds along long rails
    for i in range(1, 8):
        for z in (-0.02, table_zMax + 0.02):
            ax.plot([i * table_xMax / 8], [z], marker="D",
                    color="#dcc56b", markersize=3, zorder=2)

    # Head string (dashed)
    ax.plot([head_string_x, head_string_x], [0, table_zMax],
            color="white", linestyle="--", linewidth=0.9, alpha=0.7,
            zorder=2)
    ax.text(head_string_x - 0.02, table_zMax - 0.07, "head string",
            color="white", fontsize=6, rotation=90, va="top", ha="right",
            alpha=0.8)

    # Kitchen shading
    ax.add_patch(plt.Rectangle((0, 0), head_string_x, table_zMax,
                               facecolor="white", alpha=0.06, zorder=2))

    # Rack diamond at foot spot (tight, 1 at apex toward breaker)
    s = 2.0 * R * 1.015
    pitch = s * 0.86602540378
    z0 = 0.5 * table_zMax
    slots = [
        (foot_x, z0),                                # 1 apex
        (foot_x + pitch, z0 - s / 2),
        (foot_x + pitch, z0 + s / 2),
        (foot_x + 2 * pitch, z0 - s),
        (foot_x + 2 * pitch, z0),                    # 9 centre
        (foot_x + 2 * pitch, z0 + s),
        (foot_x + 3 * pitch, z0 - s / 2),
        (foot_x + 3 * pitch, z0 + s / 2),
        (foot_x + 4 * pitch, z0),
    ]
    ids = [1, 2, 3, 4, 9, 5, 6, 7, 8]
    PAL = {1: "#f5c518", 2: "#2b6cb0", 3: "#c1272d", 4: "#6b3fa0",
           5: "#e76f00", 6: "#1f7a3a", 7: "#7a1d1d", 8: "#1a1a1a",
           9: "#f5c518"}
    for (sx, sz), iid in zip(slots, ids):
        ax.add_patch(plt.Circle((sx, sz), R, color=PAL[iid],
                                ec="black", zorder=3))
        ax.text(sx, sz, str(iid), color="white", ha="center", va="center",
                fontsize=5, zorder=4)

    # Cue ball at the optimum
    cx, cz = best["cueX"], best["cueZ"]
    ax.add_patch(plt.Circle((cx, cz), R, color="white",
                            ec="black", zorder=3))

    # Aim arrow from cue to (apex + aim offset in z)
    ax.annotate("", xy=(foot_x, z0 + best["aimDz"]),
                xytext=(cx, cz),
                arrowprops=dict(arrowstyle="->", color="yellow",
                                lw=1.8, alpha=0.95),
                zorder=4)

    ax.set_xlim(-0.08, table_xMax + 0.08)
    ax.set_ylim(-0.08, table_zMax + 0.08)
    ax.set_aspect("equal")
    ax.set_xlabel("x (m, table length)")
    ax.set_ylabel("z (m, table width)")
    ax.set_title("P(gold) heatmap in the kitchen + the optimal shot drawn",
                 fontsize=9)
    ax.tick_params(labelsize=7)


def draw_tip(ax, best, R=0.028575):
    """Cue ball cross-section showing the optimal (a, b) tip offset
    and the miscue circle |offset| <= R/2."""
    import numpy as np
    # Ball circle
    ax.add_patch(plt.Circle((0, 0), R, facecolor="white",
                            edgecolor="black", linewidth=1.0))
    # Miscue circle (a^2+b^2 <= (R/2)^2)
    ax.add_patch(plt.Circle((0, 0), R / 2, facecolor="none",
                            edgecolor="#aa0000", linestyle="--",
                            linewidth=0.8))
    # Centre cross
    ax.plot([-R, R], [0, 0], color="grey", lw=0.4)
    ax.plot([0, 0], [-R, R], color="grey", lw=0.4)
    # Optimal hit
    ax.plot([best["a"]], [best["b"]], marker="*", color="#f5c518",
            markeredgecolor="black", markersize=18, markeredgewidth=1.0,
            zorder=4)
    # Labels
    ax.text(best["a"], best["b"] + 0.005, " optimal tip", fontsize=7,
            color="black", va="bottom")
    ax.text(0, R / 2 + 0.002, "miscue limit", fontsize=6, color="#aa0000",
            ha="center")
    ax.text(R + 0.002, 0, "right english", fontsize=6, color="grey",
            ha="left", va="center")
    ax.text(0, R + 0.002, "follow", fontsize=6, color="grey",
            ha="center", va="bottom")
    ax.text(0, -R - 0.002, "draw", fontsize=6, color="grey",
            ha="center", va="top")

    ax.set_xlim(-R - 0.01, R + 0.01)
    ax.set_ylim(-R - 0.01, R + 0.01)
    ax.set_aspect("equal")
    ax.set_title("Tip offset (a horizontal, b vertical)", fontsize=9)
    ax.set_xlabel("a (m)")
    ax.set_ylabel("b (m)")
    ax.tick_params(labelsize=7)


def main() -> int:
    grid_path = Path("docs/golden_grid.csv")
    best_path = Path("docs/golden_best.txt")
    if not grid_path.exists() or not best_path.exists():
        print(f"missing {grid_path} or {best_path} -- run "
              f"build/golden_break first", file=sys.stderr)
        return 2

    try:
        import matplotlib
        matplotlib.use("Agg")
        global plt
        import matplotlib.pyplot as plt
    except ImportError as e:
        print(f"matplotlib unavailable ({e})", file=sys.stderr)
        return 1

    rows = load_grid(grid_path)
    best = load_best(best_path)
    R = 0.028575

    fig = plt.figure(figsize=(13, 9))
    gs = fig.add_gridspec(2, 2, width_ratios=[1.6, 1.0],
                          height_ratios=[1.0, 1.0],
                          hspace=0.30, wspace=0.25)

    # Common colour scale across all P(gold) heatmaps so the eye can
    # honestly compare "this region is brighter than that one."
    vmax_pct = max(max(r["pGold"] for r in rows) * 100, 1.0)

    # PANEL A (top, spans both columns): top-down table overhead with
    # the cueX-cueZ heatmap overlaid on the kitchen + the optimal cue
    # ball / aim arrow drawn at the winning cell. The centrepiece.
    axA = fig.add_subplot(gs[0, :])
    draw_table(axA, best, rows, vmax_pct=vmax_pct)
    # Heatmap colour bar (small, off to the side)
    sm = plt.cm.ScalarMappable(cmap="viridis",
                               norm=plt.Normalize(vmin=0, vmax=vmax_pct))
    sm.set_array([])
    cb = fig.colorbar(sm, ax=axA, fraction=0.025, pad=0.02,
                      label="P(gold) % in the kitchen")
    cb.ax.tick_params(labelsize=7)

    # PANEL B (bottom-left): speed x aim cut sensitivity at the optimum.
    fixed2 = {k: best[k] for k in ("cueX", "cueZ", "a", "b")}
    s2 = slice_at(rows, fixed2)
    axB = fig.add_subplot(gs[1, 0])
    imB = heatmap_2d(axB, s2, "speed", "aimDz", "pGold",
                     "P(gold) vs aim cut on the 1 and cue speed\n"
                     "(at the optimal cue placement and spin)",
                     "cue speed (m/s)",
                     "aim offset on the 1 (ball radii)",
                     star=(best["speed"], best["aimDz"]),
                     yfmt=lambda y: f"{y/R:+.2f} R",
                     vmax=vmax_pct)
    fig.colorbar(imB, ax=axB, fraction=0.046, pad=0.04,
                 label="P(gold) %")

    # PANEL C (bottom-right): tip diagram.
    axC = fig.add_subplot(gs[1, 1])
    draw_tip(axC, best)

    sup = (f"golden_break sweep -- best in grid: cue=({best['cueX']:.2f}, "
           f"{best['cueZ']:.2f}) m, aim {best['aimDz']/R:+.2f} R, "
           f"v={best['speed']:.0f} m/s, "
           f"spin (a, b)=({best['a']/R:+.2f}, {best['b']/R:+.2f}) R   |   "
           f"P(gold) = {best['pGold']*100:.1f}%")
    fig.suptitle(sup, fontsize=10, y=0.995)

    out = Path("docs/golden_break.png")
    fig.savefig(out, dpi=150, bbox_inches="tight")
    print(f"wrote {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
