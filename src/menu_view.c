#include "menu_view.h"
#include "gui.h"
#include <math.h>
void menu_view_draw(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer data) {
    (void)area;
    App *a = data;
    gui_panel(cr, 0, 0, width, height);
    if (!a->menu_valid) {
        gui_color(cr, .85, .72, .47);
        gui_draw_text(cr, 25, 45, 16, "No valid menu at this alignment.");
        return;
    }
    double x[26], y[26], r = fmin(width * .41, height * .41), cx = width * .5, cy = height * .5;
    for (int i = 0; i < 26; i++) {
        double t = i * 2 * G_PI / 26 - G_PI / 2;
        x[i] = cx + r * cos(t);
        y[i] = cy + r * sin(t);
    }
    for (int i = 0; i < a->menu.count; i++) {
        MenuEdge *e = &a->menu.edges[i];
        cairo_set_source_rgba(cr, e->cycle ? .95 : .36, e->cycle ? .69 : .61, e->cycle ? .30 : .61,
                              a->menu.count > 60 ? .40 : .70);
        cairo_set_line_width(cr, e->cycle ? 1.8 : 1);
        cairo_move_to(cr, x[e->a], y[e->a]);
        cairo_line_to(cr, x[e->b], y[e->b]);
        cairo_stroke(cr);
        double t = .20 + .6 * (double)(i % 11) / 10;
        char text[12];
        g_snprintf(text, sizeof text, "%u", e->position);
        gui_color(cr, e->cycle ? .95 : .62, e->cycle ? .78 : .77, .53);
        gui_draw_text(cr, x[e->a] * t + x[e->b] * (1 - t), y[e->a] * t + y[e->b] * (1 - t), 9,
                      text);
    }
    for (int i = 0; i < 26; i++) {
        gui_color(cr, i == a->menu.root ? .51 : .13, i == a->menu.root ? .39 : .21, .18);
        cairo_new_sub_path(cr);
        cairo_arc(cr, x[i], y[i], 17, 0, 2 * G_PI);
        cairo_fill_preserve(cr);
        gui_color(cr, .75, .66, .44);
        cairo_set_line_width(cr, 1);
        cairo_stroke(cr);
        gui_color(cr, a->menu.degree[i] ? .96 : .42, a->menu.degree[i] ? .91 : .48,
                  a->menu.degree[i] ? .73 : .46);
        char letter[2] = {(char)('A' + i), 0};
        gui_draw_text(cr, x[i] - 6, y[i] + 6, 18, letter);
    }
}
