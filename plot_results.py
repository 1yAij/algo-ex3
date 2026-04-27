import pathlib
import os

ROOT = pathlib.Path(__file__).resolve().parent
CSV_PATH = ROOT / "cpp" / "output" / "graph_coloring_results.csv"
FIG_DIR = ROOT / "cpp" / "output" / "figures"
CACHE_DIR = ROOT / "cpp" / "output" / ".cache"

os.environ.setdefault("MPLCONFIGDIR", str(CACHE_DIR / "mpl"))
os.environ.setdefault("XDG_CACHE_HOME", str(CACHE_DIR))

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
import matplotlib.ticker as ticker
import pandas as pd
import numpy as np
import seaborn as sns

PALETTE = ["#4C72B0", "#DD8452", "#55A868", "#C44E52", "#8172B2", "#937860", "#DA8BC3"]
STRATEGY_ORDER = [
    "mrv",
    "fc_mrv",
    "fc_mrv_lcv",
]
STRATEGY_LABELS = {
    "mrv": "MRV",
    "fc_mrv": "FC+MRV",
    "fc_mrv_lcv": "FC+MRV+LCV",
}


def ensure_dirs() -> None:
    FIG_DIR.mkdir(parents=True, exist_ok=True)
    CACHE_DIR.mkdir(parents=True, exist_ok=True)


def load_data() -> pd.DataFrame:
    df = pd.read_csv(CSV_PATH)
    return df


# ── Experiment 2 ─────────────────────────────────────────────────────────────

def plot_experiment_2(df: pd.DataFrame) -> None:
    exp2 = df[df["experiment"] == "experiment_2"].copy()

    # sort by dataset then color_limit
    exp2 = exp2.sort_values(["instance", "color_limit"])

    # natural dataset order: 5a → 15b → 25a
    dataset_order = ["le450_5a.col", "le450_15b.col", "le450_25a.col"]
    datasets = [d for d in dataset_order if d in exp2["instance"].values]
    color_limits = sorted(exp2["color_limit"].unique())
    n_datasets = len(datasets)
    n_colors = len(color_limits)

    bar_width = 0.22
    x = np.arange(n_datasets)
    offsets = np.linspace(-(n_colors - 1) / 2, (n_colors - 1) / 2, n_colors) * bar_width

    pal = ["#4C72B0", "#DD8452", "#55A868"]

    fig, ax = plt.subplots(figsize=(10, 6))
    ax.set_yscale("log")

    for i, (cl, color) in enumerate(zip(color_limits, pal)):
        vals = []
        hatches = []
        for ds in datasets:
            row = exp2[(exp2["instance"] == ds) & (exp2["color_limit"] == cl)]
            if row.empty:
                vals.append(np.nan)
                hatches.append("")
            else:
                vals.append(float(row["time_ms"].iloc[0]))
                hatches.append("//" if int(row["aborted"].iloc[0]) == 1 else "")

        bars = ax.bar(
            x + offsets[i], vals, width=bar_width,
            color=color, label=f"Color Limit = {cl}",
            edgecolor="white", linewidth=0.6,
        )
        # hatch aborted bars to mark timeout
        for bar, h in zip(bars, hatches):
            if h:
                bar.set_hatch(h)
                bar.set_edgecolor("black")

    # annotate each bar with its value
    for bar in ax.patches:
        h = bar.get_height()
        if np.isfinite(h) and h > 0:
            ax.text(
                bar.get_x() + bar.get_width() / 2,
                h * 1.15,
                f"{h:.0f}" if h >= 10 else f"{h:.2f}",
                ha="center", va="bottom", fontsize=7.5, color="#333333",
            )

    ax.set_xticks(x)
    ax.set_xticklabels(datasets, fontsize=10)
    ax.set_xlabel("Dataset", fontsize=12)
    ax.set_ylabel("Runtime (ms, log scale)", fontsize=12)
    ax.set_title("Experiment 2: le450 Dataset Runtime vs Color Limit", fontsize=13, fontweight="bold")

    legend_handles = [
        mpatches.Patch(facecolor=pal[i], label=f"Color Limit = {cl}")
        for i, cl in enumerate(color_limits)
    ]
    legend_handles.append(
        mpatches.Patch(facecolor="white", edgecolor="black", hatch="//", label="Aborted (timeout)")
    )
    ax.legend(handles=legend_handles, fontsize=9)
    ax.yaxis.set_major_formatter(ticker.FuncFormatter(lambda v, _: f"{v:g}"))
    sns.despine(ax=ax, left=False)
    fig.tight_layout()
    fig.savefig(FIG_DIR / "experiment_2_runtime.png", dpi=300)
    plt.close(fig)


# ── Experiment 3 ─────────────────────────────────────────────────────────────

def plot_experiment_3(df: pd.DataFrame) -> None:
    exp3 = df[df["experiment"] == "experiment_3"].copy()
    exp3["strategy_label"] = exp3["instance"].map(STRATEGY_LABELS)
    exp3["strategy_label"] = pd.Categorical(
        exp3["strategy_label"],
        categories=[STRATEGY_LABELS[s] for s in STRATEGY_ORDER],
        ordered=True,
    )
    exp3 = exp3.sort_values("strategy_label")

    colors = [PALETTE[i] for i in range(len(STRATEGY_ORDER))]

    fig, axes = plt.subplots(1, 2, figsize=(11, 5), sharey=False)

    # Backtracks
    sns.barplot(
        data=exp3, x="strategy_label", y="backtracks",
        palette=colors, ax=axes[0], edgecolor="white", linewidth=0.6,
    )
    axes[0].set_title("Backtracks by Strategy", fontsize=12, fontweight="bold")
    axes[0].set_xlabel("Strategy", fontsize=11)
    axes[0].set_ylabel("Backtracks", fontsize=11)
    axes[0].set_xticklabels(axes[0].get_xticklabels(), rotation=15, ha="right", fontsize=9)
    axes[0].yaxis.set_major_formatter(ticker.FuncFormatter(
        lambda v, _: f"{v/1e6:.1f}M" if v >= 1e6 else f"{v/1e3:.0f}K" if v >= 1e3 else f"{v:.0f}"
    ))
    bt_max = float(exp3["backtracks"].max())
    for bar, (_, row) in zip(axes[0].patches, exp3.iterrows()):
        h = float(row["backtracks"])
        if h >= 1e6:
            label = f"{h/1e6:.2f}M"
        elif h >= 1e3:
            label = f"{h/1e3:.0f}K"
        else:
            label = f"{h:.0f}"
        # 回溯=0 时柱高为 0，仍要标出 "0"（原逻辑 if h>0 会不画，看起来像没数据）
        y_anno = h * 1.02 if h > 0 else max(bt_max * 0.012, 0.1)
        axes[0].text(
            bar.get_x() + bar.get_width() / 2, y_anno, label, ha="center", va="bottom", fontsize=8.5
        )

    # Runtime
    sns.barplot(
        data=exp3, x="strategy_label", y="time_ms",
        palette=colors, ax=axes[1], edgecolor="white", linewidth=0.6,
    )
    axes[1].set_title("Runtime by Strategy", fontsize=12, fontweight="bold")
    axes[1].set_xlabel("Strategy", fontsize=11)
    axes[1].set_ylabel("Runtime (ms)", fontsize=11)
    axes[1].set_xticklabels(axes[1].get_xticklabels(), rotation=15, ha="right", fontsize=9)
    for bar, (_, row) in zip(axes[1].patches, exp3.iterrows()):
        h = float(row["time_ms"])
        tlab = f"{h:.2f} ms" if h < 5 else f"{h:.0f} ms"
        y_anno = h * 1.02 if h > 0.001 else 0.05
        axes[1].text(
            bar.get_x() + bar.get_width() / 2, y_anno, tlab, ha="center", va="bottom", fontsize=8.5
        )

    fig.suptitle(
        "Experiment 3: Strategy Comparison (k=5, le450_5a, 450 vertices)",
        fontsize=13, fontweight="bold", y=1.01,
    )
    sns.despine()
    fig.tight_layout()
    fig.savefig(FIG_DIR / "experiment_3_strategy_comparison.png", dpi=300, bbox_inches="tight")
    plt.close(fig)


# ── Experiment 4 ─────────────────────────────────────────────────────────────

def plot_experiment_4(df: pd.DataFrame) -> None:
    exp4 = df[df["experiment"] == "experiment_4"].copy()
    exp4["vertices"] = exp4["instance"].str.extract(r"V(\d+)").astype(int)
    # 与 main 一致：V{n}_d{8|14} 表示目标平均度 d，不是常数 p
    exp4["target_deg"] = exp4["instance"].str.extract(r"_d(\d+)", expand=False)
    exp4 = exp4.sort_values(["target_deg", "vertices"])

    pal_series = {"8": "#4C72B0", "14": "#DD8452"}

    fig, axes = plt.subplots(1, 2, figsize=(13, 5))

    # Runtime
    for td, grp in exp4.groupby("target_deg"):
        axes[0].plot(
            grp["vertices"], grp["time_ms"],
            marker="o", linewidth=2, label=f"Target avg degree d = {td} (p = d/(n-1))",
            color=pal_series.get(str(td), "gray"),
        )
        for _, row in grp.iterrows():
            axes[0].annotate(
                f"{row['time_ms']:.3f}",
                (row["vertices"], row["time_ms"]),
                textcoords="offset points", xytext=(4, 4), fontsize=8,
            )
    axes[0].set_title("Runtime vs Graph Size", fontsize=12, fontweight="bold")
    axes[0].set_xlabel("Vertices", fontsize=11)
    axes[0].set_ylabel("Runtime (ms)", fontsize=11)
    axes[0].set_xticks(sorted(exp4["vertices"].unique()))
    axes[0].legend(fontsize=9)

    for td, grp in exp4.groupby("target_deg"):
        color = pal_series.get(str(td), "gray")
        axes[1].plot(
            grp["vertices"], grp["backtracks"],
            marker="s", linewidth=2, label=f"Target avg degree d = {td}",
            color=color,
        )
        for _, row in grp.iterrows():
            axes[1].annotate(
                f"{int(row['backtracks'])}",
                (row["vertices"], row["backtracks"]),
                textcoords="offset points", xytext=(4, 8), fontsize=8,
            )

    axes[1].set_title("Backtracks vs Graph Size", fontsize=12, fontweight="bold")
    axes[1].set_xlabel("Vertices", fontsize=11)
    axes[1].set_ylabel("Backtracks", fontsize=11)
    axes[1].set_xticks(sorted(exp4["vertices"].unique()))
    axes[1].legend(fontsize=9)

    # 大跨度时用对数纵轴，避免小 n 的毫秒级与近 60s 的截断在一张图里「全贴底 + 一柱」
    if exp4["backtracks"].max() / max(1, (exp4["backtracks"] + 1e-9).min()) > 200:
        axes[1].set_yscale("log")
        axes[1].set_ylabel("Backtracks (log scale)", fontsize=11)
    if exp4["time_ms"].max() / max(0.1, (exp4["time_ms"] + 1e-6).min()) > 200:
        axes[0].set_yscale("log")
        axes[0].set_ylabel("Runtime (ms, log scale)", fontsize=11)

    fig.suptitle(
        "Experiment 4: Scale (k=28, G(n,p) with p=d/(n-1), d in {8, 14}, |V|=100…500)",
        fontsize=11, fontweight="bold", y=1.01,
    )
    sns.despine()
    fig.tight_layout()
    fig.savefig(FIG_DIR / "experiment_4_scale_sensitivity.png", dpi=300, bbox_inches="tight")
    plt.close(fig)


# ── Experiment 5 ─────────────────────────────────────────────────────────────

def plot_experiment_5(df: pd.DataFrame) -> None:
    exp5 = df[df["experiment"] == "experiment_5"].copy()
    exp5 = exp5.sort_values("color_limit")

    # status label for each point
    def status(row):
        if row["aborted"] == 1:
            return "Timeout"
        elif row["success"] == 1:
            return "Success"
        else:
            return "Failed (fast)"

    exp5["status"] = exp5.apply(status, axis=1)
    status_colors = {"Success": "#55A868", "Timeout": "#C44E52", "Failed (fast)": "#8172B2"}
    status_markers = {"Success": "o", "Timeout": "X", "Failed (fast)": "s"}

    color_limits = exp5["color_limit"].values
    times = exp5["time_ms"].values
    backtracks = exp5["backtracks"].values
    statuses = exp5["status"].values

    fig, ax1 = plt.subplots(figsize=(9, 5))
    ax2 = ax1.twinx()

    # plot runtime as line on ax1
    ax1.plot(color_limits, times, color="#4C72B0", linewidth=2, zorder=2, label="_nolegend_")

    # scatter points colored by status
    for i, (cl, t, bt, s) in enumerate(zip(color_limits, times, backtracks, statuses)):
        ax1.scatter(cl, t, color=status_colors[s], marker=status_markers[s],
                    s=90, zorder=3, edgecolors="white", linewidth=0.8)
        ax1.annotate(
            f"{t:.1f} ms", (cl, t),
            textcoords="offset points", xytext=(5, 6), fontsize=8, color="#333333",
        )

    # backtracks as bar on ax2
    ax2.bar(color_limits, backtracks, width=0.6, color="#DD8452", alpha=0.35, zorder=1)
    ax2.set_ylabel("Backtracks", fontsize=11, color="#DD8452")
    ax2.tick_params(axis="y", labelcolor="#DD8452")
    ax2.yaxis.set_major_formatter(ticker.FuncFormatter(
        lambda v, _: f"{v/1e6:.1f}M" if v >= 1e6 else f"{v/1e3:.0f}K" if v >= 1e3 else f"{v:.0f}"
    ))

    ax1.set_xlabel("Color Limit", fontsize=11)
    ax1.set_ylabel("Runtime (ms)", fontsize=11, color="#4C72B0")
    ax1.tick_params(axis="y", labelcolor="#4C72B0")
    ax1.set_xticks(color_limits)
    ax1.set_title("Experiment 5: Runtime & Backtracks vs Color Limit\n(Dataset: le450_15b.col)", fontsize=12, fontweight="bold")

    # legend for status
    legend_handles = [
        plt.Line2D([0], [0], marker=status_markers[s], color="w", markerfacecolor=status_colors[s],
                   markersize=9, label=s, markeredgecolor="gray", markeredgewidth=0.5)
        for s in ["Success", "Timeout", "Failed (fast)"]
    ]
    legend_handles.append(
        mpatches.Patch(facecolor="#DD8452", alpha=0.4, label="Backtracks")
    )
    ax1.legend(handles=legend_handles, fontsize=8.5, loc="upper right")

    sns.despine(ax=ax1, right=False)
    fig.tight_layout()
    fig.savefig(FIG_DIR / "experiment_5_color_sensitivity.png", dpi=300)
    plt.close(fig)


# ─────────────────────────────────────────────────────────────────────────────

def main() -> None:
    sns.set_theme(style="whitegrid", context="talk", font_scale=0.9)
    ensure_dirs()
    df = load_data()
    plot_experiment_2(df)
    plot_experiment_3(df)
    plot_experiment_4(df)
    plot_experiment_5(df)
    print("All figures saved to", FIG_DIR)


if __name__ == "__main__":
    main()
