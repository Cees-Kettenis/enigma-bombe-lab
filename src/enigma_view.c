#include "enigma_view.h"
#include "gui.h"
#include "plugboard.h"
#include "rotor.h"
#include <math.h>
#include <string.h>
static void circle(cairo_t *cr, double x, double y, double r) {
    cairo_new_sub_path(cr);
    cairo_arc(cr, x, y, r, 0, 2 * G_PI);
}
static double progress(const App *a) {
    if (!a->animating || a->animation_duration <= 0)
        return 1;
    double p = (lab_now() - a->animation_start) / a->animation_duration - (double)a->trace_index;
    return CLAMP(p, 0, 1);
}
void enigma_view_draw(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer data) {
    (void)area;
    App *a = data;
    gui_panel(cr, 0, 0, width, height);
    double scale = fmin((double)width / 1100, (double)height / 485);
    cairo_translate(cr, (width - 1100 * scale) / 2, (height - 485 * scale) / 2);
    cairo_scale(cr, scale, scale);
    const EnigmaTrace *trace = a->trace_count ? &a->traces[a->trace_index] : NULL;
    double phase = progress(a);
    gui_color(cr, .73, .66, .48);
    gui_draw_text(cr, 24, 29, 12, "ROTOR WINDOWS  /  LEFT TO RIGHT");
    for (int i = 0; i < 3; i++) {
        double x = 260 + i * 240;
        int pos = trace ? trace->after[i] : a->key.start[i];
        double drift = trace && trace->before[i] != trace->after[i] && a->animating
                           ? 1 - fmin(phase * 4, 1)
                           : 0;
        cairo_pattern_t *metal = cairo_pattern_create_linear(x - 70, 0, x + 70, 0);
        cairo_pattern_add_color_stop_rgb(metal, 0, .14, .19, .19);
        cairo_pattern_add_color_stop_rgb(metal, .45, .41, .44, .37);
        cairo_pattern_add_color_stop_rgb(metal, 1, .12, .16, .17);
        cairo_rectangle(cr, x - 77, 50, 154, 172);
        cairo_set_source(cr, metal);
        cairo_fill(cr);
        cairo_pattern_destroy(metal);
        gui_color(cr, .80, .67, .40);
        cairo_set_line_width(cr, 3);
        cairo_rectangle(cr, x - 77, 50, 154, 172);
        cairo_stroke(cr);
        gui_color(cr, .94, .87, .68);
        char title[48];
        g_snprintf(title, sizeof title, "%s  RING %c  NOTCH %c", rotor_names[a->key.order[i]],
                   'A' + a->key.ring[i], 'A' + rotor_notch[a->key.order[i]]);
        gui_draw_text(cr, x - 85, 43, 12, title);
        cairo_save(cr);
        cairo_rectangle(cr, x - 70, 55, 140, 162);
        cairo_clip(cr);
        for (int n = -2; n <= 2; n++) {
            int letter = (pos + n + 26) % 26;
            double y = 144 + (n + drift) * 39;
            gui_color(cr, n == 0 ? .98 : .66, n == 0 ? .88 : .69, n == 0 ? .61 : .61);
            char t[2] = {(char)('A' + letter), 0};
            gui_draw_text(cr, x - 11, y, n == 0 ? 35 : 22, t);
            if (letter == rotor_notch[a->key.order[i]]) {
                gui_color(cr, .96, .61, .25);
                gui_draw_text(cr, x + 36, y - 3, 18, "<");
            }
        }
        cairo_restore(cr);
        gui_color(cr, .94, .77, .40);
        cairo_set_line_width(cr, 2);
        cairo_rectangle(cr, x - 61, 108, 122, 48);
        cairo_stroke(cr);
        gui_color(cr, .48, .52, .47);
        gui_draw_text(cr, x - 92, 242, 10, "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    }
    /* Ten stages correspond exactly to EnigmaTrace. Forward path moves right to left. */
    double px[10] = {100, 155, 740, 500, 260, 180, 260, 500, 740, 965};
    double py[10] = {295, 280, 260, 260, 260, 283, 305, 305, 305, 295};
    gui_color(cr, .37, .42, .37);
    cairo_set_line_width(cr, 3);
    cairo_move_to(cr, px[0], py[0]);
    for (int i = 1; i < 10; i++)
        cairo_line_to(cr, px[i], py[i]);
    cairo_stroke(cr);
    int stage = (int)fmin(9, fmax(0, (phase - .15) / .75 * 9));
    if (trace) {
        gui_color(cr, .97, .73, .28);
        cairo_set_line_width(cr, 4);
        cairo_move_to(cr, px[0], py[0]);
        for (int i = 1; i <= stage; i++)
            cairo_line_to(cr, px[i], py[i]);
        cairo_stroke(cr);
        circle(cr, px[stage], py[stage], 7);
        cairo_fill(cr);
        gui_color(cr, .98, .89, .67);
        char desc[190];
        static const char *names[] = {
            "KEY",       "PLUG IN",     "RIGHT FORWARD", "MIDDLE FORWARD", "LEFT FORWARD",
            "REFLECTOR", "LEFT RETURN", "MIDDLE RETURN", "RIGHT RETURN",   "LAMP"};
        g_snprintf(desc, sizeof desc, "%s  %c     %c > %c > %c > %c > %c > %c > %c > %c > %c > %c",
                   names[stage], 'A' + trace->letter[stage], 'A' + trace->letter[0],
                   'A' + trace->letter[1], 'A' + trace->letter[2], 'A' + trace->letter[3],
                   'A' + trace->letter[4], 'A' + trace->letter[5], 'A' + trace->letter[6],
                   'A' + trace->letter[7], 'A' + trace->letter[8], 'A' + trace->letter[9]);
        gui_draw_text(cr, 170, 335, 13, desc);
    } else {
        gui_color(cr, .65, .70, .66);
        gui_draw_text(cr, 230, 334, 13, "Type on the message desk, or use the keyboard above.");
    }
    const char *letters = "QWERTZUIOASDFGHJKPYXCVBNML";
    for (int i = 0; i < 26; i++) {
        int row = i < 9 ? 0 : i < 18 ? 1 : 2, col = i - (row == 0 ? 0 : row == 1 ? 9 : 18);
        double x = 240 + col * 75 + (row == 2 ? 35 : 0), y = 366 + row * 42;
        bool lit = trace && trace->letter[9] == letters[i] - 'A' && (!a->animating || phase > .88);
        if (lit) {
            cairo_pattern_t *glow = cairo_pattern_create_radial(x, y, 4, x, y, 29);
            cairo_pattern_add_color_stop_rgba(glow, 0, 1, .7, .15, .7);
            cairo_pattern_add_color_stop_rgba(glow, 1, 1, .7, .15, 0);
            cairo_set_source(cr, glow);
            circle(cr, x, y, 29);
            cairo_fill(cr);
            cairo_pattern_destroy(glow);
        }
        gui_color(cr, lit ? .98 : .14, lit ? .76 : .20, lit ? .30 : .21);
        circle(cr, x, y, 17);
        cairo_fill_preserve(cr);
        gui_color(cr, .53, .49, .36);
        cairo_set_line_width(cr, 2);
        cairo_stroke(cr);
        gui_color(cr, lit ? .12 : .86, lit ? .13 : .84, lit ? .10 : .72);
        char t[2] = {letters[i], 0};
        gui_draw_text(cr, x - 6, y + 6, 19, t);
    }
    gui_color(cr, .60, .65, .60);
    gui_draw_text(cr, 24, 400, 11, "LAMPBOARD");
    if (trace) {
        char key[24];
        g_snprintf(key, sizeof key, "KEY %c", 'A' + trace->letter[0]);
        gui_color(cr, .83, .70, .43);
        circle(cr, 89, 188, 28 - (a->animating && phase < .12 ? 3 : 0));
        cairo_fill(cr);
        gui_color(cr, .12, .16, .16);
        gui_draw_text(cr, 62, 194, 16, key);
    }
}
static void socket_xy(int i, double *x, double *y) {
    *x = 80 + (i % 13) * 76;
    *y = i < 13 ? 95 : 315;
}
void plugboard_view_draw(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer data) {
    (void)area;
    App *a = data;
    gui_panel(cr, 0, 0, width, height);
    double scale = fmin((double)width / 1100, (double)height / 420);
    cairo_translate(cr, (width - 1100 * scale) / 2, (height - 420 * scale) / 2);
    cairo_scale(cr, scale, scale);
    gui_color(cr, .72, .65, .48);
    gui_draw_text(cr, 25, 32, 13, "PATCH PANEL  /  26 CONTACTS  /  MAXIMUM 10 CABLES");
    for (int i = 0; i < 26; i++)
        if (a->key.plug[i] > i) {
            double x, y, xx, yy;
            socket_xy(i, &x, &y);
            socket_xy(a->key.plug[i], &xx, &yy);
            double bend = y == yy ? (y < 200 ? 100 : -100) : 35;
            cairo_move_to(cr, x, y);
            cairo_curve_to(cr, x, y + bend + (y == yy ? 0 : 65), xx, yy + bend - (y == yy ? 0 : 65),
                           xx, yy);
            cairo_set_line_width(cr, 11);
            gui_color(cr, .025, .045, .043);
            cairo_stroke_preserve(cr);
            cairo_set_line_width(cr, 6);
            gui_color(cr, .34 + (i % 3) * .11, .35 + (i % 4) * .04, .22 + (i % 2) * .09);
            cairo_stroke(cr);
        }
    for (int i = 0; i < 26; i++) {
        double x, y;
        socket_xy(i, &x, &y);
        gui_color(cr, .60, .55, .39);
        circle(cr, x, y, 20);
        cairo_fill(cr);
        gui_color(cr, .045, .075, .08);
        circle(cr, x, y, 13);
        cairo_fill(cr);
        if (a->key.plug[i] != i) {
            gui_color(cr, .68, .59, .37);
            circle(cr, x, y, 8);
            cairo_fill(cr);
        }
        if (a->plug_selected == i) {
            gui_color(cr, 1, .79, .31);
            circle(cr, x, y, 25);
            cairo_set_line_width(cr, 3);
            cairo_stroke(cr);
        }
        gui_color(cr, .92, .86, .70);
        char t[2] = {(char)('A' + i), 0};
        gui_draw_text(cr, x - 7, y + (i < 13 ? -30 : 43), 20, t);
    }
    char pairs[80];
    plugboard_format(a->key.plug, pairs);
    gui_color(cr, .88, .76, .49);
    gui_draw_text(cr, 80, 395, 16, *pairs ? pairs : "No cables connected");
}
void plugboard_pressed(GtkGestureClick *gesture, int presses, double x, double y, gpointer data) {
    (void)gesture;
    (void)presses;
    App *a = data;
    double width = gtk_widget_get_width(a->plug_canvas);
    double height = gtk_widget_get_height(a->plug_canvas);
    double scale = fmin(width / 1100, height / 420);
    if (scale <= 0)
        return;
    x = (x - (width - 1100 * scale) / 2) / scale;
    y = (y - (height - 420 * scale) / 2) / scale;
    for (int i = 0; i < 26; i++) {
        double xx, yy;
        socket_xy(i, &xx, &yy);
        if (hypot(x - xx, y - yy) > 27)
            continue;
        if (a->key.plug[i] != i) {
            int partner = a->key.plug[i];
            a->key.plug[partner] = (uint8_t)partner;
            a->key.plug[i] = (uint8_t)i;
            a->plug_selected = -1;
        } else if (a->plug_selected < 0)
            a->plug_selected = i;
        else {
            int other = a->plug_selected, count = 0;
            for (int n = 0; n < 26; n++)
                count += a->key.plug[n] > n;
            if (other != i && count < 10) {
                a->key.plug[i] = (uint8_t)other;
                a->key.plug[other] = (uint8_t)i;
            } else if (count >= 10)
                app_status(a, "Ten cables are already connected. Remove one first.");
            a->plug_selected = -1;
        }
        char pairs[80];
        plugboard_format(a->key.plug, pairs);
        gtk_editable_set_text(GTK_EDITABLE(a->plug_text), pairs);
        gtk_widget_queue_draw(a->plug_canvas);
        a->animating = false;
        a->trace_count = 0;
        gtk_widget_queue_draw(a->enigma_canvas);
        break;
    }
}
gboolean enigma_view_tick(GtkWidget *widget, GdkFrameClock *clock, gpointer data) {
    (void)clock;
    App *a = data;
    if (!a->animating)
        return G_SOURCE_CONTINUE;
    double elapsed = lab_now() - a->animation_start;
    size_t index = (size_t)(elapsed / a->animation_duration);
    if (index >= a->trace_count) {
        a->trace_index = a->trace_count ? a->trace_count - 1 : 0;
        a->animating = false;
    } else
        a->trace_index = index;
    gtk_widget_queue_draw(widget);
    return G_SOURCE_CONTINUE;
}
