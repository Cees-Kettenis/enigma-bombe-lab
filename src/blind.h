#ifndef BLIND_H
#define BLIND_H
#include "bombe.h"
#define BLIND_MIN_LETTERS 50
bool blind_spec_init(SearchSpec *s, const char *cipher, const uint8_t rings[3], uint8_t reflector,
                     unsigned restarts, uint64_t seed);
/* One rotor state; only ciphertext, public assumptions and random seed are inputs. */
KernelResult blind_test_state(const SearchSpec *s, uint64_t state, CandidateFn callback, void *data,
                              const atomic_bool *cancel);
double blind_english_score(const char *text);
/* Display index only, not a calibrated probability of a correct decryption. */
double blind_confidence_percent(double english_score);
#endif
