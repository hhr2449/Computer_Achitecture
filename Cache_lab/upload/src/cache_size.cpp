#define _GNU_SOURCE
#include <sched.h>
#include <sys/mman.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <vector>

using namespace std;

const int sizes_kib[] = {
    4, 8, 16, 24, 32, 48, 64, 96,
    128, 256, 384, 448, 512, 576, 768, 1024, 1536, 1792, 2048, 2304, 2560, 3072, 3584, 4096, 6144, 8192
};

const int strides_bytes[] = {
    32, 64, 128, 256, 512
};

const int access_num = 2000000;
const int repeats = 10;

int main() {
    // 绑定到 Core 0
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(0, &mask);
    if (sched_setaffinity(0, sizeof(mask), &mask) < 0) {
        perror("sched_setaffinity");
    }

    // 分配 64MB 内存，严格按 2MB (2 * 1024 * 1024) 边界对齐以激活透明大页
    const size_t BUF_BYTES = 64ULL * 1024 * 1024;
    void* raw_ptr = nullptr;
    if (posix_memalign(&raw_ptr, 2 * 1024 * 1024, BUF_BYTES) != 0) {
        perror("posix_memalign");
        return 1;
    }

    // 申请透明大页并立刻写入，强制触发 2MB 大页缺页中断
    madvise(raw_ptr, BUF_BYTES, MADV_HUGEPAGE);
    memset(raw_ptr, 0, BUF_BYTES);

    volatile int* buffer = reinterpret_cast<volatile int*>(raw_ptr);

    mt19937 rng(20261028);

    FILE* csv = fopen("results/capacity.csv", "w");
    if (!csv) {
        perror("fopen results/capacity.csv");
        return 1;
    }
    fprintf(csv, "stride_bytes,array_size_kib,avg_ns_per_access\n");

    for (size_t i = 0; i < sizeof(strides_bytes) / sizeof(strides_bytes[0]); i++) {
        int stride = strides_bytes[i];
        int stride_int = stride / sizeof(int);

        for (size_t j = 0; j < sizeof(sizes_kib) / sizeof(sizes_kib[0]); j++) {
            size_t size_kib = sizes_kib[j];
            size_t array_nums = size_kib * 1024 / sizeof(int);
            size_t point_nums = (array_nums + stride_int - 1) / stride_int;

            vector<int> position(point_nums);
            for (size_t l = 0; l < point_nums; l++) {
                position[l] = l * stride_int;
            }

            // 全局随机打乱，构成单环 Pointer Chasing
            shuffle(position.begin(), position.end(), rng);

            for (size_t h = 0; h < point_nums - 1; h++) {
                buffer[position[h]] = position[h + 1];
            }
            buffer[position[point_nums - 1]] = position[0];

            int index = position[0];
            double total_ns_per_access = 0.0;

            for (int repeat = 0; repeat < repeats; repeat++) {
                int warmup = max<int>(point_nums * 2, 65536);
                for (int w = 0; w < warmup; ++w) {
                    index = buffer[index];
                }

                auto start = std::chrono::steady_clock::now();
                for (int a = 0; a < access_num; ++a) {
                    index = buffer[index];
                }
                auto end = std::chrono::steady_clock::now();

                double cur_ns = std::chrono::duration<double, std::nano>(end - start).count() / access_num;
                total_ns_per_access += cur_ns;
            }

            // 对多次实验的单次访问耗时取算术平均值
            double avg_ns_per_access = total_ns_per_access / repeats;

            // 防止编译器优化掉 index
            if (index == -1) printf("%d\n", index);

            fprintf(csv, "%d,%zu,%.6f\n", stride, size_kib, avg_ns_per_access);
            
        }
    }

    fclose(csv);
    free(raw_ptr);
    return 0;
}
