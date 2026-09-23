#include "bombe.h"
#include <math.h>
#include <string.h>
typedef struct {
    const SearchSpec *spec;
    EnigmaKey key;
    uint8_t maps[LAB_CRIB_MAX][26];
    CandidateFn callback;
    void *data;
    const atomic_bool *cancel;
    uint64_t state;
    KernelResult result;
    bool halt;
} Solver;
bool search_spec_init(SearchSpec *s, const char *c, const char *p, unsigned off, const uint8_t rings[3], uint8_t refl) {
    memset(s, 0, sizeof *s);
    if (strlen(c) >= LAB_TEXT_MAX || strlen(p) > LAB_CRIB_MAX || refl > 1)
        return false;
    for (int i = 0; i < 3; i++)
        if (rings[i] > 25)
            return false;
    enigma_normalize(c, s->cipher, sizeof s->cipher);
    enigma_normalize(p, s->crib, sizeof s->crib);
    memcpy(s->rings, rings, 3);
    s->reflector = refl;
    s->max_stops_per_state = 64;
    return menu_build(&s->menu, s->cipher, s->crib, off);
}
uint64_t bombe_total(const SearchSpec *s) {
    uint64_t n = BOMBE_TRAINING_STATES;
    if (s->advanced)
        n *= 17576;
    return s->limit && s->limit < n ? s->limit : n;
}
void bombe_decode_state(const SearchSpec *s, uint64_t state, EnigmaKey *k) {
    enigma_key_default(k);
    k->reflector = s->reflector;
    uint64_t ring = state / BOMBE_TRAINING_STATES;
    state %= BOMBE_TRAINING_STATES;
    unsigned p = (unsigned)(state % 17576), order = (unsigned)(state / 17576), idx = 0;
    for (int a = 0; a < 5; a++)
        for (int b = 0; b < 5; b++)
            if (b != a)
                for (int c = 0; c < 5; c++)
                    if (c != a && c != b) {
                        if (idx++ == order) {
                            k->order[0] = (uint8_t)a;
                            k->order[1] = (uint8_t)b;
                            k->order[2] = (uint8_t)c;
                        }
                    }
    for (int i = 2; i >= 0; i--) {
        k->start[i] = (uint8_t)(p % 26);
        p /= 26;
        k->ring[i] = s->advanced ? (uint8_t)(ring % 26) : s->rings[i];
        ring /= 26;
    }
}
static bool assign(int8_t p[26], int a, int b, int *pairs) {
    if (p[a] >= 0)
        return p[a] == b;
    if (p[b] >= 0)
        return false;
    if (a != b && *pairs >= 10)
        return false;
    p[a] = (int8_t)b;
    p[b] = (int8_t)a;
    if (a != b)
        (*pairs)++;
    return true;
}
double bombe_plaintext_score(const char *s) {
    static const double f[26] = {8.17, 1.49, 2.78, 4.25, 12.70, 2.23, 2.02, 6.09, 6.97,
                                 .15,  .77,  4.03, 2.41, 6.75,  7.51, 1.93, .10,  5.99,
                                 6.33, 9.06, 2.76, .98,  2.36,  .15,  1.97, .07};
    double score = 0;
    size_t n = strlen(s);
    for (size_t i = 0; i < n; i++)
        score += log(f[s[i] - 'A']);
    const char *words[] = {"THE", "ING", "AND", "WETTER", "BERICHT", "ATTACK", "EIN", "DER", "DIE"};
    for (unsigned i = 0; i < sizeof words / sizeof words[0]; i++)
        for (const char *p = s; (p = strstr(p, words[i])); p++)
            score += 5;
    return n ? score / (double)n : 0;
}
static void solve(Solver *s, int8_t p[26], int pairs) {
    if (s->halt || (s->cancel && atomic_load_explicit(s->cancel, memory_order_relaxed)))
        return;
    bool changed = true;
    while (changed) {
        changed = false;
        for (int i = 0; i < s->spec->menu.count; i++) {
            const MenuEdge *e = &s->spec->menu.edges[i];
            int a = e->a, b = e->b;
            if (p[a] >= 0) {
                int y = s->maps[i][p[a]];
                bool fresh = p[b] < 0;
                if (!assign(p, b, y, &pairs))
                    return;
                changed |= fresh;
            }
            if (p[b] >= 0) {
                int y = s->maps[i][p[b]];
                bool fresh = p[a] < 0;
                if (!assign(p, a, y, &pairs))
                    return;
                changed |= fresh;
            }
        }
    }
    int next = -1;
    for (int i = 0; i < 26; i++)
        if (p[i] < 0 && s->spec->menu.degree[i] &&
            (next < 0 || s->spec->menu.degree[i] > s->spec->menu.degree[next]))
            next = i;
    if (next >= 0) {
        for (int v = 0; v < 26 && !s->halt; v++)
            if (p[v] < 0) {
                int8_t q[26];
                memcpy(q, p, 26);
                int n = pairs;
                if (assign(q, next, v, &n))
                    solve(s, q, n);
            }
        return;
    }
    Candidate c = {0};
    c.key = s->key;
    c.state = s->state;
    memcpy(c.deductions, p, 26);
    for (int i = 0; i < 26; i++) {
        if (p[i] < 0)
            c.unresolved++;
        c.key.plug[i] = (uint8_t)(p[i] < 0 ? i : p[i]);
    }
    enigma_text(&c.key, s->spec->cipher, c.plaintext, sizeof c.plaintext);
    /* Independent verification at message offset, with a reset to the candidate start. */
    if (strncmp(c.plaintext + s->spec->menu.offset, s->spec->crib, strlen(s->spec->crib)))
        return;
    c.score = bombe_plaintext_score(c.plaintext);
    s->result.stops++;
    if (s->callback && !s->callback(&c, s->data))
        s->halt = true;
    if (s->spec->max_stops_per_state && s->result.stops >= s->spec->max_stops_per_state) {
        s->result.truncated = true;
        s->halt = true;
    }
}
KernelResult bombe_test_state(const SearchSpec *spec, uint64_t state, CandidateFn fn, void *data,
                              const atomic_bool *cancel) {
    Solver s = {.spec = spec, .callback = fn, .data = data, .cancel = cancel, .state = state};
    bombe_decode_state(spec, state, &s.key);
    Enigma machine;
    enigma_reset(&machine, &s.key);
    for (unsigned i = 0; i < spec->menu.offset; i++) {
        if (cancel && atomic_load_explicit(cancel, memory_order_relaxed))
            return s.result;
        enigma_step(&machine);
    }
    for (int i = 0; i < spec->menu.count; i++) {
        enigma_step(&machine);
        for (int x = 0; x < 26; x++)
            s.maps[i][x] = enigma_scramble(&machine, (uint8_t)x);
    }
    int8_t p[26];
    memset(p, -1, sizeof p);
    solve(&s, p, 0);
    return s.result;
}
