#include "scenario.h"
#include <glib/gstdio.h>
#include <string.h>

int main(void) {
    GError *error = NULL;
    char *directory = g_dir_make_tmp("enigma-scenario-test-XXXXXX", &error);
    g_assert_no_error(error);
    char *path = g_build_filename(directory, "scenario.ini", NULL);
    GKeyFile *file = g_key_file_new();
    const char *prefix = "[Message]\nplaintext=HELLO\n[Ignored]\npadding=";
    char *data = g_malloc(LAB_SCENARIO_MAX_BYTES + 2);
    memset(data, 'x', LAB_SCENARIO_MAX_BYTES + 2);
    memcpy(data, prefix, strlen(prefix));

    /* Valid input below and exactly at the limit remains readable. */
    const gsize sizes[] = {strlen(prefix), LAB_SCENARIO_MAX_BYTES - 1, LAB_SCENARIO_MAX_BYTES};
    for (gsize i = 0; i < G_N_ELEMENTS(sizes); i++) {
        g_assert_true(g_file_set_contents(path, data, sizes[i], &error));
        g_assert_no_error(error);
        g_assert_true(scenario_load_file(file, path, &error));
        g_assert_no_error(error);
        char *plain = g_key_file_get_string(file, "Message", "plaintext", &error);
        g_assert_no_error(error);
        g_assert_cmpstr(plain, ==, "HELLO");
        g_free(plain);
    }

    /* Even unused keys count; rejection must happen before parsing. */
    g_assert_true(g_file_set_contents(path, data, LAB_SCENARIO_MAX_BYTES + 1, &error));
    g_assert_no_error(error);
    g_assert_false(scenario_load_file(file, path, &error));
    g_assert_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED);
    g_assert_nonnull(strstr(error->message, "64 KiB"));
    g_clear_error(&error);
    char *plain = g_key_file_get_string(file, "Message", "plaintext", &error);
    g_assert_no_error(error);
    g_assert_cmpstr(plain, ==, "HELLO");
    g_free(plain);

    g_assert_true(g_file_set_contents(path, "not an INI file", -1, &error));
    g_assert_no_error(error);
    g_assert_false(scenario_load_file(file, path, &error));
    g_assert_error(error, G_KEY_FILE_ERROR, G_KEY_FILE_ERROR_PARSE);
    g_clear_error(&error);
    g_assert_cmpint(g_remove(path), ==, 0);
    g_assert_false(scenario_load_file(file, path, &error));
    g_assert_error(error, G_FILE_ERROR, G_FILE_ERROR_NOENT);
    g_clear_error(&error);

    g_key_file_unref(file);
    g_free(data);
    g_free(path);
    g_assert_cmpint(g_rmdir(directory), ==, 0);
    g_free(directory);
    return 0;
}
