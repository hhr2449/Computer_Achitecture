# 编译与运行

环境：Linux x86-64、g++。以下命令均在本目录（`upload`）执行。

## 编译

```bash
mkdir -p build results
for name in cache_size cache_line_size cache_associativity cache_policy_batch cache_write_phases; do
    g++ -std=c++17 -O2 "src/$name.cpp" -o "build/$name"
done
g++ -std=c++11 -O0 src/matrix.cpp -o build/matrix_mul
```

## 必做实验

```bash
./build/cache_size
./build/cache_line_size
./build/cache_associativity
./build/matrix_mul
```

矩阵乘法使用 `-O0` 编译，运行时不绑定 CPU；程序自带正确性检查。

## 选做实验

```bash
./build/cache_policy_batch             # 基础替换序列和写入耗时
./build/cache_policy_batch tree        # 补充 Tree-PLRU 序列
sudo ./build/cache_write_phases        # 分阶段写入与淘汰实验
sudo ./build/cache_write_phases control # 非临时写入对照
```

写策略补充实验使用 AMD Zen 2 的 PMU 事件，需要运行环境开放对应硬件计数器，并使用 `sudo` 获取访问权限。

CSV 写入 `results/`，覆盖同名文件；矩阵乘法结果输出到终端。
