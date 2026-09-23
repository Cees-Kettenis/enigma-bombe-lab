#ifndef ENIGMA_VIEW_H
#define ENIGMA_VIEW_H
#include "app.h"
void enigma_view_draw(GtkDrawingArea *, cairo_t *, int, int, gpointer);
void plugboard_view_draw(GtkDrawingArea *, cairo_t *, int, int, gpointer);
void plugboard_pressed(GtkGestureClick *, int, double, double, gpointer);
gboolean enigma_view_tick(GtkWidget *, GdkFrameClock *, gpointer);
#endif
