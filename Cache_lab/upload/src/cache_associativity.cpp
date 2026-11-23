#include <algorithm>
#include <chrono>
#include <cstdio>
#include <random>
#include <sched.h>
#include <vector>

using namespace std;

// L1 DCache 容量为 32 KiB。
// 以整个 L1 DCache 容量为步长，使访问点映射到同一个 L1 缓存组。
const int stride_bytes = 32 * 1024;
const int stride_int = stride_bytes / sizeof(int);

// 访问点数从 1 增加到 16，数组大小为 16 × 32 KiB = 512 KiB。
const int max_points = 16;
const int array_nums = max_points * stride_int;

// 每轮执行相同次数的访问，重复测量后取平均。
const int access_num = 2000000;
const int repeats = 10;
const int warmup_num = 65536;

int main() {
    // 绑定到逻辑 CPU 0，与前两个实验保持一致。
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(0, &mask);
    sched_setaffinity(0, sizeof(mask), &mask);

    // 按 64 字节对齐，volatile 保证计时循环中的读取不会被删除。
    alignas(64) static volatile int buffer[array_nums];

    mt19937 rng(20261028);

    FILE* csv = fopen("results/cache_associativity.csv", "w");
    fprintf(csv, "point_count,stride_bytes,avg_ns_per_access\n");

    // 固定步长，逐渐增加竞争同一缓存组的访问点数。
    for (int point_count = 1; point_count <= max_points; ++point_count) {
        // 访问点的字节偏移为 0、32 KiB、64 KiB……
        // position 中保存的是对应的 int 数组下标。
        vector<int> position(point_count);
        for (int i = 0; i < point_count; ++i) {
            position[i] = i * stride_int;
        }

        // 打乱访问顺序，减弱规律性预取的影响。
        // 选出的地址不变，它们仍然映射到同一个缓存组。
        shuffle(position.begin(), position.end(), rng);

        // 每个访问点保存下一个访问点的下标，最后一个点连接回首点。
        for (int i = 0; i < point_count; ++i) {
            buffer[position[i]] = position[(i + 1) % point_count];
        }

        double total_ns = 0.0;

        for (int repeat = 0; repeat < repeats; ++repeat) {
            int index = position[0];

            // 预热，不计入测量时间。
            for (int i = 0; i < warmup_num; ++i) {
                index = buffer[index];
            }

            auto start = chrono::steady_clock::now();

            // 后一次访问依赖前一次读取的结果。
            // 不同访问点数的实验均执行 access_num 次读取。
            for (int i = 0; i < access_num; ++i) {
                index = buffer[index];
            }

            auto end = chrono::steady_clock::now();

            total_ns +=
                chrono::duration<double, nano>(end - start).count();
        }

        // 所有轮次的总耗时除以总访问次数。
        double avg_ns_per_access = total_ns / repeats / access_num;

        fprintf(csv, "%d,%d,%.6f\n",
                point_count, stride_bytes, avg_ns_per_access);
    }

    fclose(csv);
    return 0;
}