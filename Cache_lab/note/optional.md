# 选做：测试缓存的替换策略和写策略

实验平台为 AMD EPYC 7452 服务器，L1 DCache 参数为 32 KiB、64 B 缓存行、8 路组相联，L2 为 512 KiB。程序固定在逻辑 CPU 0，替换策略和批量计时每种情况采样 2000 次，分阶段计数每种情况采样 200 次，输出平均值和中位数，时间单位为 TSC tick。

## 编译和运行

在服务器的 Cache_lab 目录执行：

```bash
make optional
./build/cache_policy_batch
./build/cache_policy_batch tree
python3 plot_cache_policy_batch.py
sudo ./build/cache_write_phases
sudo ./build/cache_write_phases control
python3 plot_cache_write_phases.py
```

默认模式运行前三条替换序列以及读取参考、写入对照；`tree` 模式运行另外三条 Tree-PLRU 鉴别序列。两种模式分别输出 CSV，绘图时合并读取。

`cache_write_phases` 分别测量 8 KiB 目标数组的访问阶段和 128 KiB 缓冲区的串行驱逐阶段，每组 200 次；`control` 模式测量非临时写入正对照。服务器限制了 PMU 访问权限，因此这两个命令使用 sudo。代码通过系统调用读取计数器，不需要另行安装 perf 命令。

## 测量方法

替换策略实验在 32 个不同缓存组中构造相同访问历史。同组地址间隔 32 KiB，不同组基地址相隔 128 B。每个组内先访问 A0～A7，再按指定顺序刷新部分点，最后访问 A8 触发替换。

计时阶段将同一个候选点在 32 个组中的地址连接成随机指针环，串行追踪 32 次。探测每个候选点前都独立重建状态。六条序列对应的明显变慢点为 A0、A4、A0、A2、A6、A1，与 Tree-PLRU 模型预测一致。

写策略实验比较 L1 命中、L1 缺失但 L2 命中参考、清除各级缓存后的写入。每批写入 32 条不同组的缓存行，每次写后执行 MFENCE，再将整批时间除以 32。冷写入约为 L1 写命中的 5.12 倍。

AMD 手册 56305 第 2.6.2 节明确说明该型号所属处理器的 L1 数据缓存采用 Write-back。实验现象与该写策略相符。完整方法、模型对照、数据和来源见 [report.md](report.md)。

分阶段测量中，目标 Store 数量从 128 增加到 12800 时，L2 写入或权限请求仅从约 18.160 次增加到 19.765 次，没有随写入次数同比增长，与 Write-back 将修改保留在缓存中的行为相符。非临时写入正对照得到 1024 次 WCB 写请求、128 次关闭请求，验证了相关计数器对相应操作的响应。

## 文件

| 文件 | 内容 |
|---|---|
| src/cache_policy_common.h | 硬件参数、计时、缓存状态准备 |
| src/cache_policy_batch.cpp | 六条替换序列及写入对照 |
| results/cache_policy_batch.csv | 前三条序列、读取校准和写入结果 |
| results/cache_policy_tree.csv | 后三条 Tree-PLRU 鉴别序列 |
| plot_cache_policy_batch.py | 绘图程序 |
| cache_replacement_batch.png | 六条序列的探测耗时热力图 |
| cache_write_policy_batch.png | 写入耗时对照图 |
| src/cache_write_phases.cpp | 分阶段计时、PMU 计数及非临时写入正对照 |
| results/cache_write_phases.csv | 分阶段实验的全部原始样本 |
| results/cache_write_control.csv | 非临时写入正对照的原始样本 |
| results/cache_write_phases_summary.csv | 各组的平均值、中位数和计数器运行比例 |
| plot_cache_write_phases.py | 分阶段计数的汇总与绘图 |
| cache_write_phases.png | 分阶段计数和耗时对照图 |
