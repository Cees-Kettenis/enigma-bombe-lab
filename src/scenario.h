#ifndef SCENARIO_H
#define SCENARIO_H
#include <glib.h>

#define LAB_SCENARIO_MAX_BYTES (64 * 1024)

/* Limits actual bytes read before handing any input to the INI parser. */
gboolean scenario_load_file(GKeyFile *file, const char *path, GError **error);
#endif
