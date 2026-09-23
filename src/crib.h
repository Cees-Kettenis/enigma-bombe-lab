#ifndef CRIB_H
#define CRIB_H
#include <stdbool.h>
#include <stddef.h>
bool crib_alignment_valid(const char *cipher, const char *crib, size_t offset);
#endif
