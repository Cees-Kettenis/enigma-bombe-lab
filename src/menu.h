#ifndef MENU_H
#define MENU_H
#include "enigma.h"
typedef struct {
    uint8_t a, b;
    unsigned position;
    bool cycle;
} MenuEdge;
typedef struct {
    MenuEdge edges[LAB_CRIB_MAX];
    int count, degree[26], root, cycles;
    unsigned offset;
} Menu;
bool menu_build(Menu *menu, const char *cipher, const char *crib, unsigned offset);
int menu_best_alignment(const char *cipher, const char *crib);
#endif
