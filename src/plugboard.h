#ifndef PLUGBOARD_H
#define PLUGBOARD_H
#include <stdbool.h>
#include <stdint.h>
void plugboard_clear(uint8_t p[26]);
bool plugboard_valid(const uint8_t p[26]);
bool plugboard_parse(uint8_t p[26], const char *text);
void plugboard_format(const uint8_t p[26], char out[80]);
void plugboard_random(uint8_t p[26], int pairs, uint64_t *seed);
uint32_t lab_random(uint64_t *seed);
#endif
