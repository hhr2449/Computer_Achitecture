import csv
from pathlib import Path

import matplotlib.pyplot as plt


base_dir = Path(__file__).resolve().parent

# 读取缓存行测量结果，并按字节步长从小到大排列。
with (base_dir / "results/cache_line_size.csv").open(
    encoding="utf-8-sig", newline=""
) as file:
    rows = sorted(csv.DictReader(file), key=lambda row: int(row["stride_bytes"]))

strides = [int(row["stride_bytes"]) for row in rows]
times = [float(row["avg_ns_per_access"]) for row in rows]
array_size_kib = int(rows[0]["array_size_kib"])

plt.figure(figsize=(11, 6))

# 横轴为访问步长，纵轴为平均每次访问耗时。
plt.plot(
    strides,
    times,
    marker="o",
    markersize=5,
    linewidth=1.5,
    label=f"Array size: {array_size_kib} KiB",
)

# 标出已知缓存行大小，方便与曲线的转折位置对照。
plt.axvline(
    x=64,
    color="crimson",
    linestyle="--",
    linewidth=1.5,
    label="Reported cache line: 64 B",
)

# 使用对数横轴展开小步长区间，刻度显示实际的字节数。
plt.xscale("log", base=2)
plt.xticks(strides, [str(stride) for stride in strides])
plt.ylim(bottom=0)

plt.xlabel("Stride (B)")
plt.ylabel("Average access time (ns)")
plt.title("Cache line size measurement")
plt.legend()
plt.grid(True, alpha=0.3)
plt.tight_layout()

# 图片保存到实验目录，与容量测量图片分开。
plt.savefig(base_dir / "cache_line_size.png", dpi=200)
plt.close()
