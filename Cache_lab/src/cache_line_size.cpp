#include <chrono>
#include <cstdio>
#include <sched.h>

using namespace std;

// 固定数组范围为 512 KiB，大于 L1D 的 48 KiB，小于 L2 的 2 MiB。
const int array_size_kib = 512;
const int array_nums = array_size_kib * 1024 / sizeof(int);

// 步长以字节为单位，在 64 B 附近增加几个测试点。
const int strides_bytes[] = {
    4, 8, 16, 32, 48, 64, 80, 96, 128, 256
};

// 每个步长的访问次数相同，重复测量后取平均。
const int access_num = 2000000;
const int repeats = 10;

int main() {
    // 绑定到逻辑 CPU 0，与第一个实验保持一致。
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(0, &mask);
    sched_setaffinity(0, sizeof(mask), &mask);

    // 按 64 字节对齐；volatile 保证循环中的数组读取不会被删除。
    alignas(64) static volatile int buffer[array_nums];

    // 提前写入整个数组，初始化不计入访问时间。
    for (int i = 0; i < array_nums; ++i) {
        buffer[i] = i;
    }

    FILE* csv = fopen("results/cache_line_size.csv", "w");
    fprintf(csv, "stride_bytes,array_size_kib,avg_ns_per_access\n");

    for (int stride_bytes : strides_bytes) {
        // 将字节步长换算为 int 数组的下标间隔。
        int stride_int = stride_bytes / sizeof(int);
        double total_ns = 0.0;

        for (int repeat = 0; repeat < repeats; ++repeat) {
            // 按当前步长预热两遍，不计时。
            for (int warmup = 0; warmup < 2; ++warmup) {
                for (int index = 0; index < array_nums; index += stride_int) {
                    // 读取该位置的数据，丢弃读取结果。
                    (void)buffer[index];
                }
            }

            int index = 0;
            auto start = chrono::steady_clock::now();

            // 顺序循环访问，走到数组范围之外时返回起点。
            // 固定访问次数，而不是固定遍历数组的次数。
            for (int i = 0; i < access_num; ++i) {
                (void)buffer[index];
                index += stride_int;
                if (index >= array_nums) {
                    index = 0;
                }
            }

            auto end = chrono::steady_clock::now();
            total_ns += chrono::duration<double, nano>(end - start).count();
        }

        // 总耗时除以所有轮次的总访问次数，得到平均每次访问耗时。
        double avg_ns_per_access = total_ns / repeats / access_num;
        fprintf(csv, "%d,%d,%.6f\n",
                stride_bytes, array_size_kib, avg_ns_per_access);
    }

    fclose(csv);
    return 0;
}
