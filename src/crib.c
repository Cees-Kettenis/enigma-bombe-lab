#include "crib.h"
#include <string.h>
bool crib_alignment_valid(const char *c, const char *p, size_t off) {
    size_t n = strlen(p), m = strlen(c);
    if (!n || off > m || n > m - off)
        return false;
    for (size_t i = 0; i < n; i++)
        if (p[i] < 'A' || p[i] > 'Z' || c[off + i] < 'A' || c[off + i] > 'Z' || p[i] == c[off + i])
            return false;
    return true;
}
