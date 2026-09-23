#include "search_worker.h"
#include "blind.h"
#include <float.h>
#include <stdalign.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#define CHUNK 64
/* Snapshots use a per-worker mutex only once per chunk, never per cipher letter. */
typedef struct {
    alignas(64) pthread_t thread;
    pthread_mutex_t mutex;
    WorkerSnapshot snapshot;
    struct SearchPool *pool;
    unsigned id;
} Worker;
struct SearchPool {
    pthread_mutex_t mutex;
    pthread_cond_t condition;
    Worker *workers;
    unsigned count, pending, generation;
    bool shutdown, running;
    atomic_bool cancel, paused;
    atomic_uint_fast64_t next;
    SearchSpec spec;
    double started, ended;
    Candidate queue[LAB_QUEUE_SIZE];
    unsigned head, used;
    uint64_t dropped;
    double best_language_score;
    atomic_uint completed_orders[60 * 17576];
};
double lab_now(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec / 1e9;
}
unsigned lab_cpu_count(void) {
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? (unsigned)n : 1;
}
static bool found(const Candidate *c, void *data) {
    Worker *w = data;
    SearchPool *p = w->pool;
    pthread_mutex_lock(&p->mutex);
    if (c->heuristic) {
        if (c->score <= p->best_language_score) {
            pthread_mutex_unlock(&p->mutex);
            return !atomic_load(&p->cancel);
        }
        p->best_language_score = c->score;
        /* Preserve new best guesses even when the GUI has not drained the queue. */
        if (p->used == LAB_QUEUE_SIZE) {
            p->head = (p->head + 1) % LAB_QUEUE_SIZE;
            p->used--;
            p->dropped++;
        }
    }
    if (p->used < LAB_QUEUE_SIZE) {
        unsigned i = (p->head + p->used) % LAB_QUEUE_SIZE;
        p->queue[i] = *c;
        p->queue[i].worker = w->id;
        p->queue[i].elapsed = lab_now() - p->started;
        p->used++;
    } else
        p->dropped++;
    pthread_mutex_unlock(&p->mutex);
    return !atomic_load(&p->cancel);
}
static void *worker_main(void *data) {
    Worker *w = data;
    SearchPool *p = w->pool;
    unsigned seen = 0;
    for (;;) {
        pthread_mutex_lock(&p->mutex);
        while (!p->shutdown && seen == p->generation)
            pthread_cond_wait(&p->condition, &p->mutex);
        if (p->shutdown) {
            pthread_mutex_unlock(&p->mutex);
            return NULL;
        }
        seen = p->generation;
        pthread_mutex_unlock(&p->mutex);
        WorkerSnapshot snap = {.active = true};
        pthread_mutex_lock(&w->mutex);
        w->snapshot = snap;
        pthread_mutex_unlock(&w->mutex);
        uint64_t total = bombe_total(&p->spec);
        for (;;) {
            pthread_mutex_lock(&p->mutex);
            while (atomic_load(&p->paused) && !atomic_load(&p->cancel))
                pthread_cond_wait(&p->condition, &p->mutex);
            pthread_mutex_unlock(&p->mutex);
            if (atomic_load(&p->cancel))
                break;
            unsigned chunk = p->spec.blind ? 1 : CHUNK;
            uint64_t begin = atomic_fetch_add(&p->next, chunk);
            if (begin >= total)
                break;
            uint64_t end = begin + chunk;
            if (end > total)
                end = total;
            unsigned order = (unsigned)(begin / 17576), finished = 0;
            for (uint64_t i = begin; i < end; i++) {
                if (atomic_load(&p->cancel))
                    break;
                KernelResult r = p->spec.blind
                                     ? blind_test_state(&p->spec, i, found, w, &p->cancel)
                                     : bombe_test_state(&p->spec, i, found, w, &p->cancel);
                /* A cancelled state can already have published valid stops. Count those
                 * even though the rotor state itself was not searched to completion. */
                snap.state = i;
                snap.stops += r.stops;
                snap.truncated += r.truncated;
                if (atomic_load(&p->cancel))
                    break;
                snap.tested++;
                snap.rejected += r.stops == 0;
                unsigned current = (unsigned)(i / 17576);
                if (current != order) {
                    atomic_fetch_add_explicit(&p->completed_orders[order], finished,
                                              memory_order_relaxed);
                    order = current;
                    finished = 0;
                }
                finished++;
            }
            atomic_fetch_add_explicit(&p->completed_orders[order], finished, memory_order_relaxed);
            pthread_mutex_lock(&w->mutex);
            w->snapshot = snap;
            pthread_mutex_unlock(&w->mutex);
        }
        snap.active = false;
        pthread_mutex_lock(&w->mutex);
        w->snapshot = snap;
        pthread_mutex_unlock(&w->mutex);
        pthread_mutex_lock(&p->mutex);
        if (--p->pending == 0) {
            p->running = false;
            p->ended = lab_now();
            pthread_cond_broadcast(&p->condition);
        }
        pthread_mutex_unlock(&p->mutex);
    }
}
SearchPool *search_pool_new(unsigned count) {
    if (!count || count > LAB_MAX_WORKERS)
        return NULL;
    SearchPool *p = calloc(1, sizeof *p);
    if (!p)
        return NULL;
    p->workers = aligned_alloc(64, ((sizeof(Worker) * count + 63) / 64) * 64);
    if (!p->workers) {
        free(p);
        return NULL;
    }
    memset(p->workers, 0, sizeof(Worker) * count);
    pthread_mutex_init(&p->mutex, NULL);
    pthread_condattr_t attr;
    pthread_condattr_init(&attr);
    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    pthread_cond_init(&p->condition, &attr);
    pthread_condattr_destroy(&attr);
    atomic_init(&p->cancel, false);
    atomic_init(&p->paused, false);
    atomic_init(&p->next, 0);
    for (unsigned i = 0; i < 60 * 17576; i++)
        atomic_init(&p->completed_orders[i], 0);
    for (unsigned i = 0; i < count; i++) {
        Worker *w = &p->workers[i];
        w->pool = p;
        w->id = i;
        pthread_mutex_init(&w->mutex, NULL);
        if (pthread_create(&w->thread, NULL, worker_main, w)) {
            pthread_mutex_destroy(&w->mutex);
            search_pool_free(p);
            return NULL;
        }
        p->count++;
    }
    return p;
}
void search_pool_stop(SearchPool *p) {
    if (!p)
        return;
    atomic_store(&p->cancel, true);
    pthread_mutex_lock(&p->mutex);
    pthread_cond_broadcast(&p->condition);
    pthread_mutex_unlock(&p->mutex);
}
void search_pool_wait(SearchPool *p, const atomic_bool *cancel) {
    pthread_mutex_lock(&p->mutex);
    while (p->running) {
        if (cancel && atomic_load(cancel)) {
            atomic_store(&p->cancel, true);
            pthread_cond_broadcast(&p->condition);
        }
        struct timespec deadline;
        clock_gettime(CLOCK_MONOTONIC, &deadline);
        deadline.tv_nsec += 10000000;
        if (deadline.tv_nsec >= 1000000000) {
            deadline.tv_sec++;
            deadline.tv_nsec -= 1000000000;
        }
        pthread_cond_timedwait(&p->condition, &p->mutex, &deadline);
    }
    pthread_mutex_unlock(&p->mutex);
}
void search_pool_free(SearchPool *p) {
    if (!p)
        return;
    search_pool_stop(p);
    pthread_mutex_lock(&p->mutex);
    p->shutdown = true;
    pthread_cond_broadcast(&p->condition);
    pthread_mutex_unlock(&p->mutex);
    for (unsigned i = 0; i < p->count; i++) {
        pthread_join(p->workers[i].thread, NULL);
        pthread_mutex_destroy(&p->workers[i].mutex);
    }
    pthread_cond_destroy(&p->condition);
    pthread_mutex_destroy(&p->mutex);
    free(p->workers);
    free(p);
}
bool search_pool_start(SearchPool *p, const SearchSpec *s) {
    pthread_mutex_lock(&p->mutex);
    if (p->running) {
        pthread_mutex_unlock(&p->mutex);
        return false;
    }
    p->spec = *s;
    p->head = p->used = 0;
    p->dropped = 0;
    p->best_language_score = -DBL_MAX;
    p->started = lab_now();
    p->ended = 0;
    p->pending = p->count;
    for (unsigned i = 0; i < p->count; i++) {
        pthread_mutex_lock(&p->workers[i].mutex);
        memset(&p->workers[i].snapshot, 0, sizeof(WorkerSnapshot));
        pthread_mutex_unlock(&p->workers[i].mutex);
    }
    unsigned orders = s->advanced ? 60 * 17576 : 60;
    for (unsigned i = 0; i < orders; i++)
        atomic_store(&p->completed_orders[i], 0);
    atomic_store(&p->next, 0);
    atomic_store(&p->cancel, false);
    atomic_store(&p->paused, false);
    p->running = true;
    p->generation++;
    pthread_cond_broadcast(&p->condition);
    pthread_mutex_unlock(&p->mutex);
    return true;
}
void search_pool_pause(SearchPool *p, bool pause) {
    if (!p)
        return;
    atomic_store(&p->paused, pause);
    pthread_mutex_lock(&p->mutex);
    pthread_cond_broadcast(&p->condition);
    pthread_mutex_unlock(&p->mutex);
}
void search_pool_snapshot(SearchPool *p, SearchSnapshot *out, WorkerSnapshot *workers) {
    memset(out, 0, sizeof *out);
    if (!p)
        return;
    pthread_mutex_lock(&p->mutex);
    out->running = p->running;
    out->paused = atomic_load(&p->paused);
    out->total = bombe_total(&p->spec);
    out->elapsed = p->started ? ((p->running ? lab_now() : p->ended) - p->started) : 0;
    out->threads = p->count;
    out->dropped = p->dropped;
    unsigned orders = p->spec.advanced ? 60 * 17576 : 60;
    for (unsigned i = 0; i < orders; i++)
        out->orders_completed +=
            atomic_load_explicit(&p->completed_orders[i], memory_order_relaxed) == 17576;
    for (unsigned i = 0; i < p->count; i++) {
        pthread_mutex_lock(&p->workers[i].mutex);
        WorkerSnapshot s = p->workers[i].snapshot;
        pthread_mutex_unlock(&p->workers[i].mutex);
        if (workers)
            workers[i] = s;
        out->tested += s.tested;
        out->rejected += s.rejected;
        out->stops += s.stops;
        out->truncated += s.truncated;
    }
    pthread_mutex_unlock(&p->mutex);
}
bool search_pool_pop(SearchPool *p, Candidate *out) {
    if (!p)
        return false;
    pthread_mutex_lock(&p->mutex);
    bool ok = p->used > 0;
    if (ok) {
        *out = p->queue[p->head];
        p->head = (p->head + 1) % LAB_QUEUE_SIZE;
        p->used--;
    }
    pthread_mutex_unlock(&p->mutex);
    return ok;
}
