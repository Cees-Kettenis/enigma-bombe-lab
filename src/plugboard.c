#include "plugboard.h"
#include <ctype.h>
#include <string.h>
void plugboard_clear(uint8_t p[26]) {
    for (int i = 0; i < 26; i++)
        p[i] = (uint8_t)i;
}
bool plugboard_valid(const uint8_t p[26]) {
    int pairs = 0;
    for (int i = 0; i < 26; i++) {
        if (p[i] >= 26 || p[p[i]] != i)
            return false;
        pairs += p[i] > i;
    }
    return pairs <= 10;
}
bool plugboard_parse(uint8_t p[26], const char *text) {
    uint8_t tmp[26];
    plugboard_clear(tmp);
    while (*text) {
        while (isspace((unsigned char)*text))
            text++;
        if (!*text)
            break;
        int a = toupper((unsigned char)*text++) - 'A';
        if (!*text)
            return false;
        int b = toupper((unsigned char)*text++) - 'A';
        if (a < 0 || a > 25 || b < 0 || b > 25 || a == b || tmp[a] != a || tmp[b] != b)
            return false;
        tmp[a] = (uint8_t)b;
        tmp[b] = (uint8_t)a;
        if (*text && !isspace((unsigned char)*text))
            return false;
    }
    if (!plugboard_valid(tmp))
        return false;
    memcpy(p, tmp, 26);
    return true;
}
void plugboard_format(const uint8_t p[26], char out[80]) {
    int n = 0;
    for (int i = 0; i < 26; i++)
        if (p[i] > i) {
            if (n)
                out[n++] = ' ';
            out[n++] = (char)('A' + i);
            out[n++] = (char)('A' + p[i]);
        }
    out[n] = 0;
}
uint32_t lab_random(uint64_t *seed) {
    uint64_t x = *seed;
    if (!x)
        x = 0x123456789abcdefULL;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    *seed = x;
    return (uint32_t)(x >> 16);
}
void plugboard_random(uint8_t p[26], int pairs, uint64_t *seed) {
    uint8_t bag[26];
    plugboard_clear(bag);
    plugboard_clear(p);
    if (pairs < 0)
        pairs = 0;
    if (pairs > 10)
        pairs = 10;
    for (int i = 25; i > 0; i--) {
        int j = (int)(lab_random(seed) % (uint32_t)(i + 1));
        uint8_t t = bag[i];
        bag[i] = bag[j];
        bag[j] = t;
    }
    for (int i = 0; i < pairs * 2; i += 2) {
        p[bag[i]] = bag[i + 1];
        p[bag[i + 1]] = bag[i];
    }
}
