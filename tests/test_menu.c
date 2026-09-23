#include "crib.h"
#include "menu.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x)                                                                                   \
    do {                                                                                           \
        if (!(x)) {                                                                                \
            fprintf(stderr, "line %d: %s\n", __LINE__, #x);                                        \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)
int main(void) {
    CHECK(!crib_alignment_valid("ABC", "ABC", 0));
    CHECK(!crib_alignment_valid("AB", "XYZ", 0));
    CHECK(crib_alignment_valid("BCD", "ABC", 0));
    Menu m;
    CHECK(menu_build(&m, "BCA", "ABC", 0));
    CHECK(m.count == 3);
    CHECK(m.cycles == 1);
    CHECK(m.degree[0] == 2);
    CHECK(menu_best_alignment("ABCBCA", "ABC") >= 0);
    CHECK(!menu_build(&m, "A", "A", 0));
    CHECK(!menu_build(&m, "BC", "", 0));
    puts("Crib alignment and menu cycles passed");
}
