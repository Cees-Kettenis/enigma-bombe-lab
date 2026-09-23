#include "app.h"
#include "blind.h"
#include <glib/gstdio.h>
#include <string.h>

static App app;
static char *expected, *roundtrip;
static unsigned phase;
static double deadline;
static bool known, different;

static gboolean tick(gpointer unused) {
    (void)unused;
    g_assert_true(lab_now() < deadline);
    if (phase == 0) {
        for (GtkWidget *row = gtk_widget_get_first_child(app.results); row;
             row = gtk_widget_get_next_sibling(row)) {
            Candidate *c = g_object_get_data(G_OBJECT(row), "candidate");
            g_assert_true(c->heuristic);
            g_assert_cmpfloat(c->score, ==, blind_english_score(c->plaintext));
            const char *label =
                gtk_label_get_text(GTK_LABEL(gtk_list_box_row_get_child(GTK_LIST_BOX_ROW(row))));
            char score[80];
            g_snprintf(score, sizeof score, "English confidence %.1f%%",
                       blind_confidence_percent(c->score));
            g_assert_nonnull(strstr(label, score));
            g_assert_null(strstr(label, "Verified"));
            g_assert_null(strstr(label, "Demo check"));
            g_assert_nonnull(strstr(gtk_widget_get_tooltip_text(row), "No original answer"));
            if (!strcmp(c->plaintext, expected)) {
                app_candidate_activated(GTK_LIST_BOX(app.results), GTK_LIST_BOX_ROW(row), &app);
                g_assert_nonnull(strstr(gtk_label_get_text(GTK_LABEL(app.status)),
                                        "No original answer was checked"));
                app_stop(NULL, &app);
                phase = 1;
                break;
            }
        }
    } else if (!app.snapshot.running) {
        char *text = app_text(app.decrypted);
        g_assert_cmpstr(text, ==, expected);
        g_free(text);
        if (different)
            app.challenge.key.start[0] = 0;
        gtk_editable_set_text(GTK_EDITABLE(app.scenario_path), roundtrip);
        app_save(NULL, &app);
        gtk_drop_down_set_selected(GTK_DROP_DOWN(app.mode), 0);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(app.blind_restarts), 9);
        app_load(NULL, &app);
        g_assert_cmpuint(gtk_drop_down_get_selected(GTK_DROP_DOWN(app.mode)), ==, 3);
        g_assert_cmpint(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(app.blind_restarts)), ==,
                        2);
        g_print("No-crib GUI: readable labels, verification, tooltip, recovery, manual stop "
                "and scenario roundtrip passed.\n");
        g_application_quit(G_APPLICATION(app.application));
        return G_SOURCE_REMOVE;
    }
    return G_SOURCE_CONTINUE;
}

static void activate(GtkApplication *application, gpointer unused) {
    (void)unused;
    app.initial_threads = 2;
    app_activate(application, &app);
    expected = g_strdup(app.challenge.plaintext);
    g_assert_cmpuint(strlen(expected), >, 500);
    if (!known)
        memset(&app.challenge, 0, sizeof app.challenge);
    else if (different)
        app.challenge.key.start[0] = 1; /* Same plaintext, unrelated ciphertext. */
    /* These controls are not no-crib inputs and must not gate search. */
    gtk_drop_down_set_selected(GTK_DROP_DOWN(app.rotor[0]), 0);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(app.rotor[1]), 0);
    gtk_editable_set_text(GTK_EDITABLE(app.positions), "???");
    gtk_editable_set_text(GTK_EDITABLE(app.plug_text), "invalid");
    gtk_editable_set_text(GTK_EDITABLE(app.crib), "");
    app_start(NULL, &app);
    g_assert_true(app.snapshot.running && app.active_spec.blind);
    g_assert_cmpstr(app.active_spec.crib, ==, "");
    deadline = lab_now() + 20;
    g_timeout_add(100, tick, NULL);
}

int main(int argc, char **argv) {
    g_assert_true(argc == 2 || argc == 3);
    known = argc == 3;
    different = known && !strcmp(argv[2], "different");
    char *config = g_dir_make_tmp("enigma-blind-gui-XXXXXX", NULL);
    g_assert_nonnull(config);
    g_setenv("XDG_CONFIG_HOME", config, TRUE);
    if (!gtk_init_check()) {
        g_rmdir(config);
        g_free(config);
        return 77;
    }
    app.initial_file = g_strdup(argv[1]);
    roundtrip = g_build_filename(config, "roundtrip.ini", NULL);
    GtkApplication *application =
        gtk_application_new("org.enigmabombelab.BlindTest", G_APPLICATION_NON_UNIQUE);
    g_signal_connect(application, "activate", G_CALLBACK(activate), NULL);
    int result = g_application_run(G_APPLICATION(application), 1, argv);
    app_destroy(&app);
    g_object_unref(application);
    g_remove(roundtrip);
    char *dir = g_build_filename(config, "enigma-bombe-lab", NULL);
    char *marker = g_build_filename(dir, "tutorial-seen", NULL);
    g_remove(marker);
    g_rmdir(dir);
    g_rmdir(config);
    g_free(marker);
    g_free(dir);
    g_free(config);
    g_free(expected);
    g_free(roundtrip);
    return result;
}
