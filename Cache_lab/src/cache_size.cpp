#include <algorithm>
#include <chrono>
#include <cstdio>
#include <random>
#include <vector>
using namespace std;

// 取点的array范围
// 读取系统信息知道L1d为48k，L2为3072k，所以在这两个周围安排更密集的测试点
const int sizes_kib[] = {
    4, 8, 16, 24, 32, 64,
    128, 256, 512, 1024, 
    2048, 4096, 8192
};
// 测试步长序列
const int strides_bytes[] = {
    32, 64, 128, 256, 512, 1024
};

// 每次测量都重复相同的缓存访问次数
const int access_num = 5000000;
// 每次实验重复若干次，再取平均
const int repeats = 10;

int main() {
    // 创建数组，大小为16m
    // 使用alignas(64)进行64字节对齐
    alignas(64) static volatile int buffer[
        16 * 1024 * 1024 / sizeof(int)
    ];

    // 随机数生成器
    mt19937 rng(20261028);

    // 创建 CSV 文件，w 表示每次运行重新写入
    FILE* csv = fopen("results/capacity.csv", "w");

    // 写入表头
    fprintf(csv, "stride_bytes,array_size_kib,avg_ns_per_access\n");

    // 遍历每个步长
    for (int i = 0; i < sizeof(strides_bytes) / sizeof(strides_bytes[0]); i++) {
        // 固定步长，遍历array范围
        int stride = strides_bytes[i];
        // 转换为int下标间隔
        int stride_int = stride / sizeof(int);
        for (int j = 0; j < sizeof(sizes_kib) / sizeof(sizes_kib[0]); j++) {
            // 访问的数组范围大小，单位是k
            int size = sizes_kib[j];
            // 转换为int数组元素个数
            int array_nums = size * 1024 / sizeof(int);
            // array_nums个元素，按照间隔stride_int选取，最终选取的个数应该是(array_nums + stride_int - 1) / stride_int
            int point_nums = (array_nums + stride_int - 1) / stride_int;
            // 将选取的访问点的数组下标写入数组中
            vector<int> position(point_nums);
            int access_point = 0;
            for (int l = 0; l < point_nums; l++) {
                position[l] = access_point;
                access_point += stride_int;
            }
            // 然后将其打乱，打乱后的数组中的下标顺序就是访问顺序
            // 即依次访问buffer[pos[0]]、buffer[pos[1]]、…………
            shuffle(position.begin(), position.end(), rng);

            // 为了让前后的访问互相依赖，所以令buffer[pos[k]]的值为pos[k+1]，这样对后一个点的访问依赖于对前一个点的访问
            for(int h = 0; h < point_nums - 1; h++) {
                buffer[position[h]] = position[h + 1];
            }
            buffer[position[point_nums - 1]] = position[0];

            // 开始进行访问测试
            // 访问位置索引
            int index = position[0];
            double total_ns = 0;
            // 重复测试repeats次
            for (int repeat = 0; repeat < repeats; repeat++) {
                // 先预热
                int warmup = max(point_nums * 2, 65536);

                for (int i = 0; i < warmup; ++i) {
                    index = buffer[index];
                }

                // 然后访问access_num次
                auto start = std::chrono::steady_clock::now();

                for (int i = 0; i < access_num; ++i) {
                    index = buffer[index];
                }

                auto end = std::chrono::steady_clock::now();

                // 总耗时转换为纳秒，再除以访问次数。
                total_ns +=
                    std::chrono::duration<double, std::nano>(
                        end - start
                    ).count();

            }
            // 一共访问 repeats * access_num 次，计算平均每次访问耗时
            double avg_ns_per_access = total_ns / repeats / access_num;

            // 每组参数只输出一行
            fprintf(csv, "%d,%d,%.6f\n",
                    stride,
                    size,
                    avg_ns_per_access);

        }
        
    }
    fclose(csv);
    return 0;
}