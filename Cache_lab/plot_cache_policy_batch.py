import csv
from pathlib import Path

import matplotlib.pyplot as plt

base = Path(__file__).resolve().parent
rows = []
for name in ("cache_policy_batch.csv", "cache_policy_tree.csv"):
    with (base / "results" / name).open(newline="") as file:
        rows.extend(csv.DictReader(file))

data = {
    (row["case"], int(row["probe"])): float(row["mean_tsc_ticks_per_op"])
    for row in rows
}
scenarios = ["fill_only", "refresh_oldest", "frequent_then_oldest",
             "refresh_0_4", "refresh_0_4_2", "refresh_0_4_2_6"]
values = [[data[(case, a)] for a in range(9)] for case in scenarios]

# 直接绘制实测时间，避免将时间阈值误解释为真实缺失率。
fig, ax = plt.subplots(figsize=(12, 5))
heatmap = ax.imshow(values, cmap="YlOrRd", vmin=4, vmax=9, aspect="auto")
ax.set_xticks(range(9), [f"A{a}" for a in range(9)])
ax.set_yticks(range(6), scenarios)
ax.set_xlabel("Probed cache line")
ax.set_title("Replacement policy: dependent probes across 32 cache sets")
for i, row in enumerate(values):
    for j, value in enumerate(row):
        ax.text(j, i, f"{value:.2f}", ha="center", va="center",
                color="white" if value > 7.5 else "black")
fig.colorbar(heatmap, ax=ax, label="Mean TSC ticks per probe")
fig.tight_layout()
fig.savefig(base / "cache_replacement_batch.png", dpi=200)
plt.close(fig)

fig, ax = plt.subplots(figsize=(8, 5))
cases = ["store_l1_hit", "store_l2_reference", "store_flushed"]
heights = [data[(case, -1)] for case in cases]
bars = ax.bar(["L1 hit", "L1 miss / L2 reference", "All levels flushed"], heights)
ax.bar_label(bars, fmt="%.2f", padding=4)
ax.axhline(data[("store_fence_baseline", -1)], color="crimson", linestyle="--",
           label="Fence-loop baseline")
ax.set_ylabel("Mean TSC ticks per store + MFENCE")
ax.set_title("Batched store timing: 32 stores, MFENCE after each")
ax.set_ylim(0, 350)
ax.set_axisbelow(True)
ax.grid(axis="y", alpha=0.3)
ax.legend()
fig.tight_layout()
fig.savefig(base / "cache_write_policy_batch.png", dpi=200)
plt.close(fig)
