#!/usr/bin/env python3
"""
Visualize a RantanPlan solver log produced by LoggingPropagator.

Three display modes (--mode):
  scatter  Scatter plot: each ground variable is a row, decisions are markers;
           group bands show predicate families, band height reflects variable
           count — best for understanding solver behaviour (default)
  focus    Column-normalised heatmap, predicates sorted by temporal
           centre-of-mass — reveals whether the solver phases through
           predicates in a consistent order
  density  Raw-count heatmap on a log scale — shows absolute hot spots

Usage:
    python plot.py <logfile> [--mode {scatter,focus,density}]
                             [--lines N] [--bins N] [--output out.png] [--no-show]
"""
from __future__ import annotations

import argparse
import copy
import re
import sys
from pathlib import Path
from typing import TypedDict

try:
    import matplotlib.pyplot as plt
    import numpy as np
except ImportError:
    sys.exit("matplotlib is required: pip install matplotlib")


# ---------------------------------------------------------------------------
# Parsing
# ---------------------------------------------------------------------------

def parse_log(path: str, max_lines: int | None = None) -> list[tuple]:
    """Return a list of events in chronological order.

    Each event is one of:
        ("inc",)
        ("restart",)
        ("dec", name: str, is_pos: bool)
    """
    events = []
    with open(path) as f:
        for i, raw in enumerate(f):
            if max_lines is not None and i >= max_lines:
                break
            line = raw.rstrip("\n")
            if not line or line.startswith("#"):
                continue
            if line == "inc":
                events.append(("inc",))
            elif line == "restart":
                events.append(("restart",))
            elif line.startswith("dec "):
                rest = line[4:]
                if not rest:
                    continue
                events.append(("dec", rest[1:], rest[0] == "+"))
    return events


# ---------------------------------------------------------------------------
# Predicate extraction helpers
# ---------------------------------------------------------------------------

_TRAILING_INT_RE = re.compile(r"^(.*?)_(\d+)$")
_PARAM_START_RE = re.compile(r"_[a-zA-Z][a-zA-Z_-]*\d")
_SMTLIB_OP_RE = re.compile(r"^\((\S+)")
_PDDL_IDENT_RE = re.compile(r"[a-zA-Z][a-zA-Z-]+")


def _get_bool_predicate(name: str) -> str:
    m = _PARAM_START_RE.search(name)
    if m:
        return name[:m.start()]
    m2 = _TRAILING_INT_RE.match(name)
    return m2.group(1) if m2 else name


def _get_atom_predicate(name: str) -> str:
    op_m = _SMTLIB_OP_RE.match(name)
    op = op_m.group(1) if op_m else "?"
    for m in _PDDL_IDENT_RE.finditer(name[1 + len(op):]):
        return f"{op} {m.group(0)}"
    return op


# ---------------------------------------------------------------------------
# Variable ordering: one row per PDDL predicate (heatmap modes)
# ---------------------------------------------------------------------------

def _build_predicate_order(
    events: list[tuple],
) -> tuple[dict[str, int], list[tuple[int, str]], float | None, int]:
    """One y-row per PDDL predicate.

    Returns:
        var_order  – {var_name: y_index}
        tick_info  – [(y_index, label), …] sorted by y_index
        sep_y      – y position of the boolean/atom separator (or None)
        n_rows     – total number of rows (includes gap row)
    """
    bool_preds: set[str] = set()
    atom_preds: set[str] = set()
    all_names: set[str] = set()

    for ev in events:
        if ev[0] == "dec":
            name = ev[1]
            all_names.add(name)
            if name.startswith("("):
                atom_preds.add(_get_atom_predicate(name))
            else:
                bool_preds.add(_get_bool_predicate(name))

    sorted_bool = sorted(bool_preds)
    sorted_atom = sorted(atom_preds)

    pred_to_row: dict[str, int] = {}
    for i, p in enumerate(sorted_bool):
        pred_to_row[p] = i
    gap = len(sorted_bool)
    for j, p in enumerate(sorted_atom):
        pred_to_row[p] = gap + 1 + j

    var_order: dict[str, int] = {
        name: pred_to_row[
            _get_atom_predicate(name) if name.startswith("(") else _get_bool_predicate(name)
        ]
        for name in all_names
    }

    tick_info = sorted((row, label) for label, row in pred_to_row.items())
    sep_y = gap + 0.5 if (sorted_bool and sorted_atom) else None
    n_rows = gap + 1 + len(sorted_atom) + 1

    return var_order, tick_info, sep_y, n_rows


# ---------------------------------------------------------------------------
# Variable ordering: one row per ground variable (scatter mode)
# ---------------------------------------------------------------------------

def _build_var_order(
    events: list[tuple],
) -> tuple[dict[str, int], list[tuple[str, int, int]], float | None, int]:
    """One y-row per ground variable, grouped by predicate.

    Returns:
        var_order  – {var_name: y_index}
        groups     – [(pred_name, y_start, y_end), …] in y order
        sep_y      – y boundary between boolean/atom sections (or None)
        n_vars     – total number of variable rows
    """
    bool_groups: dict[str, set[str]] = {}
    atom_groups: dict[str, set[str]] = {}

    for ev in events:
        if ev[0] == "dec":
            name = ev[1]
            if name.startswith("("):
                atom_groups.setdefault(_get_atom_predicate(name), set()).add(name)
            else:
                bool_groups.setdefault(_get_bool_predicate(name), set()).add(name)

    var_order: dict[str, int] = {}
    groups: list[tuple[str, int, int]] = []
    y = 0

    for pred in sorted(bool_groups):
        y_start = y
        for var in sorted(bool_groups[pred]):
            var_order[var] = y
            y += 1
        groups.append((pred, y_start, y - 1))

    sep_y = y - 0.5 if (bool_groups and atom_groups) else None

    for pred in sorted(atom_groups):
        y_start = y
        for var in sorted(atom_groups[pred]):
            var_order[var] = y
            y += 1
        groups.append((pred, y_start, y - 1))

    return var_order, groups, sep_y, y


# ---------------------------------------------------------------------------
# Shared helpers
# ---------------------------------------------------------------------------

class _HeatmapResult(TypedDict):
    counts: np.ndarray               # shape (n_bins, n_rows)
    row_order: list[int]
    sorted_ticks: list[tuple[int, str]]
    row_to_label: dict[int, str]
    sep_y: float | None
    n_rows: int
    n_decisions: int
    inc_xs: list[float]
    restart_xs: list[float]


def _build_heatmap(events: list[tuple], n_bins: int) -> _HeatmapResult:
    """Compute 2-D histogram, CoM row order, and tick metadata."""
    var_order, tick_info, sep_y, n_rows = _build_predicate_order(events)

    xs: list[float] = []
    ys: list[float] = []
    inc_xs: list[float] = []
    restart_xs: list[float] = []
    dec_idx = 0

    for ev in events:
        if ev[0] == "inc":
            inc_xs.append(dec_idx - 0.5)
        elif ev[0] == "restart":
            restart_xs.append(dec_idx - 0.5)
        else:
            name = ev[1]
            if name in var_order:
                xs.append(dec_idx)
                ys.append(var_order[name])
            dec_idx += 1

    n_decisions = dec_idx
    if not n_decisions:
        sys.exit("No decisions found in log.")

    counts, _, _ = np.histogram2d(
        np.array(xs, dtype=float), np.array(ys, dtype=float),
        bins=[n_bins, n_rows],
        range=[[0, n_decisions], [-0.5, n_rows - 0.5]],
    )                                # shape (n_bins, n_rows)

    row_order = _com_row_order(counts, sep_y, n_rows, n_decisions)
    old_to_new = {int(old): int(new) for new, old in enumerate(row_order)}
    sorted_ticks = sorted(
        (old_to_new[t[0]], t[1]) for t in tick_info if t[0] in old_to_new
    )
    row_to_label = {pos: label for pos, label in sorted_ticks}
    new_sep_y = old_to_new[int(sep_y - 0.5)] + 0.5 if sep_y is not None else None

    return _HeatmapResult(
        counts=counts, row_order=row_order, sorted_ticks=sorted_ticks,
        row_to_label=row_to_label, sep_y=new_sep_y,
        n_rows=n_rows, n_decisions=n_decisions,
        inc_xs=inc_xs, restart_xs=restart_xs,
    )


def _com_row_order(counts: np.ndarray, sep_y: float | None,
                   n_rows: int, n_decisions: int) -> list[int]:
    """Sort predicate rows by temporal centre-of-mass within each group."""
    col_sums = counts.sum(axis=1, keepdims=True)
    col_sums[col_sums == 0] = 1.0
    normed = counts / col_sums

    n_bins = counts.shape[0]
    bin_centres = (np.arange(n_bins) + 0.5) * (n_decisions / n_bins)
    row_totals = normed.sum(axis=0)
    centers = np.where(
        row_totals > 0,
        (normed * bin_centres[:, np.newaxis]).sum(axis=0) / np.maximum(row_totals, 1e-12),
        float(n_decisions) / 2.0,
    )

    if sep_y is not None:
        gap = int(sep_y - 0.5)
        n_atom = n_rows - gap - 2
        bool_rows = sorted(range(gap), key=lambda r: centers[r])
        atom_rows = sorted(range(gap + 1, gap + 1 + n_atom), key=lambda r: centers[r])
        return bool_rows + [gap] + atom_rows + [n_rows - 1]
    return sorted(range(n_rows - 1), key=lambda r: centers[r]) + [n_rows - 1]


def _make_fig(n_decisions: int, n_rows: int = 30):
    fig_w = max(16, min(30, n_decisions / 80))
    # Give each row ~0.3" so cells are roughly equi-height regardless of n_bins
    fig_h = max(4.0, min(20.0, n_rows * 0.3))
    return plt.subplots(figsize=(fig_w, fig_h))


def _draw_vlines(ax, inc_xs: list[float], restart_xs: list[float]) -> None:
    for x in inc_xs:
        ax.axvline(x, color="#bbbbbb", linewidth=0.8, linestyle="--", zorder=1)
    for x in restart_xs:
        ax.axvline(x, color="#e05050", linewidth=1.2, alpha=0.5, zorder=1)


def _apply_ticks_and_sep(ax, sorted_ticks: list[tuple[int, str]],
                          n_rows: int, sep_y: float | None, font_size: float) -> None:
    ax.set_yticks([t[0] for t in sorted_ticks])
    ax.set_yticklabels([t[1] for t in sorted_ticks], fontsize=font_size, family="monospace")
    ax.set_ylim(-0.5, n_rows - 0.5)
    if sep_y is not None:
        ax.axhline(sep_y, color="#aaaaaa", linewidth=0.8, linestyle=":", zorder=3)


def _add_hover(ax, data_2d: np.ndarray, row_to_label: dict[int, str],
               n_decisions: int, value_fmt) -> None:
    """Hover: moving the mouse over a heatmap cell shows its predicate and value."""
    n_bins = data_2d.shape[1]
    annot = ax.annotate(
        "", xy=(0, 0), xytext=(14, 14), textcoords="offset points",
        bbox=dict(boxstyle="round,pad=0.35", fc="white", ec="#888888", alpha=0.92),
        fontsize=8, family="monospace",
    )
    annot.set_visible(False)

    def on_move(event):
        if event.inaxes != ax or event.xdata is None or event.ydata is None:
            if annot.get_visible():
                annot.set_visible(False)
                ax.figure.canvas.draw_idle()
            return
        row = int(round(event.ydata))
        col = max(0, min(n_bins - 1, int(event.xdata * n_bins / n_decisions)))
        label = row_to_label.get(row)
        if label and 0 <= row < data_2d.shape[0]:
            val = data_2d[row, col]
            if not np.isnan(val):
                annot.xy = (event.xdata, event.ydata)
                annot.set_text(f"{label}\n{value_fmt(val)}")
                annot.set_visible(True)
            else:
                annot.set_visible(False)
        else:
            annot.set_visible(False)
        ax.figure.canvas.draw_idle()

    ax.figure.canvas.mpl_connect("motion_notify_event", on_move)


def _add_scatter_hover(ax, xs_arr: np.ndarray, ys_arr: np.ndarray,
                        var_names: list[str], n_decisions: int, n_vars: int) -> None:
    """Hover: show the full variable name for the nearest scatter point."""
    if len(xs_arr) == 0:
        return
    # Normalise axes so x and y contribute equally to the distance metric
    xs_n = xs_arr / max(n_decisions, 1)
    ys_n = ys_arr / max(n_vars, 1)

    annot = ax.annotate(
        "", xy=(0, 0), xytext=(14, 14), textcoords="offset points",
        bbox=dict(boxstyle="round,pad=0.35", fc="white", ec="#888888", alpha=0.92),
        fontsize=8, family="monospace",
    )
    annot.set_visible(False)

    def on_move(event):
        if event.inaxes != ax or event.xdata is None or event.ydata is None:
            if annot.get_visible():
                annot.set_visible(False)
                ax.figure.canvas.draw_idle()
            return
        px = event.xdata / max(n_decisions, 1)
        py = event.ydata / max(n_vars, 1)
        dists = (xs_n - px) ** 2 + (ys_n - py) ** 2
        idx = int(np.argmin(dists))
        if dists[idx] < 2e-4:
            annot.xy = (float(xs_arr[idx]), float(ys_arr[idx]))
            annot.set_text(var_names[idx])
            annot.set_visible(True)
        else:
            annot.set_visible(False)
        ax.figure.canvas.draw_idle()

    ax.figure.canvas.mpl_connect("motion_notify_event", on_move)


def _finalize(output_path: str | None, show: bool) -> None:
    plt.tight_layout()
    if output_path:
        plt.savefig(output_path, dpi=150, bbox_inches="tight")
        print(f"Saved to {output_path}")
    if show:
        plt.show()


# ---------------------------------------------------------------------------
# Plot mode 1: scatter plot (per-variable rows, group bands)
# ---------------------------------------------------------------------------

def plot_scatter(events: list[tuple], output_path: str | None,
                 show: bool, title: str) -> None:
    """Scatter plot with one row per ground variable.

    Variables are grouped by predicate; each group is drawn as a coloured band
    whose height reflects the number of ground variables it contains.
    Hover over a point to see the full variable name.
    """
    var_order, groups, sep_y, n_vars = _build_var_order(events)

    xs: list[int] = []
    ys: list[int] = []
    var_names: list[str] = []
    inc_xs: list[float] = []
    restart_xs: list[float] = []
    dec_idx = 0

    for ev in events:
        if ev[0] == "inc":
            inc_xs.append(dec_idx - 0.5)
        elif ev[0] == "restart":
            restart_xs.append(dec_idx - 0.5)
        else:
            name = ev[1]
            if name in var_order:
                xs.append(dec_idx)
                ys.append(var_order[name])
                var_names.append(name)
            dec_idx += 1

    n_decisions = dec_idx
    if not n_decisions:
        sys.exit("No decisions found in log.")

    fig_w = max(16, min(30, n_decisions / 80))
    # Each variable row gets ~0.15"; tighter than heatmap since there are more rows
    fig_h = max(4.0, min(20.0, n_vars * 0.15))
    _, ax = plt.subplots(figsize=(fig_w, fig_h))

    # Alternating green bands, one per predicate group
    band_colors = ["#c8eac8", "#a8d4a8"]
    for i, (_, y_start, y_end) in enumerate(groups):
        ax.axhspan(y_start - 0.5, y_end + 0.5,
                   color=band_colors[i % 2], alpha=0.6, zorder=0)

    # Scatter decisions as small + markers
    ax.scatter(xs, ys, marker="+", s=8, c="#5500bb", alpha=0.4,
               linewidths=0.5, zorder=2)

    # Vertical event markers
    for x in inc_xs:
        ax.axvline(x, color="#222222", linewidth=1.5, zorder=3)
    for x in restart_xs:
        ax.axvline(x, color="#e05050", linewidth=1.0, alpha=0.5, zorder=3)

    # Boolean / atom section separator
    if sep_y is not None:
        ax.axhline(sep_y, color="#444444", linewidth=1.0, linestyle="--", zorder=4)

    # Y-axis: group label centred on each band
    tick_ys = [(y_start + y_end) / 2 for _, y_start, y_end in groups]
    tick_labels = [pred for pred, _, _ in groups]
    font_size = max(6, min(10, 200 / max(len(groups), 1)))
    ax.set_yticks(tick_ys)
    ax.set_yticklabels(tick_labels, fontsize=font_size, family="monospace")
    ax.set_ylim(-0.5, n_vars - 0.5)
    ax.set_xlim(0, n_decisions)
    ax.set_xlabel("decisions along time")
    ax.set_ylabel("variables")
    ax.set_title(title)

    _add_scatter_hover(ax, np.array(xs, dtype=float), np.array(ys, dtype=float),
                       var_names, n_decisions, n_vars)
    _finalize(output_path, show)


# ---------------------------------------------------------------------------
# Plot mode 2: attention focus heatmap
# ---------------------------------------------------------------------------

def plot_focus(events: list[tuple], output_path: str | None,
               show: bool, title: str, n_bins: int = 300) -> None:
    """Column-normalised heatmap sorted by temporal centre-of-mass.

    Colour = fraction of decisions at this time step going to this predicate.
    """
    from matplotlib.colors import Normalize

    h = _build_heatmap(events, n_bins)
    counts = h["counts"]

    col_sums = counts.sum(axis=1, keepdims=True)
    col_sums[col_sums == 0] = 1.0
    normed = counts / col_sums
    data_2d = np.where(normed[:, h["row_order"]].T > 0, normed[:, h["row_order"]].T, np.nan)

    cmap = copy.copy(plt.cm.Blues)
    cmap.set_bad("white")

    _, ax = _make_fig(h["n_decisions"], h["n_rows"])
    ax.imshow(data_2d, aspect="auto", origin="lower",
              extent=(0, h["n_decisions"], -0.5, h["n_rows"] - 0.5),
              cmap=cmap, vmin=0.0, vmax=1.0, interpolation="nearest")
    _draw_vlines(ax, h["inc_xs"], h["restart_xs"])

    font_size = max(6, min(10, 300 / max(len(h["sorted_ticks"]), 1)))
    _apply_ticks_and_sep(ax, h["sorted_ticks"], h["n_rows"], h["sep_y"], font_size)

    sm = plt.cm.ScalarMappable(cmap=cmap, norm=Normalize(0, 1))
    sm.set_array([])
    plt.colorbar(sm, ax=ax, label="fraction of decisions at this step", shrink=0.8)
    ax.set_xlabel("Decision step")
    ax.set_title(title + "  [focus: sorted by temporal centre-of-mass]")

    _add_hover(ax, data_2d, h["row_to_label"], h["n_decisions"],
               lambda v: f"{v * 100:.1f}% of step")
    _finalize(output_path, show)


# ---------------------------------------------------------------------------
# Plot mode 3: raw-count density heatmap
# ---------------------------------------------------------------------------

def plot_density(events: list[tuple], output_path: str | None,
                 show: bool, title: str, n_bins: int = 1000) -> None:
    """Raw-count heatmap on a log scale, same row ordering as focus mode.

    Colour = total number of decisions in that time-bin × predicate cell.
    """
    from matplotlib.colors import LogNorm

    h = _build_heatmap(events, n_bins)
    counts = h["counts"]
    data_2d = np.where(counts[:, h["row_order"]].T > 0, counts[:, h["row_order"]].T, np.nan)

    cmap = copy.copy(plt.cm.YlOrRd)
    cmap.set_bad("white")

    _, ax = _make_fig(h["n_decisions"], h["n_rows"])
    ax.imshow(data_2d, aspect="auto", origin="lower",
              extent=(0, h["n_decisions"], -0.5, h["n_rows"] - 0.5),
              cmap=cmap, norm=LogNorm(vmin=1, vmax=max(counts.max(), 1)),
              interpolation="nearest")
    _draw_vlines(ax, h["inc_xs"], h["restart_xs"])

    font_size = max(6, min(10, 300 / max(len(h["sorted_ticks"]), 1)))
    _apply_ticks_and_sep(ax, h["sorted_ticks"], h["n_rows"], h["sep_y"], font_size)

    plt.colorbar(ax.images[0], ax=ax, label="decisions per cell (log scale)", shrink=0.8)
    ax.set_xlabel("Decision step")
    ax.set_title(title)

    _add_hover(ax, data_2d, h["row_to_label"], h["n_decisions"],
               lambda v: f"{int(v)} decisions")
    _finalize(output_path, show)


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main() -> None:
    parser = argparse.ArgumentParser(
        description="Plot a RantanPlan solver log (LoggingPropagator output).",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
modes:
  scatter  scatter plot with one row per ground variable; group bands show
           predicate families, band height reflects variable count (default)
  focus    column-normalised heatmap sorted by temporal centre-of-mass;
           colour = fraction of decisions at this step going to this predicate
  density  raw-count heatmap on a log scale; colour = total decisions per cell
""",
    )
    parser.add_argument("logfile", help="Path to the solver log file")
    parser.add_argument("--mode", choices=["scatter", "focus", "density"],
                        default="scatter", metavar="MODE",
                        help="scatter | focus | density  (default: scatter)")
    parser.add_argument("--lines", type=int, default=None, metavar="N",
                        help="only read the first N lines of the log file")
    parser.add_argument("--bins", type=int, default=300, metavar="N",
                        help="number of time bins for heatmap modes (default: 300)")
    parser.add_argument("--output", metavar="FILE",
                        help="Save plot to FILE (PNG, PDF, …)")
    parser.add_argument("--no-show", action="store_true",
                        help="Do not open an interactive window")
    args = parser.parse_args()

    events = parse_log(args.logfile, args.lines)
    if not events:
        sys.exit("Log file is empty or contains no recognised events.")

    n_dec = sum(1 for e in events if e[0] == "dec")
    n_inc = sum(1 for e in events if e[0] == "inc")
    n_restart = sum(1 for e in events if e[0] == "restart")
    n_vars = len({e[1] for e in events if e[0] == "dec"})
    print(f"Events: {n_dec} decisions, {n_inc} inc, {n_restart} restarts  "
          f"| Variables: {n_vars}  | Mode: {args.mode}")

    title = Path(args.logfile).stem
    show = not args.no_show

    if args.mode == "scatter":
        plot_scatter(events, args.output, show, title)
    elif args.mode == "focus":
        plot_focus(events, args.output, show, title, args.bins)
    else:
        plot_density(events, args.output, show, title, args.bins)


if __name__ == "__main__":
    main()