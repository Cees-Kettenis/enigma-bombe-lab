#include "menu.h"
#include "crib.h"
#include <string.h>
static int root(int *parent, int a) {
    while (parent[a] != a)
        a = parent[a];
    return a;
}
bool menu_build(Menu *m, const char *cipher, const char *crib, unsigned off) {
    memset(m, 0, sizeof *m);
    size_t n = strlen(crib);
    if (n > LAB_CRIB_MAX || !crib_alignment_valid(cipher, crib, off))
        return false;
    int parent[26];
    for (int i = 0; i < 26; i++)
        parent[i] = i;
    m->offset = off;
    m->count = (int)n;
    for (int i = 0; i < m->count; i++) {
        MenuEdge *e = &m->edges[i];
        e->a = (uint8_t)(crib[i] - 'A');
        e->b = (uint8_t)(cipher[off + (unsigned)i] - 'A');
        e->position = off + (unsigned)i;
        m->degree[e->a]++;
        m->degree[e->b]++;
        int a = root(parent, e->a), b = root(parent, e->b);
        e->cycle = a == b;
        if (e->cycle)
            m->cycles++;
        else
            parent[a] = b;
    }
    for (int i = 1; i < 26; i++)
        if (m->degree[i] > m->degree[m->root])
            m->root = i;
    return true;
}
int menu_best_alignment(const char *cipher, const char *crib) {
    size_t n = strlen(crib), len = strlen(cipher);
    if (!n || n > len)
        return -1;
    int best = -1, score = -1;
    Menu m;
    for (size_t i = 0; i <= len - n; i++)
        if (menu_build(&m, cipher, crib, (unsigned)i)) {
            int s = m.cycles * 100 + m.degree[m.root];
            if (s > score) {
                score = s;
                best = (int)i;
            }
        }
    return best;
}
