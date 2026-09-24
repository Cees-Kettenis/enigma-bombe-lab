#ifndef SEARCH_WORKER_H
#define SEARCH_WORKER_H
#include "bombe.h"
#include <pthread.h>
#define LAB_MAX_WORKERS 1024
#define LAB_QUEUE_SIZE 256
typedef struct {
    uint64_t tested, rejected, stops, truncated, state;
    bool active;
} WorkerSnapshot;
typedef struct {
    uint64_t tested, rejected, stops, truncated, total, dropped;
    double elapsed;
    unsigned threads, orders_completed;
    bool running, paused;
    double reached_confidence; /* Zero unless a candidate met the requested threshold. */
} SearchSnapshot;
typedef struct SearchPool SearchPool;
SearchPool *search_pool_new(unsigned count);
void search_pool_free(SearchPool *pool);
bool search_pool_start(SearchPool *pool, const SearchSpec *spec);
void search_pool_stop(SearchPool *pool);
void search_pool_pause(SearchPool *pool, bool paused);
/* Blocking completion wait for background coordinators. Never call on the GTK thread. */
void search_pool_wait(SearchPool *pool, const atomic_bool *cancel);
void search_pool_snapshot(SearchPool *pool, SearchSnapshot *out, WorkerSnapshot *workers);
bool search_pool_pop(SearchPool *pool, Candidate *out);
double lab_now(void);
unsigned lab_cpu_count(void);
#endif
