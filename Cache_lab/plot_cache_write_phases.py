import csv
from collections import defaultdict
from pathlib import Path
from statistics import mean, median

import matplotlib.pyplot as plt

base = Path(__file__).resolve().parent
groups = defaultdict(list)
for name in ("cache_write_phases.csv", "cache_write_control.csv"):
    with (base / "results" / name).open(newline="") as file:
        for row in csv.DictReader(file):
            groups[(row["case"], row["phase"])].append(row)

fields = ["tsc_ticks", "store_dispatch", "l2_store_or_permission",
          "wcb_write", "wcb_close", "l2_all_requests"]
averages = {
    key: {field: mean(float(row[field]) for row in rows) for field in fields}
    for key, rows in groups.items()
}
with (base / "results/cache_write_phases_summary.csv").open("w", newline="") as file:
    writer = csv.writer(file)
    writer.writerow(["case", "phase", "samples", *["mean_" + f for f in fields],
                     "median_tsc_ticks", "min_running_fraction"])
    for key, rows in sorted(groups.items()):
        writer.writerow([
            *key, len(rows), *[f"{averages[key][f]:.6f}" for f in fields],
            median(float(row["tsc_ticks"]) for row in rows),
            min(int(row["time_running"]) / int(row["time_enabled"]) for row in rows),
        ])

fig, axes = plt.subplots(1, 3, figsize=(15, 4.5))

# 两种计数分别画柱，明确请求类型，而不标成写回次数。
for offset, field, label in [(-0.18, "store_dispatch", "Store dispatches"),
                            (0.18, "l2_store_or_permission", "L2 store / permission requests")]:
    heights = [averages[(case, "access")][field] for case in ("write_1", "write_100")]
    bars = axes[0].bar([offset, 1 + offset], heights, width=0.36, label=label)
    axes[0].bar_label(bars, fmt="%.1f", padding=3, fontsize=8)
axes[0].set_xticks([0, 1], ["1 write round", "100 write rounds"])
axes[0].set_yscale("log")
axes[0].set_ylim(1, 100000)
axes[0].set_ylabel("Mean events per access phase (log scale)")
axes[0].set_title("128 cache lines retained in L1")
axes[0].legend(fontsize=7, loc="upper left")

for offset, field, label in [(-0.18, "wcb_write", "WCB write requests"),
                            (0.18, "wcb_close", "WCB close requests")]:
    heights = [averages[(case, "access")][field] for case in ("write_100", "nt_write_control")]
    bars = axes[1].bar([offset, 1 + offset], heights, width=0.36, label=label)
    axes[1].bar_label(bars, fmt="%.0f", padding=3, fontsize=8)
axes[1].set_xticks([0, 1], ["Cached stores", "Non-temporal control"])
axes[1].set_ylim(0, 1250)
axes[1].set_ylabel("Mean events per access phase")
axes[1].set_title("Positive control for WCB events")
axes[1].legend(fontsize=8, loc="upper left")

cases = ["read_1", "write_1", "read_100", "write_100"]
heights = [averages[(case, "evict")]["tsc_ticks"] / 1000 for case in cases]
bars = axes[2].bar(range(4), heights)
axes[2].bar_label(bars, fmt="%.2f", padding=3, fontsize=8)
axes[2].set_xticks(range(4), ["Read 1", "Write 1", "Read 100", "Write 100"], rotation=15)
axes[2].set_ylim(0, 27)
axes[2].set_ylabel("Mean eviction time (thousand TSC ticks)")
axes[2].set_title("Serial traversal of 128 KiB eviction buffer")

for ax in axes:
    ax.set_axisbelow(True)
    ax.grid(axis="y", alpha=0.25)
fig.tight_layout()
fig.savefig(base / "cache_write_phases.png", dpi=200)
plt.close(fig)
