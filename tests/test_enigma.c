#include "enigma.h"
#include "plugboard.h"
#include "rotor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x);                                \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)
int main(void) {
    rotor_init();
    for (int r = 0; r < 5; r++)
        for (int d = 0; d < 26; d++)
            for (int x = 0; x < 26; x++)
                CHECK(rotor_inverse[r][d][rotor_forward[r][d][x]] == x);
    for (int r = 0; r < 2; r++)
        for (int x = 0; x < 26; x++) {
            CHECK(reflectors[r][reflectors[r][x]] == x);
            CHECK(reflectors[r][x] != x);
        }
    EnigmaKey key;
    enigma_key_default(&key);
    char out[LAB_TEXT_MAX], back[LAB_TEXT_MAX];
    enigma_text(&key, "AAAAA", out, sizeof out);
    CHECK(!strcmp(out, "BDZGO"));
    enigma_text(&key, out, back, sizeof back);
    CHECK(!strcmp(back, "AAAAA"));
    enigma_text(&key, "HELLOWORLD", out, sizeof out);
    CHECK(!strcmp(out, "ILBDAAMTAZ"));
    Enigma m;
    key.start[0] = 0;
    key.start[1] = 3;
    key.start[2] = 20;
    enigma_reset(&m, &key);
    enigma_step(&m);
    CHECK(m.pos[0] == 0 && m.pos[1] == 3 && m.pos[2] == 21);
    enigma_step(&m);
    CHECK(m.pos[0] == 0 && m.pos[1] == 4 && m.pos[2] == 22);
    enigma_step(&m);
    CHECK(m.pos[0] == 1 && m.pos[1] == 5 && m.pos[2] == 23);
    key.ring[1] = 9;
    enigma_reset(&m, &key);
    enigma_step(&m);
    enigma_step(&m);
    enigma_step(&m);
    CHECK(m.pos[0] == 1 && m.pos[1] == 5);
    CHECK(plugboard_parse(key.plug, "AG BL CZ"));
    CHECK(plugboard_valid(key.plug));
    CHECK(!plugboard_parse(key.plug, "AG AB"));
    CHECK(!plugboard_parse(key.plug, "AA"));
    CHECK(!plugboard_parse(key.plug, "AB CD EF GH IJ KL MN OP QR ST UV"));
    uint64_t seed = 12345;
    for (int trial = 0; trial < 100; trial++) {
        enigma_random_key(&key, true, &seed);
        key.reflector = (uint8_t)(trial % 2);
        CHECK(enigma_key_valid(&key));
        CHECK(plugboard_valid(key.plug));
        const char *plain = "THEQUICKBROWNFOXJUMPSOVERTHELAZYDOGWETTERBERICHT";
        enigma_text(&key, plain, out, sizeof out);
        enigma_text(&key, out, back, sizeof back);
        CHECK(!strcmp(plain, back));
        for (size_t i = 0; i < strlen(plain); i++)
            CHECK(out[i] != plain[i]);
    }
    puts("Enigma permutations, reference vectors, reciprocity, rings and double stepping passed");
}
