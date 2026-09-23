#include "rotor.h"
#include <pthread.h>
uint8_t rotor_forward[5][26][26], rotor_inverse[5][26][26], reflectors[2][26];
const uint8_t rotor_notch[5] = {16, 4, 21, 9, 25};
const char *rotor_names[5] = {"I", "II", "III", "IV", "V"};
static pthread_once_t once = PTHREAD_ONCE_INIT;
static void initialize(void) {
    const char *w[5] = {"EKMFLGDQVZNTOWYHXUSPAIBRCJ", "AJDKSIRUXBLHWTMCQGZNPYFVOE",
                        "BDFHJLCPRTXVZNYEIWGAKMUSQO", "ESOVPZJAYQUIRHXLNFTGKDCMWB",
                        "VZBRGITYUPSDNHLXAWMJQOFECK"};
    const char *r[2] = {"YRUHQSLDPXNGOKMIEBFZCWVJAT", "FVPJIAOYEDRZXWGCTKUQSBNMHL"};
    for (int k = 0; k < 5; k++)
        for (int d = 0; d < 26; d++)
            for (int x = 0; x < 26; x++) {
                int y = (w[k][(x + d) % 26] - 'A' - d + 26) % 26;
                rotor_forward[k][d][x] = (uint8_t)y;
                rotor_inverse[k][d][y] = (uint8_t)x;
            }
    for (int k = 0; k < 2; k++)
        for (int x = 0; x < 26; x++)
            reflectors[k][x] = (uint8_t)(r[k][x] - 'A');
}
void rotor_init(void) {
    pthread_once(&once, initialize);
}
