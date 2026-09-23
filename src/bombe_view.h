#ifndef BOMBE_VIEW_H
#define BOMBE_VIEW_H
#include "app.h"
void bombe_view_draw(GtkDrawingArea *, cairo_t *, int, int, gpointer);
void graph_view_draw(GtkDrawingArea *, cairo_t *, int, int, gpointer);
void benchmark_view_draw(GtkDrawingArea *, cairo_t *, int, int, gpointer);
#endif
