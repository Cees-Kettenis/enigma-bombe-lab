#ifndef BOMBE_H
#define BOMBE_H
#include "menu.h"
#include <stdatomic.h>
#define BOMBE_TRAINING_STATES (60ULL * 26 * 26 * 26)
typedef struct {
    char cipher[LAB_TEXT_MAX], crib[LAB_CRIB_MAX + 1];
    Menu menu;
    uint8_t rings[3], reflector;
    bool advanced;
    uint64_t limit;
    unsigned max_stops_per_state;
} SearchSpec;
typedef struct {
    EnigmaKey key;
    int8_t deductions[26];
    unsigned unresolved, worker;
    uint64_t state;
    double elapsed, score;
    char plaintext[LAB_TEXT_MAX];
} Candidate;
typedef bool (*CandidateFn)(const Candidate *, void *);
typedef struct {
    uint64_t stops;
    bool truncated;
} KernelResult;
bool search_spec_init(SearchSpec *s, const char *cipher, const char *crib, unsigned offset,
                      const uint8_t rings[3], uint8_t reflector);
uint64_t bombe_total(const SearchSpec *s);
void bombe_decode_state(const SearchSpec *s, uint64_t state, EnigmaKey *key);
KernelResult bombe_test_state(const SearchSpec *s, uint64_t state, CandidateFn callback, void *data,
                              const atomic_bool *cancel);
double bombe_plaintext_score(const char *text);
#endif
