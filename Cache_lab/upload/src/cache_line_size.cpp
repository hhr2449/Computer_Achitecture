#include <algorithm>
#include <chrono>
#include <cstdio>
#include <numeric>
#include <random>
#include <sched.h>
#include <vector>

using namespace std;

// 固定数组范围为 256 KiB，大于 L1D 的 32 KiB，小于 L2 的 512 KiB。
const int array_size_kib = 256;
const int array_nums = array_size_kib * 1024 / sizeof(int);

// 每块为 256 字节，块间随机访问，块内按步长顺序访问。
const int block_bytes = 256;
const int block_ints = block_bytes / sizeof(int);
const int block_nums = array_nums / block_ints;

// 步长以字节为单位
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

    // 将块编号打乱一次，各个步长使用相同的块访问顺序。
    vector<int> block_order(block_nums);
    iota(block_order.begin(), block_order.end(), 0);
    mt19937 rng(20261028);
    shuffle(block_order.begin(), block_order.end(), rng);

    FILE* csv = fopen("results/cache_line_size.csv", "w");
    fprintf(csv, "stride_bytes,array_size_kib,avg_ns_per_access\n");

    for (int stride_bytes : strides_bytes) {
        // 将字节步长换算为 int 数组的下标间隔。
        int stride_int = stride_bytes / sizeof(int);
        int points_per_block = (block_ints + stride_int - 1) / stride_int;
        int point_nums = block_nums * points_per_block;

        // 每块从块首开始，按步长选点；只打乱块顺序，保留块内缓存行复用。
        // 例如步长为 32 字节时，块内依次访问偏移 0、32、64……224。
        for (int b = 0; b < block_nums; ++b) {
            int base = block_order[b] * block_ints;
            int next_block = block_order[(b + 1) % block_nums] * block_ints;

            for (int offset = 0; offset < block_ints; offset += stride_int) {
                int next_offset = offset + stride_int;

                // 块内指向下一个点；块尾指向下一个随机块的首点。
                // 最后一个块连接回第一个块，形成完整的访问环。
                buffer[base + offset] =
                    next_offset < block_ints ? base + next_offset : next_block;
            }
        }

        double total_ns = 0.0;

        for (int repeat = 0; repeat < repeats; ++repeat) {
            int index = block_order[0] * block_ints;

            // 沿当前访问环预热两遍，不计时。
            for (int i = 0; i < point_nums * 2; ++i) {
                index = buffer[index];
            }

            auto start = chrono::steady_clock::now();

            // 必须先读出当前点保存的下标，才能访问下一个点。
            // 各步长仍执行相同次数的读取。
            for (int i = 0; i < access_num; ++i) {
                index = buffer[index];
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
