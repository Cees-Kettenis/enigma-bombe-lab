#ifndef BENCHMARK_H
#define BENCHMARK_H
#include "search_worker.h"
#define BENCHMARK_ROWS 16
typedef struct {
    unsigned threads;
    double rate, speedup, efficiency;
} BenchmarkRow;
typedef struct {
    pthread_t thread;
    pthread_mutex_t mutex;
    atomic_bool cancel;
    bool running, started;
    unsigned count, max_threads;
    BenchmarkRow rows[BENCHMARK_ROWS];
} Benchmark;
void benchmark_init(Benchmark *b);
bool benchmark_start(Benchmark *b, unsigned max_threads);
void benchmark_snapshot(Benchmark *b, BenchmarkRow rows[BENCHMARK_ROWS], unsigned *count,
                        bool *running);
void benchmark_destroy(Benchmark *b);
#endif
