#include "benchmark.h"
#include "plugboard.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "line %d: %s\n", __LINE__, #x);                                        \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)
static const char *plain = "WETTERBERICHTTHEQUICKBROWNFOXJUMPSOVERTHELAZYDOGPACKMYBOXWITHFIVEDOZENL"
                           "IQUORJUGSANGRIFFBEIMORGENGRAUEN";
static bool recovered = false;
static bool verify_partial(const Candidate *c, void *data) {
    const char *crib = data;
    CHECK(!strncmp(c->plaintext, crib, strlen(crib)));
    CHECK(plugboard_valid(c->key.plug));
    unsigned unresolved = 0;
    for (int i = 0; i < 26; i++) {
        if (c->deductions[i] < 0)
            unresolved++;
        else
            CHECK(c->deductions[c->deductions[i]] == i);
    }
    CHECK(unresolved == c->unresolved);
    return true;
}
static bool verify(const Candidate *c, void *unused) {
    (void)unused;
    if (!strcmp(c->plaintext, plain)) {
        CHECK(plugboard_valid(c->key.plug));
        CHECK(c->unresolved == 0);
        CHECK(c->key.order[0] == 0 && c->key.order[1] == 1 && c->key.order[2] == 2);
        CHECK(c->key.start[0] == 0 && c->key.start[1] == 0 && c->key.start[2] == 7);
        recovered = true;
    }
    return true;
}
int main(void) {
    EnigmaKey key;
    enigma_key_default(&key);
    key.start[2] = 7;
    CHECK(plugboard_parse(key.plug, "AV BS CG DL FU HZ IN KM OW RX"));
    char cipher[LAB_TEXT_MAX];
    enigma_text(&key, plain, cipher, sizeof cipher);
    SearchSpec s;
    CHECK(search_spec_init(&s, cipher, plain, 0, key.ring, 0));
    KernelResult result = bombe_test_state(&s, 7, verify, NULL, NULL);
    CHECK(result.stops > 0);
    CHECK(recovered);
    SearchSpec weak;
    const char *short_crib = "WE";
    CHECK(search_spec_init(&weak, cipher, short_crib, 0, key.ring, 0));
    weak.max_stops_per_state = 2;
    result = bombe_test_state(&weak, 7, verify_partial, (void *)short_crib, NULL);
    CHECK(result.stops == 2 && result.truncated);
    /* The search API receives ciphertext and a crib only, never the challenge key. */
    s.limit = 2048;
    SearchPool *pool = search_pool_new(4);
    CHECK(pool);
    CHECK(search_pool_start(pool, &s));
    search_pool_pause(pool, true);
    SearchSnapshot snap;
    search_pool_snapshot(pool, &snap, NULL);
    CHECK(snap.paused);
    search_pool_pause(pool, false);
    do {
        struct timespec t = {0, 1000000};
        nanosleep(&t, NULL);
        search_pool_snapshot(pool, &snap, NULL);
    } while (snap.running);
    CHECK(snap.tested == s.limit);
    CHECK(snap.rejected < s.limit);
    Candidate c;
    recovered = false;
    while (search_pool_pop(pool, &c))
        verify(&c, NULL);
    CHECK(recovered);
    WorkerSnapshot workers[4];
    search_pool_snapshot(pool, &snap, workers);
    unsigned used = 0;
    for (int i = 0; i < 4; i++)
        used += workers[i].tested > 0;
    CHECK(used >= 2);
    /* Pool reuse and prompt cancellation while paused. */
    s.limit = 0;
    CHECK(search_pool_start(pool, &s));
    search_pool_pause(pool, true);
    double start = lab_now();
    search_pool_stop(pool);
    do {
        struct timespec t = {0, 1000000};
        nanosleep(&t, NULL);
        search_pool_snapshot(pool, &snap, NULL);
    } while (snap.running && lab_now() - start < 2);
    CHECK(!snap.running);
    search_pool_free(pool);
    /* Nonzero crib offset and nonzero rings, reflector C. */
    key.ring[0] = 3;
    key.ring[1] = 7;
    key.ring[2] = 11;
    key.reflector = 1;
    enigma_text(&key, plain, cipher, sizeof cipher);
    CHECK(search_spec_init(&s, cipher, plain + 13, 13, key.ring, 1));
    recovered = false;
    result = bombe_test_state(&s, 7, verify, NULL, NULL);
    CHECK(result.stops > 0);
    CHECK(recovered);
    s.advanced = true;
    uint64_t ring_index = (uint64_t)(3 * 26 * 26 + 7 * 26 + 11);
    recovered = false;
    result = bombe_test_state(&s, ring_index * BOMBE_TRAINING_STATES + 7, verify, NULL, NULL);
    CHECK(result.stops > 0);
    CHECK(recovered);
    CHECK(bombe_total(&s) == 18534946560ULL);
    Benchmark bench;
    benchmark_init(&bench);
    CHECK(benchmark_start(&bench, 4));
    BenchmarkRow rows[BENCHMARK_ROWS];
    unsigned count;
    bool running;
    do {
        struct timespec t = {0, 10000000};
        nanosleep(&t, NULL);
        benchmark_snapshot(&bench, rows, &count, &running);
    } while (running);
    CHECK(count == 3);
    CHECK(rows[0].threads == 1 && rows[1].threads == 2 && rows[2].threads == 4);
    for (unsigned i = 0; i < count; i++) {
        CHECK(rows[i].rate > 0);
        printf("Benchmark %u threads: %.0f states/s, %.2fx\n", rows[i].threads, rows[i].rate,
               rows[i].speedup);
    }
    benchmark_destroy(&bench);
    puts("Deterministic plugboard recovery, independent candidate verification, parallel work, "
         "pause/cancel and pool reuse passed");
}
