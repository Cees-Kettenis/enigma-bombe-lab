#include "scenario.h"
#include <errno.h>
#include <glib/gstdio.h>
#include <stdio.h>

gboolean scenario_load_file(GKeyFile *file, const char *path, GError **error) {
    FILE *input = g_fopen(path, "rb");
    if (!input) {
        int saved_errno = errno;
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(saved_errno),
                    "Cannot open scenario: %s", g_strerror(saved_errno));
        return FALSE;
    }
    /* The extra byte distinguishes an exact-limit file from an oversized one.
     * A stat-only check would not bound a file that grows while being read. */
    char *buffer = g_malloc(LAB_SCENARIO_MAX_BYTES + 1);
    size_t length = fread(buffer, 1, LAB_SCENARIO_MAX_BYTES + 1, input);
    gboolean ok = FALSE;
    if (ferror(input)) {
        int saved_errno = errno;
        g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(saved_errno),
                    "Cannot read scenario: %s", g_strerror(saved_errno));
    } else if (length > LAB_SCENARIO_MAX_BYTES) {
        g_set_error_literal(error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                            "Scenario exceeds the 64 KiB file size limit.");
    } else {
        ok = g_key_file_load_from_data(file, buffer, length, G_KEY_FILE_NONE, error);
    }
    fclose(input);
    g_free(buffer);
    return ok;
}
