#include "benchmark.h"
#include <string.h>
#include <time.h>
static void *run_benchmark(void *arg) {
    Benchmark *b = arg;
    EnigmaKey k;
    enigma_key_default(&k);
    uint64_t seed = 91234;
    enigma_random_key(&k, false, &seed);
    const char *plain = "WETTERBERICHTDIEQUICKBROWNFOXJUMPSOVERTHELAZYDOGANGRIFFBEIMORGENGRAUEN";
    char cipher[LAB_TEXT_MAX];
    enigma_text(&k, plain, cipher, sizeof cipher);
    SearchSpec spec;
    search_spec_init(&spec, cipher, plain, 0, k.ring, 0);
    spec.limit = 65536;
    double base = 0;
    for (unsigned n = 1; n <= b->max_threads && !atomic_load(&b->cancel);) {
        SearchPool *pool = search_pool_new(n);
        if (!pool)
            break;
        double start = lab_now();
        uint64_t tested = 0;
        do {
            search_pool_start(pool, &spec);
            SearchSnapshot snap;
            search_pool_wait(pool, &b->cancel);
            search_pool_snapshot(pool, &snap, NULL);
            tested += snap.tested;
        } while (lab_now() - start < 0.75 && !atomic_load(&b->cancel));
        double rate = (double)tested / (lab_now() - start);
        search_pool_free(pool);
        if (!atomic_load(&b->cancel)) {
            if (n == 1)
                base = rate;
            pthread_mutex_lock(&b->mutex);
            if (b->count < BENCHMARK_ROWS)
                b->rows[b->count++] = (BenchmarkRow){n, rate, base ? rate / base : 0,
                                                     base ? rate / base / (double)n : 0};
            pthread_mutex_unlock(&b->mutex);
        }
        if (n == b->max_threads)
            break;
        n = n * 2 > b->max_threads ? b->max_threads : n * 2;
    }
    pthread_mutex_lock(&b->mutex);
    b->running = false;
    pthread_mutex_unlock(&b->mutex);
    return NULL;
}
void benchmark_init(Benchmark *b) {
    memset(b, 0, sizeof *b);
    pthread_mutex_init(&b->mutex, NULL);
    atomic_init(&b->cancel, false);
}
bool benchmark_start(Benchmark *b, unsigned max) {
    pthread_mutex_lock(&b->mutex);
    bool running = b->running;
    pthread_mutex_unlock(&b->mutex);
    if (running)
        return false;
    if (b->started)
        pthread_join(b->thread, NULL);
    b->started = false;
    b->count = 0;
    b->max_threads = max > LAB_MAX_WORKERS ? LAB_MAX_WORKERS : max;
    atomic_store(&b->cancel, false);
    b->running = true;
    if (pthread_create(&b->thread, NULL, run_benchmark, b)) {
        b->running = false;
        return false;
    }
    b->started = true;
    return true;
}
void benchmark_snapshot(Benchmark *b, BenchmarkRow rows[BENCHMARK_ROWS], unsigned *count,
                        bool *running) {
    pthread_mutex_lock(&b->mutex);
    *count = b->count;
    *running = b->running;
    memcpy(rows, b->rows, sizeof b->rows);
    pthread_mutex_unlock(&b->mutex);
}
void benchmark_destroy(Benchmark *b) {
    atomic_store(&b->cancel, true);
    if (b->started)
        pthread_join(b->thread, NULL);
    pthread_mutex_destroy(&b->mutex);
}
