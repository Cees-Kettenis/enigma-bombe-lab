#include "enigma.h"
#include "plugboard.h"
#include "rotor.h"
#include <string.h>
void enigma_key_default(EnigmaKey *k) {
    memset(k, 0, sizeof *k);
    k->order[1] = 1;
    k->order[2] = 2;
    plugboard_clear(k->plug);
    rotor_init();
}
bool enigma_key_valid(const EnigmaKey *k) {
    if (k->reflector > 1 || !plugboard_valid(k->plug))
        return false;
    for (int i = 0; i < 3; i++) {
        if (k->order[i] > 4 || k->ring[i] > 25 || k->start[i] > 25)
            return false;
        for (int j = 0; j < i; j++)
            if (k->order[i] == k->order[j])
                return false;
    }
    return true;
}
void enigma_reset(Enigma *m, const EnigmaKey *k) {
    rotor_init();
    m->key = *k;
    memcpy(m->pos, k->start, 3);
}
void enigma_step(Enigma *m) {
    bool middle = m->pos[1] == rotor_notch[m->key.order[1]];
    bool right = m->pos[2] == rotor_notch[m->key.order[2]];
    if (middle)
        m->pos[0] = (uint8_t)((m->pos[0] + 1) % 26);
    if (middle || right)
        m->pos[1] = (uint8_t)((m->pos[1] + 1) % 26);
    m->pos[2] = (uint8_t)((m->pos[2] + 1) % 26);
}
static uint8_t traverse(const Enigma *m, uint8_t x, EnigmaTrace *t) {
    int n = 2, d[3];
    for (int i = 0; i < 3; i++) {
        d[i] = m->pos[i] - m->key.ring[i];
        if (d[i] < 0)
            d[i] += 26;
    }
    for (int i = 2; i >= 0; i--) {
        x = rotor_forward[m->key.order[i]][d[i]][x];
        if (t)
            t->letter[n++] = x;
    }
    x = reflectors[m->key.reflector][x];
    if (t)
        t->letter[n++] = x;
    for (int i = 0; i < 3; i++) {
        x = rotor_inverse[m->key.order[i]][d[i]][x];
        if (t)
            t->letter[n++] = x;
    }
    return x;
}
uint8_t enigma_scramble(const Enigma *m, uint8_t x) {
    return traverse(m, x, NULL);
}
uint8_t enigma_press(Enigma *m, uint8_t x, EnigmaTrace *t) {
    if (t) {
        memcpy(t->before, m->pos, 3);
        t->letter[0] = x;
    }
    enigma_step(m);
    x = m->key.plug[x];
    if (t) {
        memcpy(t->after, m->pos, 3);
        t->letter[1] = x;
    }
    x = traverse(m, x, t);
    x = m->key.plug[x];
    if (t)
        t->letter[9] = x;
    return x;
}
size_t enigma_normalize(const char *in, char *out, size_t cap) {
    size_t n = 0;
    if (!cap)
        return 0;
    for (; *in && n + 1 < cap; in++) {
        unsigned char c = (unsigned char)*in;
        if (c >= 'a' && c <= 'z')
            c = (unsigned char)(c - 'a' + 'A');
        if (c >= 'A' && c <= 'Z')
            out[n++] = (char)c;
    }
    out[n] = 0;
    return n;
}
void enigma_text(const EnigmaKey *k, const char *in, char *out, size_t cap) {
    Enigma m;
    enigma_reset(&m, k);
    size_t n = 0;
    if (!cap)
        return;
    for (; *in && n + 1 < cap; in++) {
        int c = (unsigned char)*in;
        if (c >= 'a' && c <= 'z')
            c = c - 'a' + 'A';
        if (c >= 'A' && c <= 'Z')
            out[n++] = (char)('A' + enigma_press(&m, (uint8_t)(c - 'A'), NULL));
    }
    out[n] = 0;
}
void enigma_random_key(EnigmaKey *k, bool rings, uint64_t *seed) {
    enigma_key_default(k);
    uint8_t bag[5] = {0, 1, 2, 3, 4};
    for (int i = 4; i > 0; i--) {
        int j = (int)(lab_random(seed) % (uint32_t)(i + 1));
        uint8_t t = bag[i];
        bag[i] = bag[j];
        bag[j] = t;
    }
    for (int i = 0; i < 3; i++) {
        k->order[i] = bag[i];
        k->start[i] = (uint8_t)(lab_random(seed) % 26);
        k->ring[i] = rings ? (uint8_t)(lab_random(seed) % 26) : 0;
    }
    plugboard_random(k->plug, 10, seed);
}
