#ifndef APP_H
#define APP_H
#include "benchmark.h"
#include <gtk/gtk.h>
typedef struct {
    bool present, visible;
    EnigmaKey key;
    char plaintext[LAB_TEXT_MAX];
} Challenge;
typedef struct App {
    GtkApplication *application;
    GtkWidget *window, *stack, *status;
    GtkWidget *tutorial_panel, *tutorial_title, *tutorial_text, *tutorial_next;
    unsigned tutorial_step;
    bool tutorial_requested;
    GtkWidget *rotor[3], *rings, *positions, *reflector, *plug_text, *secret_label;
    GtkWidget *plain, *cipher, *decrypted, *crib, *alignment, *menu_label;
    GtkWidget *enigma_canvas, *plug_canvas, *menu_canvas, *bombe_canvas, *graph_canvas,
        *benchmark_canvas;
    GtkWidget *threads, *speed, *mode, *stop_limit, *auto_encrypt, *random_rings, *progress;
    GtkWidget *blind_restarts, *stop_confidence;
    GtkWidget *stats, *results, *benchmark_text, *scenario_path, *key_entry;
    GtkWidget *start_button, *pause_button, *resume_button, *stop_button, *benchmark_button;
    EnigmaKey key;
    Challenge challenge;
    uint64_t random_seed;
    EnigmaTrace traces[LAB_TEXT_MAX];
    size_t trace_count, trace_index;
    double animation_start, animation_duration;
    bool animating, loading, reduced_motion, started_search, benchmark_requested;
    unsigned initial_threads, smoke_ms;
    char *initial_file;
    int plug_selected;
    Menu menu;
    bool menu_valid;
    SearchPool *pool;
    unsigned pool_threads;
    SearchSpec active_spec;
    WorkerSnapshot *workers, *previous_workers;
    double *worker_rates;
    double *worker_flashes;
    SearchSnapshot snapshot;
    Benchmark benchmark;
    BenchmarkRow benchmark_rows[BENCHMARK_ROWS];
    unsigned benchmark_count;
    bool benchmark_running;
    double history[240], history_max, last_sample;
    unsigned history_count, history_head;
    uint64_t last_tested;
    unsigned result_count;
    guint timer, encrypt_timer;
} App;
void app_activate(GtkApplication *application, gpointer data);
void app_destroy(App *app);
void app_status(App *app, const char *format, ...) G_GNUC_PRINTF(2, 3);
char *app_text(GtkWidget *view);
void app_set_text(GtkWidget *view, const char *text);
bool app_read_key(App *app);
void app_sync_key(App *app);
void app_encrypt(GtkButton *button, gpointer data);
void app_reset(GtkButton *button, gpointer data);
void app_random_key(GtkButton *button, gpointer data);
void app_random_plugs(GtkButton *button, gpointer data);
void app_clear_plugs(GtkButton *button, gpointer data);
void app_apply_plugs(GtkButton *button, gpointer data);
void app_intercept(GtkButton *button, gpointer data);
void app_example(GtkButton *button, gpointer data);
void app_reveal(GtkButton *button, gpointer data);
void app_hide(GtkButton *button, gpointer data);
void app_copy(GtkButton *button, gpointer data);
void app_start(GtkButton *button, gpointer data);
void app_pause(GtkButton *button, gpointer data);
void app_resume(GtkButton *button, gpointer data);
void app_stop(GtkButton *button, gpointer data);
void app_benchmark(GtkButton *button, gpointer data);
void app_save(GtkButton *button, gpointer data);
void app_load(GtkButton *button, gpointer data);
void app_tutorial(GtkButton *button, gpointer data);
void tutorial_build(App *app, GtkWidget *parent);
void tutorial_refresh(App *app);
bool app_capture_intercept(App *app);
void app_keep_intercept(GtkButton *button, gpointer data);
void app_menu_changed(GtkWidget *widget, gpointer data);
void app_auto_alignment(GtkButton *button, gpointer data);
void app_previous_alignment(GtkButton *button, gpointer data);
void app_next_alignment(GtkButton *button, gpointer data);
void app_plain_changed(GtkTextBuffer *buffer, gpointer data);
void app_candidate_activated(GtkListBox *box, GtkListBoxRow *row, gpointer data);
void app_key_entered(GtkEntry *entry, gpointer data);
gboolean app_keyboard_press(GtkEventControllerKey *controller, guint keyval, guint keycode,
                            GdkModifierType state, gpointer data);
#endif
