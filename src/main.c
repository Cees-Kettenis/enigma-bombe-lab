#include "app.h"
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv) {
    App *app = g_new0(App, 1);
    app->initial_threads = MIN(lab_cpu_count(), LAB_MAX_WORKERS);
    gint threads = 0, smoke = 0;
    gboolean benchmark = FALSE, tutorial = FALSE;
    gchar **files = NULL;
    GOptionEntry entries[] = {
        {"threads", 't', 0, G_OPTION_ARG_INT, &threads, "Number of virtual Bombe workers", "N"},
        {"tutorial", 0, 0, G_OPTION_ARG_NONE, &tutorial, "Walk through encryption and codebreaking",
         NULL},
        {"benchmark", 'b', 0, G_OPTION_ARG_NONE, &benchmark, "Open GUI and start CPU benchmark",
         NULL},
        {"smoke-test", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_INT, &smoke,
         "Run example and close after N milliseconds", "N"},
        {G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &files, "Scenario INI file",
         "[scenario.ini]"},
        {NULL, 0, 0, 0, NULL, NULL, NULL}};
    GOptionContext *context = g_option_context_new("- Enigma Bombe Lab");
    g_option_context_add_main_entries(context, entries, NULL);
    GError *error = NULL;
    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        g_printerr("%s\n", error->message);
        g_clear_error(&error);
        g_option_context_free(context);
        g_free(app);
        return 2;
    }
    g_option_context_free(context);
    if (threads < 0 || threads > LAB_MAX_WORKERS || (files && files[0] && files[1])) {
        g_printerr("Use 1-%d workers and at most one scenario file.\n", LAB_MAX_WORKERS);
        g_strfreev(files);
        g_free(app);
        return 2;
    }
    if (threads)
        app->initial_threads = (unsigned)threads;
    if (tutorial && benchmark) {
        g_printerr("Choose --tutorial or --benchmark, not both.\n");
        g_strfreev(files);
        g_free(app);
        return 2;
    }
    app->tutorial_requested = tutorial;
    app->benchmark_requested = benchmark;
    app->smoke_ms = smoke > 0 ? (unsigned)smoke : 0;
    if (files)
        app->initial_file = g_strdup(files[0]);
    g_strfreev(files);
    GtkApplication *application =
        gtk_application_new("org.enigmabombelab.Lab", G_APPLICATION_NON_UNIQUE);
    g_signal_connect(application, "activate", G_CALLBACK(app_activate), app);
    int status = g_application_run(G_APPLICATION(application), 1, argv);
    if (app->smoke_ms && (app->benchmark_requested ? !app->benchmark_count : !app->solved_count))
        status = 1;
    app_destroy(app);
    g_object_unref(application);
    g_free(app);
    return status;
}
