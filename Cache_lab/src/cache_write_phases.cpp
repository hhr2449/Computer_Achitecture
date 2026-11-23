#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <linux/perf_event.h>
#include <random>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "cache_policy_common.h"

// Zen 2 的公开 PMU 事件；这些名称不表示 L1 脏行写回次数。
constexpr uint64_t events[] = {0x0229, 0x4860, 0x4063, 0x2063, 0xff60, 0x0143};
constexpr int event_count = 6;
constexpr int target_lines = 128;  // 8 KiB，能够完整放入 L1。
constexpr int eviction_lines = eviction_bytes / line_bytes;
constexpr int trials = 200;
alignas(4096) volatile uint64_t target[target_lines][8];
int order[target_lines];
int eviction_order[eviction_lines];
volatile uint64_t result_sink;

struct Counters {
    uint64_t count, enabled, running, values[event_count];
};

Counters snapshot(int fd) {
    Counters value;
    if (read(fd, &value, sizeof(value)) != sizeof(value)) {
        perror("read perf counters");
        exit(1);
    }
    return value;
}

__attribute__((noinline)) void chase_eviction() {
    volatile uint64_t* p = eviction_buffer + eviction_order[0] * 8;
    for (int i = 0; i < eviction_lines; ++i) {
        p = reinterpret_cast<volatile uint64_t*>(*p);
    }
    result_sink = reinterpret_cast<uintptr_t>(p);
    _mm_lfence();
}

// 将目标行置为干净且驻留于 L1，驱逐缓冲区预先置于较低层缓存。
void prepare() {
    for (int i = 0; i < target_lines; ++i) flush_line(target[i]);
    _mm_mfence();
    chase_eviction();
    for (int i : order) read_line(target[i]);
}

void save(FILE* csv, int trial, const char* name, const char* phase,
          int operations, uint64_t ticks, const Counters& before,
          const Counters& after) {
    uint64_t enabled = after.enabled - before.enabled;
    uint64_t running = after.running - before.running;
    // 必须确认计数器实际运行，不能将不可用的事件当作零次请求。
    if (running == 0) {
        fprintf(stderr, "PMU event group did not run\n");
        exit(1);
    }
    fprintf(csv, "%d,%s,%s,%d,%lu,%lu,%lu", trial, name, phase,
            operations, ticks, enabled, running);
    for (int e = 0; e < event_count; ++e) {
        fprintf(csv, ",%lu", after.values[e] - before.values[e]);
    }
    fprintf(csv, "\n");
}

int main(int argc, char** argv) {
    bool control = argc > 1 && strcmp(argv[1], "control") == 0;
    initialize_policy_experiment();
    std::mt19937 rng(20261030);
    std::iota(order, order + target_lines, 0);
    std::iota(eviction_order, eviction_order + eviction_lines, 0);
    std::shuffle(order, order + target_lines, rng);
    std::shuffle(eviction_order, eviction_order + eviction_lines, rng);
    for (int i = 0; i < eviction_lines; ++i) {
        eviction_buffer[eviction_order[i] * 8] = reinterpret_cast<uintptr_t>(
            eviction_buffer + eviction_order[(i + 1) % eviction_lines] * 8);
    }
    for (int i = 0; i < target_lines; ++i) target[i][0] = i;
    _mm_mfence();

    int fds[event_count];
    for (int e = 0; e < event_count; ++e) {
        perf_event_attr attr{};
        attr.type = PERF_TYPE_RAW;
        attr.size = sizeof(attr);
        attr.config = events[e];
        attr.disabled = 1;
        attr.exclude_kernel = 1;
        attr.exclude_hv = 1;
        attr.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_TOTAL_TIME_ENABLED
                         | PERF_FORMAT_TOTAL_TIME_RUNNING;
        fds[e] = syscall(SYS_perf_event_open, &attr, 0, -1,
                         e == 0 ? -1 : fds[0], 0);
        if (fds[e] < 0) {
            perror("perf_event_open: run with sudo on the server");
            return 1;
        }
    }
    ioctl(fds[0], PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);
    FILE* csv = fopen(control ? "results/cache_write_control.csv"
                             : "results/cache_write_phases.csv", "w");
    fprintf(csv, "trial,case,phase,operations,tsc_ticks,time_enabled,time_running,"
                 "store_dispatch,l2_store_or_permission,wcb_write,wcb_close,"
                 "l2_all_requests,l1_refill_from_l2\n");
    const char* names[] = {"read_1", "write_1", "read_100", "write_100",
                          "nt_write_control"};

    // 各组交替测量，避免所有只读和所有写入分布在不同时间段。
    for (int trial = 0; trial < trials; ++trial) {
        for (int offset = 0; offset < (control ? 1 : 4); ++offset) {
            int which = control ? 4 : (trial + offset) % 4;
            int rounds = which == 2 || which == 3 ? 100 : 1;
            prepare();
            if (control) {
                // 正对照：完整非临时写入 128 条冷缓存行，检查 WCB 事件能否计数。
                for (int i = 0; i < target_lines; ++i) flush_line(target[i]);
                _mm_mfence();
            }
            Counters a = snapshot(fds[0]);
            uint64_t start = begin_ticks();
            for (int r = 0; r < rounds; ++r) {
                for (int i : order) {
                    if (control) {
                        for (int k = 0; k < 8; ++k) {
                            _mm_stream_si64((long long*)(target[i] + k), trial + 1);
                        }
                    } else if (which % 2) target[i][0] = trial * 100 + r + 1;
                    else (void)target[i][0];
                }
                _mm_mfence();
            }
            uint64_t write_ticks = end_ticks() - start;
            Counters b = snapshot(fds[0]);
            start = begin_ticks();
            chase_eviction();
            _mm_mfence();
            uint64_t evict_ticks = end_ticks() - start;
            Counters c = snapshot(fds[0]);
            save(csv, trial, names[which], "access", rounds * target_lines,
                 write_ticks, a, b);
            save(csv, trial, names[which], "evict", eviction_lines,
                 evict_ticks, b, c);
        }
    }
    fclose(csv);
    for (int fd : fds) close(fd);
    puts(control ? "Saved results/cache_write_control.csv"
                 : "Saved results/cache_write_phases.csv");
}
