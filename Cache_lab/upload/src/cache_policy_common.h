#pragma once

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <sched.h>
#include <vector>
#include <x86intrin.h>

// 使用前三个实验测得的服务器 L1 DCache 参数。
constexpr int l1_bytes = 32 * 1024;
constexpr int line_bytes = 64;
constexpr int ways = 8;
constexpr int sample_count = 2000;

// 128 KiB 大于 L1、小于 L2，用来将目标数据从 L1 驱逐到 L2。
constexpr int eviction_bytes = 128 * 1024;
alignas(64) static volatile uint64_t eviction_buffer[
    eviction_bytes / sizeof(uint64_t)
];

inline void initialize_policy_experiment() {
    cpu_set_t mask;
    CPU_ZERO(&mask);
    CPU_SET(0, &mask);
    sched_setaffinity(0, sizeof(mask), &mask);

    // 实际写入每条缓存行，提前建立页面映射。
    for (int i = 0; i < eviction_bytes / sizeof(uint64_t);
         i += line_bytes / sizeof(uint64_t)) {
        eviction_buffer[i] = i;
    }
    _mm_mfence();
}

inline void read_line(volatile uint64_t* address) {
    (void)*address;
    // 保持实验中各次读取的先后顺序。
    _mm_lfence();
}

inline void flush_line(volatile uint64_t* address) {
    _mm_clflush((const void*)address);
}

inline void evict_l1() {
    for (int i = 0; i < eviction_bytes / sizeof(uint64_t);
         i += line_bytes / sizeof(uint64_t)) {
        (void)eviction_buffer[i];
    }
    _mm_lfence();
}

// 计时单位为 TSC tick，不将它当作当前 CPU 核心的时钟周期。
inline uint64_t begin_ticks() {
    _mm_mfence();
    _mm_lfence();
    uint64_t start = __rdtsc();
    _mm_lfence();
    return start;
}

inline uint64_t end_ticks() {
    unsigned int auxiliary;
    uint64_t end = __rdtscp(&auxiliary);
    _mm_lfence();
    return end;
}

inline double mean_ticks(const std::vector<uint64_t>& samples) {
    return std::accumulate(samples.begin(), samples.end(), 0.0) / samples.size();
}

inline double median_ticks(std::vector<uint64_t> samples) {
    std::sort(samples.begin(), samples.end());
    int middle = samples.size() / 2;
    return (samples[middle - 1] + samples[middle]) / 2.0;
}

