#include "blind.h"
#include "plugboard.h"
#include "search_worker.h"
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/* Original test prose, not included in the scoring corpus. */
static const char *message =
    "The morning train had already left the station when we reached the platform. "
    "A porter told us that another would arrive before noon, so we walked across "
    "the square and found a small place to eat. Through the open window we could "
    "see people carrying baskets toward the market and children waiting beside "
    "the fountain. My sister took a letter from her pocket and read the address "
    "again. We had never visited this town before, but the house we were looking "
    "for was supposed to stand near the river, beyond the old stone bridge. "
    "After breakfast we asked the owner for directions and continued on foot. "
    "The streets were quiet by then and the sun was beginning to warm the walls.";
static Candidate best;
static unsigned received;
static bool collect(const Candidate *c, void *unused) {
    (void)unused;
    assert(enigma_key_valid(&c->key));
    assert(c->heuristic && c->unresolved == 26);
    assert(c->score == blind_english_score(c->plaintext));
    if (!received || c->score > best.score)
        best = *c;
    received++;
    return true;
}
int main(void) {
    assert(fabs(blind_confidence_percent(-10) - 5) < .001);
    assert(fabs(blind_confidence_percent(-8.75) - 50) < .001);
    assert(fabs(blind_confidence_percent(-7.5) - 95) < .001);
    assert(blind_confidence_percent(-7.489) > blind_confidence_percent(-8));
    assert(blind_confidence_percent(0) <= 99.9);
    assert(blind_confidence_percent(NAN) == 0);
    char plain[LAB_TEXT_MAX], cipher[LAB_TEXT_MAX];
    enigma_normalize(message, plain, sizeof plain);
    EnigmaKey key;
    enigma_key_default(&key);
    key.start[2] = 7;
    key.ring[0] = 2;
    key.ring[1] = 5;
    key.ring[2] = 9;
    key.reflector = 1;
    assert(plugboard_parse(key.plug, "AV BS CG DL FU HZ IN KM OW RX"));
    enigma_text(&key, plain, cipher, sizeof cipher);
    SearchSpec s;
    assert(blind_spec_init(&s, cipher, key.ring, key.reflector, 4, 12345));
    assert(!s.crib[0] && !s.menu.count && s.blind && !s.advanced);
    double start = lab_now();
    blind_test_state(&s, 7, collect, NULL, NULL);
    printf("Known rotor state, ten unknown plugs: %s, %.3fs, score %.3f\n",
           strcmp(best.plaintext, plain) ? "NOT RECOVERED" : "recovered", lab_now() - start,
           best.score);
    if (strcmp(best.plaintext, plain))
        puts(best.plaintext);
    assert(received == 1);
    assert(!strcmp(best.plaintext, plain));

    /* Real parallel rotor enumeration, no crib, known rings/reflector only.
     * The small state limit bounds this regression test, not the GUI search. */
    s.limit = 24;
    s.blind_restarts = 1;
    received = 0;
    SearchPool *pool = search_pool_new(2);
    assert(pool && search_pool_start(pool, &s));
    search_pool_wait(pool, NULL);
    SearchSnapshot snapshot;
    search_pool_snapshot(pool, &snapshot, NULL);
    assert(snapshot.tested == s.limit && !snapshot.running);
    Candidate candidate;
    while (search_pool_pop(pool, &candidate))
        collect(&candidate, NULL);
    assert(received && !strcmp(best.plaintext, plain));
    printf("Parallel 24-state search recovered %zu letters and the key.\n", strlen(plain));
    assert(!memcmp(&best.key, &key, sizeof key));
    /* The worker pool must stop without waiting for the GUI, and retain its answer. */
    s.stop_confidence = 80;
    received = 0;
    assert(search_pool_start(pool, &s));
    search_pool_wait(pool, NULL);
    search_pool_snapshot(pool, &snapshot, NULL);
    assert(!snapshot.running && snapshot.tested < s.limit);
    assert(snapshot.reached_confidence >= 80);
    while (search_pool_pop(pool, &candidate))
        collect(&candidate, NULL);
    assert(received && blind_confidence_percent(best.score) >= 80);

    /* Disabling the threshold on the same pool restores exhaustive scanning. */
    s.stop_confidence = 0;
    assert(search_pool_start(pool, &s));
    search_pool_wait(pool, NULL);
    search_pool_snapshot(pool, &snapshot, NULL);
    assert(snapshot.tested == s.limit && snapshot.reached_confidence == 0);

    /* Clue mode uses the same English scale, rather than its legacy ranking score. */
    SearchSpec clue;
    assert(search_spec_init(&clue, cipher, "THEMORNINGTRAINHADALREADYLEFTTHESTATION", 0,
                            key.ring, key.reflector));
    clue.stop_confidence = 80;
    clue.limit = 24;
    assert(search_pool_start(pool, &clue));
    search_pool_wait(pool, NULL);
    search_pool_snapshot(pool, &snapshot, NULL);
    assert(!snapshot.running && snapshot.reached_confidence >= 80);
    bool retained = false;
    while (search_pool_pop(pool, &candidate))
        retained |= blind_confidence_percent(blind_english_score(candidate.plaintext)) >= 80;
    assert(retained);
    clue.stop_confidence = 101;
    assert(!search_pool_start(pool, &clue));
    s.limit = 0;
    assert(search_pool_start(pool, &s));
    search_pool_pause(pool, true);
    start = lab_now();
    search_pool_stop(pool);
    search_pool_wait(pool, NULL);
    assert(lab_now() - start < 2);
    search_pool_free(pool);

    atomic_bool cancel = ATOMIC_VAR_INIT(true);
    received = 0;
    blind_test_state(&s, 7, collect, NULL, &cancel);
    assert(received == 0);
    assert(!blind_spec_init(&s, "ABC", key.ring, 0, 1, 0));
    assert(!blind_spec_init(&s, cipher, key.ring, 2, 1, 0));
    assert(!blind_spec_init(&s, cipher, key.ring, 0, 17, 0));
    assert(blind_english_score("123") == -DBL_MAX);
    return 0;
}
