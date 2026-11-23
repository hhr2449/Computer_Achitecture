import csv
from pathlib import Path

import matplotlib.pyplot as plt


base_dir = Path(__file__).resolve().parent

# 读取实验结果，按访问点数从小到大排列。
with (base_dir / "results/cache_associativity.csv").open(
    encoding="utf-8-sig", newline=""
) as file:
    rows = sorted(
        csv.DictReader(file),
        key=lambda row: int(row["point_count"])
    )

point_counts = [int(row["point_count"]) for row in rows]
times = [float(row["avg_ns_per_access"]) for row in rows]
stride_kib = int(rows[0]["stride_bytes"]) // 1024

plt.figure(figsize=(11, 6))

# 访问点数增加，意味着更多缓存行竞争同一个缓存组。
plt.plot(
    point_counts,
    times,
    marker="o",
    markersize=5,
    linewidth=1.5,
    label=f"Stride: {stride_kib} KiB"
)

# 已知 L1 DCache 为 8 路相联，同一组最多容纳 8 条缓存行。
# 预期在访问点数从 8 增加到 9 时出现延迟上升。
plt.axvline(
    x=8,
    color="crimson",
    linestyle="--",
    linewidth=1.5,
    label="Reported L1D associativity: 8 ways"
)

# 使用线性横轴，显示每一个实际测试的访问点数。
plt.xticks(point_counts)
plt.ylim(bottom=0)

plt.xlabel("Number of access points")
plt.ylabel("Average access time (ns)")
plt.title("L1 DCache associativity measurement")
plt.legend()
plt.grid(True, alpha=0.3)
plt.tight_layout()

# 保存图片到实验目录。
plt.savefig(base_dir / "cache_associativity.png", dpi=200)
plt.close()