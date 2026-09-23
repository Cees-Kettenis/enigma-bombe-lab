#include "blind.h"
#include "english_trigrams.inc"
#include "plugboard.h"
#include <float.h>
#include <math.h>
#include <string.h>

typedef struct {
    uint8_t maps[LAB_TEXT_MAX][26], cipher[LAB_TEXT_MAX];
    size_t length;
} Landscape;

double blind_confidence_percent(double score) {
    if (!isfinite(score))
        return 0;
    /* Fixed display anchors: -10 -> 5%, -8.75 -> 50%, -7.5 -> 95%.
     * These are heuristic scale choices, not measured success frequencies. */
    double value = 100.0 / (1.0 + exp(-2.3555511833 * (score + 8.75)));
    return fmin(99.9, fmax(0.1, value));
}

double blind_english_score(const char *text) {
    size_t n = strlen(text);
    if (n < 3)
        return -DBL_MAX;
    double sum = 0;
    for (size_t i = 0; i < n; i++) {
        if (text[i] < 'A' || text[i] > 'Z')
            return -DBL_MAX;
        if (i >= 2)
            sum += english_trigrams[(text[i - 2] - 'A') * 676 + (text[i - 1] - 'A') * 26 + text[i] -
                                    'A'];
    }
    return sum / (1000.0 * (double)(n - 2));
}

bool blind_spec_init(SearchSpec *s, const char *cipher, const uint8_t rings[3], uint8_t reflector,
                     unsigned restarts, uint64_t seed) {
    memset(s, 0, sizeof *s);
    if (strlen(cipher) >= LAB_TEXT_MAX || reflector > 1 || !restarts || restarts > 16)
        return false;
    for (int i = 0; i < 3; i++)
        if (rings[i] > 25)
            return false;
    if (enigma_normalize(cipher, s->cipher, sizeof s->cipher) < BLIND_MIN_LETTERS)
        return false;
    memcpy(s->rings, rings, 3);
    s->reflector = reflector;
    s->blind = true;
    s->blind_restarts = restarts;
    s->blind_seed = seed;
    return true;
}

static double evaluate(const Landscape *l, const uint8_t plug[26], bool language) {
    unsigned counts[26] = {0};
    int prev2 = 0, prev = 0;
    double sum = 0;
    for (size_t i = 0; i < l->length; i++) {
        int x = plug[l->maps[i][plug[l->cipher[i]]]];
        if (language) {
            if (i >= 2)
                sum += english_trigrams[prev2 * 676 + prev * 26 + x];
            prev2 = prev;
            prev = x;
        } else
            counts[x]++;
    }
    if (!language)
        for (int i = 0; i < 26; i++)
            sum += (double)counts[i] * (counts[i] - 1.0);
    return sum;
}

/* Rewire two contacts while preserving the involution and ten-pair limit. */
static bool rewire(uint8_t p[26], int a, int b) {
    int x = p[a], y = p[b];
    p[x] = (uint8_t)x;
    p[y] = (uint8_t)y;
    p[a] = (uint8_t)a;
    p[b] = (uint8_t)b;
    if (x != b) {
        p[a] = (uint8_t)b;
        p[b] = (uint8_t)a;
        if (x != a && y != b) {
            p[x] = (uint8_t)y;
            p[y] = (uint8_t)x;
        }
    }
    return plugboard_valid(p);
}

static bool stopped(const atomic_bool *cancel) {
    return cancel && atomic_load_explicit(cancel, memory_order_relaxed);
}

KernelResult blind_test_state(const SearchSpec *s, uint64_t state, CandidateFn callback, void *data,
                              const atomic_bool *cancel) {
    KernelResult result = {0};
    if (stopped(cancel))
        return result;
    Landscape l;
    l.length = strlen(s->cipher);
    EnigmaKey key;
    bombe_decode_state(s, state, &key);
    Enigma machine;
    enigma_reset(&machine, &key);
    for (size_t i = 0; i < l.length; i++) {
        if (stopped(cancel))
            return result;
        l.cipher[i] = (uint8_t)(s->cipher[i] - 'A');
        enigma_step(&machine);
        for (int x = 0; x < 26; x++)
            l.maps[i][x] = enigma_scramble(&machine, (uint8_t)x);
    }
    Candidate best = {.key = key, .state = state, .score = -DBL_MAX, .heuristic = true};
    uint64_t seed = s->blind_seed ^ (state + 1) * UINT64_C(0x9e3779b97f4a7c15);
    for (unsigned restart = 0; restart < s->blind_restarts; restart++) {
        uint8_t plug[26];
        plugboard_random(plug, restart ? 1 + (int)(lab_random(&seed) % 3) : 0, &seed);
        /* IC helps find initial plugs; trigrams refine readable English. */
        for (int phase = 0; phase < 2; phase++) {
            double score = evaluate(&l, plug, phase != 0);
            for (unsigned round = 0; round < 20; round++) {
                double english = evaluate(&l, plug, true) / (1000.0 * (double)(l.length - 2));
                if (english > best.score) {
                    best.score = english;
                    memcpy(best.key.plug, plug, 26);
                }
                double next_score = score;
                uint8_t next[26];
                memcpy(next, plug, 26);
                for (int a = 0; a < 26; a++) {
                    if (stopped(cancel))
                        return result;
                    for (int b = a + 1; b < 26; b++) {
                        uint8_t trial[26];
                        memcpy(trial, plug, 26);
                        if (!rewire(trial, a, b))
                            continue;
                        double value = evaluate(&l, trial, phase != 0);
                        if (value > next_score) {
                            next_score = value;
                            memcpy(next, trial, 26);
                        }
                    }
                }
                if (next_score <= score)
                    break;
                score = next_score;
                memcpy(plug, next, 26);
            }
        }
        double english = evaluate(&l, plug, true) / (1000.0 * (double)(l.length - 2));
        if (english > best.score) {
            best.score = english;
            memcpy(best.key.plug, plug, 26);
        }
    }
    if (stopped(cancel))
        return result;
    enigma_text(&best.key, s->cipher, best.plaintext, sizeof best.plaintext);
    /* All mappings here are guesses, never crib deductions. */
    memset(best.deductions, -1, sizeof best.deductions);
    best.unresolved = 26;
    result.stops = 1;
    if (callback)
        callback(&best, data);
    return result;
}
