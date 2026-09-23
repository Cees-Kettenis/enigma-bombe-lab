#include "bombe_view.h"
#include "gui.h"
#include "rotor.h"
#include <math.h>
void bombe_view_draw(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer data) {
    (void)area;
    (void)height;
    App *a = data;
    if (!a->pool_threads) {
        gui_color(cr, .75, .78, .69);
        gui_draw_text(cr, 25, 55, 18, "Choose a search mode and start exploring your intercept.");
        return;
    }
    int columns = MAX(1, width / 290);
    double cell = (double)width / columns;
    double now = lab_now();
    for (unsigned i = 0; i < a->pool_threads; i++) {
        double x = (i % (unsigned)columns) * cell + 5, y = (i / (unsigned)columns) * 190 + 5;
        WorkerSnapshot *w = &a->workers[i];
        EnigmaKey k;
        bombe_decode_state(&a->active_spec, w->state, &k);
        gui_panel(cr, x, y, cell - 10, 180);
        bool flash = now - a->worker_flashes[i] < 1;
        gui_color(cr, flash ? 1 : .80, flash ? .76 : .74, .46);
        char line[140];
        g_snprintf(line, sizeof line, "WORKER %02u  %s", i + 1,
                   flash                ? "STOP FOUND"
                   : a->snapshot.paused ? "PAUSED"
                   : w->active          ? "SEARCHING"
                                        : "STOPPED");
        gui_draw_text(cr, x + 12, y + 23, 12, line);
        for (int j = 0; j < 3; j++) {
            double cx = x + cell * (.21 + j * .28), cy = y + 79, r = 29;
            gui_color(cr, .50, .49, .38);
            cairo_new_sub_path(cr);
            cairo_arc(cr, cx, cy, r, 0, 2 * G_PI);
            cairo_set_line_width(cr, 5);
            cairo_stroke(cr);
            double turn = (double)k.start[j] / 26 * 2 * G_PI;
            if (w->active && !a->snapshot.paused && !a->reduced_motion)
                turn += (now - a->last_sample) *
                        (gtk_drop_down_get_selected(GTK_DROP_DOWN(a->mode)) == 1 ? 1.5 : 5);
            for (int t = 0; t < 26; t++) {
                double angle = turn + t * 2 * G_PI / 26;
                cairo_move_to(cr, cx + cos(angle) * (r - 5), cy + sin(angle) * (r - 5));
                cairo_line_to(cr, cx + cos(angle) * r, cy + sin(angle) * r);
            }
            gui_color(cr, .82, .70, .43);
            cairo_set_line_width(cr, 1);
            cairo_stroke(cr);
            gui_color(cr, .95, .87, .64);
            char c[2] = {(char)('A' + k.start[j]), 0};
            gui_draw_text(cr, cx - 9, cy + 8, 26, c);
            gui_draw_text(cr, cx - 9, cy + 45, 11, rotor_names[k.order[j]]);
        }
        gui_color(cr, .75, .81, .76);
        g_snprintf(line, sizeof line, "%" G_GUINT64_FORMAT " states   %.0f /s", w->tested,
                   a->worker_rates[i]);
        gui_draw_text(cr, x + 12, y + 147, 12, line);
        g_snprintf(line, sizeof line, "%" G_GUINT64_FORMAT " stops  |  ring %c%c%c", w->stops,
                   'A' + k.ring[0], 'A' + k.ring[1], 'A' + k.ring[2]);
        gui_draw_text(cr, x + 12, y + 167, 11, line);
    }
}
void graph_view_draw(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer data) {
    (void)area;
    App *a = data;
    gui_panel(cr, 0, 0, width, height);
    gui_color(cr, .68, .74, .67);
    char title[100];
    g_snprintf(title, sizeof title, "THROUGHPUT / REAL STATES PER SECOND    peak %.0f",
               a->history_max);
    gui_draw_text(cr, 15, 22, 11, title);
    if (a->history_count < 2)
        return;
    double max = fmax(a->history_max, 1), w = width - 30, h = height - 42;
    gui_color(cr, .90, .71, .37);
    cairo_set_line_width(cr, 2);
    for (unsigned i = 0; i < a->history_count; i++) {
        unsigned j = (a->history_head + 240 - a->history_count + i) % 240;
        double x = 15 + w * i / 239, y = height - 10 - a->history[j] / max * h;
        if (i == 0)
            cairo_move_to(cr, x, y);
        else
            cairo_line_to(cr, x, y);
    }
    cairo_stroke(cr);
}
void benchmark_view_draw(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer data) {
    (void)area;
    App *a = data;
    gui_panel(cr, 0, 0, width, height);
    double max = 1;
    for (unsigned i = 0; i < a->benchmark_count; i++)
        max = fmax(max, a->benchmark_rows[i].rate);
    for (unsigned i = 0; i < a->benchmark_count; i++) {
        BenchmarkRow *r = &a->benchmark_rows[i];
        double y = 35 + i * (height - 50.0) / MAX(a->benchmark_count, 1);
        gui_color(cr, .87, .71, .42);
        cairo_rectangle(cr, 140, y - 13, (width - 290) * r->rate / max, 20);
        cairo_fill(cr);
        char label[90];
        gui_color(cr, .88, .88, .77);
        g_snprintf(label, sizeof label, "%u threads", r->threads);
        gui_draw_text(cr, 20, y + 2, 13, label);
        g_snprintf(label, sizeof label, "%.0f /s", r->rate);
        gui_draw_text(cr, 155 + (width - 290) * r->rate / max, y + 2, 12, label);
    }
}
