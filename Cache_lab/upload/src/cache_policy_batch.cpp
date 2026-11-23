#include <cstdio>
#include <cstring>
#include <random>

#include "cache_policy_common.h"

// 在 32 个不同缓存组中重复同一种状态，一次串行读取 32 个探测点。
// 同组的地址仍相隔 32 KiB；不同组相隔 128 B，留出未使用的组。
constexpr int groups = 32;
alignas(4096) static volatile uint64_t data[
    (ways + 1) * l1_bytes / sizeof(uint64_t)
];
int layout[ways + 1];
int group_order[groups];
volatile uintptr_t sink;

volatile uint64_t* node(int group, int item) {
    return data + (layout[item] * l1_bytes + group * 128) / sizeof(uint64_t);
}

// 每个候选行各自组成跨组的指针环，下一次读取依赖上一次的结果。
__attribute__((noinline)) uint64_t probe_chain(int item) {
    volatile uint64_t* p = node(group_order[0], item);
    uint64_t start = begin_ticks();
    for (int i = 0; i < groups; ++i) {
        p = reinterpret_cast<volatile uint64_t*>(*p);
    }
    uint64_t elapsed = end_ticks() - start;
    sink = reinterpret_cast<uintptr_t>(p);
    return elapsed;
}

void prepare(int scenario) {
    for (int g : group_order) {
        for (int a = 0; a <= ways; ++a) flush_line(node(g, a));
    }
    _mm_mfence();
    for (int g : group_order) {
        for (int a = 0; a < ways; ++a) read_line(node(g, a));
        if (scenario == 1) {
            read_line(node(g, 0));
        } else if (scenario == 2) {
            for (int k = 0; k < 16; ++k) read_line(node(g, 0));
            for (int a = 1; a < ways; ++a) read_line(node(g, a));
        } else if (scenario >= 3) {
            // 逐层改变 PLRU 树的选择方向。
            read_line(node(g, 0));
            read_line(node(g, 4));
            if (scenario >= 4) read_line(node(g, 2));
            if (scenario >= 5) read_line(node(g, 6));
        }
        read_line(node(g, ways));
    }
}

void save(FILE* csv, const char* name, int probe,
          const std::vector<uint64_t>& samples) {
    fprintf(csv, "%s,%d,%d,%.6f,%.6f\n", name, probe, groups,
            mean_ticks(samples) / groups, median_ticks(samples) / groups);
    printf("%-26s A%d mean %.3f, median %.3f ticks/op\n", name, probe,
           mean_ticks(samples) / groups, median_ticks(samples) / groups);
}

int main(int argc, char** argv) {
    // 默认模式运行前三条序列和读写校准；tree 模式运行后三条序列。
    bool tree = argc > 1 && strcmp(argv[1], "tree") == 0;
    initialize_policy_experiment();
    std::mt19937 rng(20261029);
    std::iota(layout, layout + ways + 1, 0);
    std::iota(group_order, group_order + groups, 0);
    std::shuffle(layout, layout + ways + 1, rng);
    std::shuffle(group_order, group_order + groups, rng);
    for (int a = 0; a <= ways; ++a) {
        for (int i = 0; i < groups; ++i) {
            *node(group_order[i], a) = reinterpret_cast<uintptr_t>(
                node(group_order[(i + 1) % groups], a));
        }
    }
    _mm_mfence();

    FILE* csv = fopen(tree ? "results/cache_policy_tree.csv"
                          : "results/cache_policy_batch.csv", "w");
    fprintf(csv, "case,probe,operations,mean_tsc_ticks_per_op,median_tsc_ticks_per_op\n");
    const char* references[] = {"load_l1_reference", "load_l2_reference"};
    for (int state = 0; state < (tree ? 0 : 2); ++state) {
        std::vector<uint64_t> samples;
        samples.reserve(sample_count);
        for (int r = 0; r < sample_count; ++r) {
            for (int g : group_order) read_line(node(g, ways));
            if (state == 1) evict_l1();
            samples.push_back(probe_chain(ways));
        }
        save(csv, references[state], ways, samples);
    }

    const char* scenarios[] = {"fill_only", "refresh_oldest", "frequent_then_oldest",
        "refresh_0_4", "refresh_0_4_2", "refresh_0_4_2_6"};
    for (int scenario = tree ? 3 : 0; scenario < (tree ? 6 : 3); ++scenario) {
        for (int a = 0; a <= ways; ++a) {
            std::vector<uint64_t> samples;
            samples.reserve(sample_count);
            for (int r = 0; r < sample_count; ++r) {
                prepare(scenario);
                samples.push_back(probe_chain(a));
            }
            save(csv, scenarios[scenario], a, samples);
        }
    }

    // 写实验使用 32 条属于不同组的行。每次写后执行 MFENCE，
    // 一批只计时一次，减少时间戳开销对写命中/缺失差异的掩盖。
    volatile uint64_t* targets[groups];
    for (int i = 0; i < groups; ++i) targets[i] = node(group_order[i], ways) + 1;
    const char* write_cases[] = {"store_fence_baseline", "store_l1_hit",
        "store_l2_reference", "store_flushed"};
    for (int state = 0; state < (tree ? 0 : 4); ++state) {
        std::vector<uint64_t> samples;
        samples.reserve(sample_count);
        for (int r = 0; r < sample_count; ++r) {
            for (auto p : targets) read_line(p);
            if (state == 2) evict_l1();
            if (state == 3) {
                for (auto p : targets) flush_line(p);
                _mm_mfence();
            }
            uint64_t start = begin_ticks();
            for (int i = 0; i < groups; ++i) {
                if (state != 0) *targets[i] = r + 1;
                _mm_mfence();
            }
            samples.push_back(end_ticks() - start);
        }
        save(csv, write_cases[state], -1, samples);
    }
    fclose(csv);
}
