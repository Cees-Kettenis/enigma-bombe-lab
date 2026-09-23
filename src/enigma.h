#ifndef ENIGMA_H
#define ENIGMA_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define LAB_TEXT_MAX 2048
#define LAB_CRIB_MAX 256
typedef struct {
    uint8_t order[3], ring[3], start[3], plug[26], reflector;
} EnigmaKey;
typedef struct {
    EnigmaKey key;
    uint8_t pos[3];
} Enigma;
typedef struct {
    uint8_t letter[10], before[3], after[3];
} EnigmaTrace;
void enigma_key_default(EnigmaKey *key);
bool enigma_key_valid(const EnigmaKey *key);
void enigma_reset(Enigma *m, const EnigmaKey *key);
void enigma_step(Enigma *m);
uint8_t enigma_scramble(const Enigma *m, uint8_t x);
uint8_t enigma_press(Enigma *m, uint8_t x, EnigmaTrace *trace);
void enigma_text(const EnigmaKey *key, const char *input, char *output, size_t capacity);
size_t enigma_normalize(const char *input, char *output, size_t capacity);
void enigma_random_key(EnigmaKey *key, bool rings, uint64_t *seed);
#endif
