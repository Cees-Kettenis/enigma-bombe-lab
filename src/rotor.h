#ifndef ROTOR_H
#define ROTOR_H
#include <stdint.h>
extern uint8_t rotor_forward[5][26][26];
extern uint8_t rotor_inverse[5][26][26];
extern uint8_t reflectors[2][26];
extern const uint8_t rotor_notch[5];
extern const char *rotor_names[5];
void rotor_init(void);
#endif
