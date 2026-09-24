#include "app.h"
#include "enigma_view.h"
#include <glib/gstdio.h>
#include <math.h>
#include <string.h>

static App app;
static unsigned page_index;
static bool sockets_checked, keyboard_checked, wide;
static const char *pages[] = {"enigma", "plugboard", "message", "menu",
                              "bombe",  "benchmark", "bombe"};
static bool capture(const char *name) {
    const char *directory = g_getenv("LAB_SCREENSHOT_DIR");
    if (!directory)
        return true;
    g_assert_cmpint(g_mkdir_with_parents(directory, 0755), ==, 0);
    GdkPaintable *paintable = gtk_widget_paintable_new(app.window);
    GtkSnapshot *snapshot = gtk_snapshot_new();
    int width = gtk_widget_get_width(app.window), height = gtk_widget_get_height(app.window);
    gdk_paintable_snapshot(paintable, GDK_SNAPSHOT(snapshot), width, height);
    GskRenderNode *node = gtk_snapshot_free_to_node(snapshot);
    if (!node) {
        g_object_unref(paintable);
        return false;
    }
    graphene_rect_t rect = GRAPHENE_RECT_INIT(0, 0, width, height);
    GdkTexture *texture =
        gsk_renderer_render_texture(gtk_native_get_renderer(GTK_NATIVE(app.window)), node, &rect);
    char *path = g_build_filename(directory, name, NULL);
    g_assert_true(gdk_texture_save_to_png(texture, path));
    g_free(path);
    g_object_unref(texture);
    gsk_render_node_unref(node);
    g_object_unref(paintable);
    return true;
}

static gboolean tick(gpointer unused) {
    (void)unused;
    GtkWidget *page = gtk_stack_get_visible_child(GTK_STACK(app.stack));
    GtkWidget *scroll = g_object_get_data(G_OBJECT(page), "page-scroll");
    GtkAdjustment *adjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scroll));
    double overflow =
        gtk_adjustment_get_upper(adjustment) - gtk_adjustment_get_page_size(adjustment);
    g_print("%s at %dx%d: page overflow %.0f px\n", pages[page_index],
            gtk_widget_get_width(app.window), gtk_widget_get_height(app.window), overflow);
    g_assert_cmpfloat(overflow, <=, 1);
    if (page_index == 0 && !keyboard_checked) {
        GtkWidget *toolbar = gtk_widget_get_parent(gtk_widget_get_parent(app.key_entry));
        graphene_rect_t windows, keyboard;
        g_assert_true(gtk_widget_compute_bounds(app.positions, toolbar, &windows));
        g_assert_true(gtk_widget_compute_bounds(app.key_entry, toolbar, &keyboard));
        g_assert_cmpfloat(fabs(windows.origin.y - keyboard.origin.y), <=, 2);
        gtk_window_set_focus(GTK_WINDOW(app.window), NULL);
        unsigned visited = 0;
        bool reached_keyboard = false;
        while (gtk_widget_child_focus(toolbar, GTK_DIR_TAB_FORWARD)) {
            GtkWidget *focused = gtk_window_get_focus(GTK_WINDOW(app.window));
            reached_keyboard |= focused == app.key_entry ||
                                (focused && gtk_widget_is_ancestor(focused, app.key_entry));
            g_assert_cmpuint(++visited, <, 40);
        }
        g_assert_true(reached_keyboard);
        gtk_window_set_focus(GTK_WINDOW(app.window), NULL);
        keyboard_checked = true;
        return G_SOURCE_CONTINUE;
    }
    if (page_index == 1 && !sockets_checked) {
        /* Socket hit testing must follow the same aspect-preserving transform as drawing. */
        double width = gtk_widget_get_width(app.plug_canvas);
        double height = gtk_widget_get_height(app.plug_canvas);
        double scale = fmin(width / 1100, height / 420);
        double left = (width - 1100 * scale) / 2;
        double top = (height - 420 * scale) / 2;
        plugboard_pressed(NULL, 1, left + 80 * scale, top + 95 * scale, &app);
        plugboard_pressed(NULL, 1, left + 156 * scale, top + 95 * scale, &app);
        g_assert_cmpuint(app.key.plug[0], ==, 1);
        g_assert_cmpuint(app.key.plug[1], ==, 0);
        plugboard_pressed(NULL, 1, left + 80 * scale, top + 95 * scale, &app);
        g_assert_cmpuint(app.key.plug[0], ==, 0);
        sockets_checked = true;
        return G_SOURCE_CONTINUE; /* Allow the changed canvas to render before capture. */
    }
    char *name = g_strdup_printf("%s%s.png", wide ? "wide-" : "",
                                 page_index == 6 ? "bombe-no-crib" : pages[page_index]);
    bool captured = capture(name);
    g_free(name);
    if (!captured)
        return G_SOURCE_CONTINUE;
    if (++page_index == G_N_ELEMENTS(pages)) {
        g_application_quit(G_APPLICATION(app.application));
        return G_SOURCE_REMOVE;
    }
    if (page_index == 6)
        gtk_drop_down_set_selected(GTK_DROP_DOWN(app.mode), 3);
    gtk_stack_set_visible_child_name(GTK_STACK(app.stack), pages[page_index]);
    return G_SOURCE_CONTINUE;
}
static void activate(GtkApplication *application, gpointer unused) {
    (void)unused;
    app.initial_threads = 12;
    app_activate(application, &app);
    gtk_widget_set_visible(app.tutorial_panel, FALSE);
    gtk_stack_set_transition_type(GTK_STACK(app.stack), GTK_STACK_TRANSITION_TYPE_NONE);
    gtk_window_set_default_size(GTK_WINDOW(app.window), wide ? 1900 : 1366, wide ? 1000 : 768);
    gtk_window_set_resizable(GTK_WINDOW(app.window), FALSE);
    app_example(NULL, &app);
    app_start(NULL, &app);
    gtk_stack_set_visible_child_name(GTK_STACK(app.stack), pages[0]);
    g_timeout_add(500, tick, NULL);
}
int main(int argc, char **argv) {
    wide = argc == 2 && !strcmp(argv[1], "wide");
    char *config = g_dir_make_tmp("enigma-layout-XXXXXX", NULL);
    g_assert_nonnull(config);
    g_setenv("XDG_CONFIG_HOME", config, TRUE);
    if (!gtk_init_check()) {
        g_rmdir(config);
        g_free(config);
        return 77;
    }
    GtkApplication *application =
        gtk_application_new("org.enigmabombelab.LayoutTest", G_APPLICATION_NON_UNIQUE);
    g_signal_connect(application, "activate", G_CALLBACK(activate), NULL);
    int result = g_application_run(G_APPLICATION(application), 1, argv);
    app_destroy(&app);
    g_object_unref(application);
    char *dir = g_build_filename(config, "enigma-bombe-lab", NULL);
    char *marker = g_build_filename(dir, "tutorial-seen", NULL);
    g_remove(marker);
    g_rmdir(dir);
    g_rmdir(config);
    g_free(marker);
    g_free(dir);
    g_free(config);
    return result;
}
