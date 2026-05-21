#!/usr/bin/env python3
"""Render a cuesearch JSON trace to a top-down GIF.

    build/trace_shot.exe follow > follow.json
    python viz/render.py follow.json follow.gif

Frames are physically exact (the C++ side samples closed-form Segments),
so curves/arcs are real, not interpolated. Requires matplotlib + pillow;
if absent, the JSON still ships and this script documents how to render.
"""
import json
import sys


def main() -> int:
    if len(sys.argv) < 3:
        print("usage: render.py trace.json out.gif", file=sys.stderr)
        return 2
    trace = json.load(open(sys.argv[1]))

    try:
        import matplotlib

        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
        from matplotlib.animation import FuncAnimation, PillowWriter
    except ImportError as e:  # honest fallback
        print(f"matplotlib/pillow unavailable ({e}); JSON is valid and "
              f"can be rendered elsewhere.", file=sys.stderr)
        return 1

    R = trace["R"]
    T = trace["table"]
    frames = trace["frames"]

    fig, ax = plt.subplots(figsize=(8, 4))
    ax.set_xlim(-0.05, T["xMax"] + 0.05)
    ax.set_ylim(-0.05, T["zMax"] + 0.05)
    ax.set_aspect("equal")
    ax.set_facecolor("#0a6b3b")
    for px in (0, T["xMax"] / 2, T["xMax"]):
        for pz in (0, T["zMax"]):
            if px == T["xMax"] / 2 and pz not in (0, T["zMax"]):
                continue
            ax.add_patch(plt.Circle((px, pz), T["pr"], color="black"))
    # Real 9-ball colour palette (1=yellow, 2=blue, ..., 9=yellow-stripe).
    PAL = {
        0: "white", 1: "#f5c518", 2: "#2b6cb0", 3: "#c1272d", 4: "#6b3fa0",
        5: "#e76f00", 6: "#1f7a3a", 7: "#7a1d1d", 8: "#1a1a1a",
        9: "#f5c518",
    }
    dots = {}
    labels = {}

    def draw(i):
        for d in dots.values():
            d.remove()
        for tx in labels.values():
            tx.remove()
        dots.clear()
        labels.clear()
        for b in frames[i]["b"]:
            col = PAL.get(b["id"], "red")
            c = plt.Circle((b["x"], b["z"]), R, color=col, ec="black",
                           zorder=3)
            ax.add_patch(c)
            dots[b["id"]] = c
            if b["id"] != 0:
                tx = ax.text(b["x"], b["z"], str(b["id"]), color="white",
                             ha="center", va="center", fontsize=6,
                             zorder=4)
                labels[b["id"]] = tx
        ax.set_title(f"t = {frames[i]['t']:.2f}s")
        return list(dots.values()) + list(labels.values())

    step = max(1, len(frames) // 240)  # cap ~240 frames
    idx = list(range(0, len(frames), step))
    anim = FuncAnimation(fig, lambda k: draw(idx[k]), frames=len(idx),
                         interval=33, blit=False)
    anim.save(sys.argv[2], writer=PillowWriter(fps=30))
    print(f"wrote {sys.argv[2]} ({len(idx)} frames)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
