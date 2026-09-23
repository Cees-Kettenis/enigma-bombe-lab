#ifndef GUI_H
#define GUI_H
#include "app.h"
void gui_build(App *app);
void gui_draw_text(cairo_t *cr, double x, double y, double size, const char *text);
void gui_color(cairo_t *cr, double r, double g, double b);
void gui_panel(cairo_t *cr, double x, double y, double width, double height);
#endif
