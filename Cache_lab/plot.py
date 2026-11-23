import csv
from collections import defaultdict
from itertools import dropwhile
from pathlib import Path
import matplotlib.pyplot as plt

# 按步长分组：
# data[步长] = [(数组大小, 平均访问时间), ...]
data = defaultdict(list)

base_dir = Path(__file__).resolve().parent
with (base_dir / "results/capacity.csv").open(encoding="utf-8-sig", newline="") as file:
    reader = csv.DictReader(dropwhile(lambda line: not line.strip(), file))

    for row in reader:
        stride = int(row["stride_bytes"])
        size = int(row["array_size_kib"])
        time = float(row["avg_ns_per_access"])

        data[stride].append((size, time))

plt.figure(figsize=(11, 6))

# 每种步长画一条曲线
for stride, points in sorted(data.items()):
    # 按数组大小从小到大排列
    points.sort()

    sizes = [point[0] for point in points]
    times = [point[1] for point in points]

    plt.plot(
        sizes,
        times,
        marker="o",
        markersize=4,
        linewidth=1.5,
        label=f"{stride} B"
    )

# 数组大小跨度较大，横轴使用以 2 为底的对数刻度
plt.xscale("log", base=2)

# 将横轴刻度显示为实际的 KiB 数值
ticks = [
    4, 8, 16, 32, 64, 128, 256,
    512, 1024, 2048, 4096, 6144, 8192
]

plt.xticks(ticks, [str(value) for value in ticks], rotation=45)

plt.xlabel("Array size (KiB)")
plt.ylabel("Average access time (ns)")
plt.title("Cache capacity measurement")
# 图例同时包含各步长曲线和缓存容量参考线
plt.legend()
plt.grid(True, alpha=0.3)
# 标出系统报告的 L1D、L2 容量
plt.axvline(
    x=32,
    color="crimson",
    linestyle="--",
    linewidth=1.5,
    label="L1D: 48 KiB"
)

plt.axvline(
    x=512,
    color="black",
    linestyle="--",
    linewidth=1.5,
    label="L2: 3072 KiB"
)
plt.tight_layout()

# 保存图片，再显示窗口
plt.savefig(base_dir / "capacity.png", dpi=200)
