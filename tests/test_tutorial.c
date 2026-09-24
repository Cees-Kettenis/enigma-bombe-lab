#include "app.h"
#include "scenario.h"
#include <glib/gstdio.h>
#include <string.h>

/* Exercises the same buttons as a user, including the real background solver.
 * With LAB_SCREENSHOT_DIR set, also captures documentation images from GTK. */
static App app;
static unsigned phase;
static char *expected_plain, *expected_cipher;
static EnigmaKey expected_key;
static double deadline;

static void check_oversized_import(void) {
    GError *error = NULL;
    char *directory = g_dir_make_tmp("enigma-import-test-XXXXXX", &error);
    g_assert_no_error(error);
    char *path = g_build_filename(directory, "oversized.ini", NULL);
    char *contents = g_malloc0(LAB_SCENARIO_MAX_BYTES + 2);
    memset(contents, 'x', LAB_SCENARIO_MAX_BYTES + 1);
    g_assert_true(g_file_set_contents(path, contents, LAB_SCENARIO_MAX_BYTES + 1, &error));
    g_assert_no_error(error);
    char *old_path = g_strdup(gtk_editable_get_text(GTK_EDITABLE(app.scenario_path)));
    char *old_status = g_strdup(gtk_label_get_text(GTK_LABEL(app.status)));
    char *plain = app_text(app.plain), *cipher = app_text(app.cipher);
    EnigmaKey key = app.key;
    Challenge challenge = app.challenge;
    gtk_editable_set_text(GTK_EDITABLE(app.scenario_path), path);
    app_load(NULL, &app);
    g_assert_nonnull(strstr(gtk_label_get_text(GTK_LABEL(app.status)), "64 KiB"));
    char *after_plain = app_text(app.plain), *after_cipher = app_text(app.cipher);
    g_assert_cmpstr(after_plain, ==, plain);
    g_assert_cmpstr(after_cipher, ==, cipher);
    g_assert_cmpmem(&app.key, sizeof key, &key, sizeof key);
    g_assert_cmpmem(&app.challenge, sizeof challenge, &challenge, sizeof challenge);
    gtk_editable_set_text(GTK_EDITABLE(app.scenario_path), old_path);
    app_status(&app, "%s", old_status);
    g_free(old_status);
    g_free(after_plain);
    g_free(after_cipher);
    g_free(plain);
    g_free(cipher);
    g_free(old_path);
    g_free(contents);
    g_assert_cmpint(g_remove(path), ==, 0);
    g_assert_cmpint(g_rmdir(directory), ==, 0);
    g_free(path);
    g_free(directory);
}

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

static void next(unsigned expected_step) {
    g_signal_emit_by_name(app.tutorial_next, "clicked");
    g_assert_cmpuint(app.tutorial_step, ==, expected_step);
}

static gboolean tick(gpointer data) {
    (void)data;
    g_assert_true(lab_now() < deadline);
    switch (phase) {
    case 0:
        g_assert_true(gtk_widget_get_visible(app.tutorial_panel));
        g_assert_cmpuint(app.tutorial_step, ==, 0);
        if (!capture("tutorial-compact.png"))
            return G_SOURCE_CONTINUE;
        /* Opening the guide does not replace an existing message. */
        app_set_text(app.plain, "KEEP THIS UNTIL I START");
        app_tutorial(NULL, &app);
        char *untouched = app_text(app.plain);
        g_assert_cmpstr(untouched, ==, "KEEP THIS UNTIL I START");
        g_free(untouched);
        app.random_seed = 20260923;
        next(1);
        break;
    case 1:
        next(2);
        g_assert_cmpuint(app.key.ring[0] + app.key.ring[1] + app.key.ring[2], ==, 0);
        g_assert_cmpuint(app.key.reflector, ==, 0);
        break;
    case 2:
        if (!capture("plugboard.png"))
            return G_SOURCE_CONTINUE;
        next(3);
        break;
    case 3: {
        char *starter = app_text(app.plain);
        app_set_text(app.plain, "A SHORT MESSAGE WITHOUT THE LESSON CRIB");
        next(3); /* Invalid lesson text must not advance. */
        app_set_text(app.plain, starter);
        g_free(starter);
        next(4);
        char *text = app_text(app.plain);
        expected_plain = g_malloc(LAB_TEXT_MAX);
        enigma_normalize(text, expected_plain, LAB_TEXT_MAX);
        g_free(text);
        expected_cipher = app_text(app.cipher);
        expected_key = app.key;
        gtk_stack_set_visible_child_name(GTK_STACK(app.stack), "message");
        break;
    }
    case 4:
        if (!capture("message.png"))
            return G_SOURCE_CONTINUE;
        gtk_stack_set_visible_child_name(GTK_STACK(app.stack), "enigma");
        break;
    case 5: {
        if (!capture("enigma.png"))
            return G_SOURCE_CONTINUE;
        /* A changed key must not silently replace the encrypted message. */
        gtk_editable_set_text(GTK_EDITABLE(app.positions), "ZZZ");
        next(4);
        app.key = expected_key;
        app_sync_key(&app);
        next(5);
        g_assert_true(app.challenge.present);
        g_assert_false(app.challenge.visible);
        g_assert_cmpmem(&app.challenge.key, sizeof expected_key, &expected_key,
                        sizeof expected_key);
        char *cipher = app_text(app.cipher), *plain = app_text(app.plain);
        g_assert_cmpstr(cipher, ==, expected_cipher);
        g_assert_cmpstr(plain, ==, "");
        g_free(cipher);
        g_free(plain);
        g_assert_true(app.menu_valid);
        check_oversized_import();
        break;
    }
    case 6:
        if (!capture("crib.png"))
            return G_SOURCE_CONTINUE;
        next(6);
        break;
    case 7:
        next(7);
        g_assert_true(app.snapshot.running);
        g_assert_false(gtk_widget_get_sensitive(app.tutorial_next));
        break;
    case 8:
        if (!capture("bombe.png"))
            return G_SOURCE_CONTINUE;
        break;
    case 9:
        if (app.snapshot.running || !app.result_count)
            return G_SOURCE_CONTINUE;
        g_assert_cmpuint(app.snapshot.tested, ==, app.snapshot.total);
        g_assert_cmpuint(app.result_count, >, 0);
        next(8);
        char *recovered = app_text(app.decrypted);
        g_assert_cmpstr(recovered, ==, expected_plain);
        g_free(recovered);
        /* Verify the app's focus target even if the user's desktop is active elsewhere. */
        g_assert_true(gtk_window_get_focus(GTK_WINDOW(app.window)) == app.decrypted);
        break;
    case 10:
        if (!capture("recovered.png"))
            return G_SOURCE_CONTINUE;
        next(8);
        g_assert_false(gtk_widget_get_visible(app.tutorial_panel));
        app_tutorial(NULL, &app);
        g_assert_cmpuint(app.tutorial_step, ==, 0);
        g_assert_true(gtk_widget_get_visible(app.tutorial_panel));
        g_print("Tutorial: real encryption, same-key intercept, crib search, recovery and restart "
                "passed.\n");
        g_application_quit(G_APPLICATION(app.application));
        return G_SOURCE_REMOVE;
    default:
        g_assert_not_reached();
    }
    phase++;
    return G_SOURCE_CONTINUE;
}

static void activate(GtkApplication *application, gpointer data) {
    (void)data;
    app.initial_threads = 4;
    app.tutorial_requested = true;
    app_activate(application, &app);
    g_assert_nonnull(strstr(gtk_label_get_text(GTK_LABEL(app.status)), "64 KiB"));
    char *initial_plain = app_text(app.plain);
    g_assert_cmpstr(initial_plain, ==, "WETTERBERICHT");
    g_assert_false(app.challenge.present);
    g_free(initial_plain);
    /* Return to the normal lesson after verifying the deliberately invalid startup file. */
    gtk_editable_set_text(GTK_EDITABLE(app.scenario_path), "scenario.ini");
    app_status(&app, "Ready to play. Start the tutorial for a guided first run.");
    gtk_stack_set_transition_type(GTK_STACK(app.stack), GTK_STACK_TRANSITION_TYPE_NONE);
    const char *size = g_getenv("LAB_TEST_SMALL_WINDOW");
    if (size)
        gtk_window_set_default_size(GTK_WINDOW(app.window), 820, 600);
    gtk_window_set_resizable(GTK_WINDOW(app.window), FALSE);
    deadline = lab_now() + 100;
    g_timeout_add(350, tick, NULL);
}

int main(int argc, char **argv) {
    char *config = g_dir_make_tmp("enigma-tutorial-test-XXXXXX", NULL);
    g_assert_nonnull(config);
    g_setenv("XDG_CONFIG_HOME", config, TRUE);
    if (!gtk_init_check()) {
        g_print("Tutorial test needs a graphical display.\n");
        g_rmdir(config);
        g_free(config);
        return 77;
    }
    char *startup_path = g_build_filename(config, "oversized.ini", NULL);
    char *oversized = g_malloc0(LAB_SCENARIO_MAX_BYTES + 1);
    g_assert_true(g_file_set_contents(startup_path, oversized, LAB_SCENARIO_MAX_BYTES + 1, NULL));
    g_free(oversized);
    app.initial_file = g_strdup(startup_path);
    GtkApplication *application =
        gtk_application_new("org.enigmabombelab.TutorialTest", G_APPLICATION_NON_UNIQUE);
    g_signal_connect(application, "activate", G_CALLBACK(activate), NULL);
    int result = g_application_run(G_APPLICATION(application), argc, argv);
    app_destroy(&app);
    g_assert_cmpint(g_remove(startup_path), ==, 0);
    g_free(startup_path);
    g_object_unref(application);
    g_free(expected_plain);
    g_free(expected_cipher);
    char *marker = g_build_filename(config, "enigma-bombe-lab", "tutorial-seen", NULL);
    char *subdir = g_path_get_dirname(marker);
    g_remove(marker);
    g_rmdir(subdir);
    g_rmdir(config);
    g_free(marker);
    g_free(subdir);
    g_free(config);
    return result;
}
