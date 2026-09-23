#include "app.h"
#include "gui.h"
#include "plugboard.h"
#include "rotor.h"
#include "scenario.h"
#include <glib/gstdio.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
static const char *example_plain = "WETTERBERICHTTHEQUICKBROWNFOXJUMPSOVERTHELAZYDOGPACKMYBOXWITHFI"
                                   "VEDOZENLIQUORJUGSANGRIFFBEIMORGENGRAUENWINDWESTSTAERKEFUENF";
static const char *example_crib =
    "WETTERBERICHTTHEQUICKBROWNFOXJUMPSOVERTHELAZYDOGPACKMYBOXWITHFIVEDOZENLIQUORJUGS";
void app_status(App *a, const char *format, ...) {
    va_list args;
    va_start(args, format);
    char *s = g_strdup_vprintf(format, args);
    va_end(args);
    gtk_label_set_text(GTK_LABEL(a->status), s);
    g_free(s);
}
char *app_text(GtkWidget *v) {
    GtkTextBuffer *b = gtk_text_view_get_buffer(GTK_TEXT_VIEW(v));
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(b, &start, &end);
    return gtk_text_buffer_get_text(b, &start, &end, FALSE);
}
void app_set_text(GtkWidget *v, const char *text) {
    gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(v)), text, -1);
}
static bool letters3(const char *text, uint8_t out[3]) {
    if (strlen(text) != 3)
        return false;
    for (int i = 0; i < 3; i++) {
        char c = g_ascii_toupper(text[i]);
        if (c < 'A' || c > 'Z')
            return false;
        out[i] = (uint8_t)(c - 'A');
    }
    return true;
}
static void format3(const uint8_t in[3], char out[4]) {
    for (int i = 0; i < 3; i++)
        out[i] = (char)('A' + in[i]);
    out[3] = 0;
}
bool app_read_key(App *a) {
    EnigmaKey k = a->key;
    for (int i = 0; i < 3; i++)
        k.order[i] = (uint8_t)gtk_drop_down_get_selected(GTK_DROP_DOWN(a->rotor[i]));
    k.reflector = (uint8_t)gtk_drop_down_get_selected(GTK_DROP_DOWN(a->reflector));
    if (!letters3(gtk_editable_get_text(GTK_EDITABLE(a->rings)), k.ring) ||
        !letters3(gtk_editable_get_text(GTK_EDITABLE(a->positions)), k.start)) {
        app_status(a, "Rings and start windows must each contain three letters A-Z.");
        return false;
    }
    if (!plugboard_parse(k.plug, gtk_editable_get_text(GTK_EDITABLE(a->plug_text)))) {
        app_status(a, "Invalid plugboard. Use at most ten distinct pairs, for example AG BL CZ.");
        return false;
    }
    if (!enigma_key_valid(&k)) {
        app_status(a, "Select three different rotors.");
        return false;
    }
    a->key = k;
    return true;
}
void app_sync_key(App *a) {
    for (int i = 0; i < 3; i++)
        gtk_drop_down_set_selected(GTK_DROP_DOWN(a->rotor[i]), a->key.order[i]);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(a->reflector), a->key.reflector);
    char s[4], pairs[80];
    format3(a->key.ring, s);
    gtk_editable_set_text(GTK_EDITABLE(a->rings), s);
    format3(a->key.start, s);
    gtk_editable_set_text(GTK_EDITABLE(a->positions), s);
    plugboard_format(a->key.plug, pairs);
    gtk_editable_set_text(GTK_EDITABLE(a->plug_text), pairs);
    gtk_widget_queue_draw(a->plug_canvas);
    gtk_widget_queue_draw(a->enigma_canvas);
}
static void update_secret(App *a) {
    if (!a->challenge.present) {
        gtk_label_set_text(GTK_LABEL(a->secret_label), "No challenge generated.");
        return;
    }
    if (!a->challenge.visible) {
        gtk_label_set_text(
            GTK_LABEL(a->secret_label),
            "Secret key hidden. Solver input contains only ciphertext, crib, rings and reflector.");
        return;
    }
    EnigmaKey *k = &a->challenge.key;
    char pairs[80], text[240];
    plugboard_format(k->plug, pairs);
    g_snprintf(text, sizeof text, "%s %s %s / %c%c%c / rings %c%c%c / UKW %c / %s",
               rotor_names[k->order[0]], rotor_names[k->order[1]], rotor_names[k->order[2]],
               'A' + k->start[0], 'A' + k->start[1], 'A' + k->start[2], 'A' + k->ring[0],
               'A' + k->ring[1], 'A' + k->ring[2], 'B' + k->reflector, pairs);
    gtk_label_set_text(GTK_LABEL(a->secret_label), text);
}
void app_encrypt(GtkButton *button, gpointer data) {
    (void)button;
    App *a = data;
    if (!app_read_key(a))
        return;
    char *input = app_text(a->plain);
    if (strlen(input) >= LAB_TEXT_MAX) {
        app_status(a, "The message limit is %d bytes. Shorten the plaintext.", LAB_TEXT_MAX - 1);
        g_free(input);
        return;
    }
    char plain[LAB_TEXT_MAX], cipher[LAB_TEXT_MAX];
    enigma_normalize(input, plain, sizeof plain);
    g_free(input);
    Enigma m;
    enigma_reset(&m, &a->key);
    a->trace_count = strlen(plain);
    for (size_t i = 0; i < a->trace_count; i++)
        cipher[i] = (char)('A' + enigma_press(&m, (uint8_t)(plain[i] - 'A'), &a->traces[i]));
    cipher[a->trace_count] = 0;
    app_set_text(a->cipher, cipher);
    a->challenge.present = false;
    update_secret(a);
    unsigned speed = gtk_drop_down_get_selected(GTK_DROP_DOWN(a->speed));
    const double durations[] = {1.5, .35, .06, 0};
    a->animation_duration = a->reduced_motion ? 0 : durations[MIN(speed, 3)];
    a->animating = a->animation_duration > 0 && a->trace_count > 0;
    a->trace_index = a->animating ? 0 : a->trace_count ? a->trace_count - 1 : 0;
    a->animation_start = lab_now();
    gtk_widget_queue_draw(a->enigma_canvas);
    app_status(a,
               "Encrypted %zu letters from the starting windows. Reset and encrypt ciphertext with "
               "the same key to decrypt.",
               a->trace_count);
}
static gboolean delayed_encrypt(gpointer data) {
    App *a = data;
    a->encrypt_timer = 0;
    if (gtk_check_button_get_active(GTK_CHECK_BUTTON(a->auto_encrypt)))
        app_encrypt(NULL, a);
    return G_SOURCE_REMOVE;
}
void app_plain_changed(GtkTextBuffer *buffer, gpointer data) {
    (void)buffer;
    App *a = data;
    if (a->loading || !gtk_check_button_get_active(GTK_CHECK_BUTTON(a->auto_encrypt)))
        return;
    if (a->encrypt_timer)
        g_source_remove(a->encrypt_timer);
    a->encrypt_timer = g_timeout_add(200, delayed_encrypt, a);
}
void app_key_entered(GtkEntry *entry, gpointer data) {
    App *a = data;
    char *old = app_text(a->plain),
         *joined = g_strconcat(old, gtk_editable_get_text(GTK_EDITABLE(entry)), NULL);
    a->loading = true;
    app_set_text(a->plain, joined);
    a->loading = false;
    gtk_editable_set_text(GTK_EDITABLE(entry), "");
    g_free(old);
    g_free(joined);
    app_encrypt(NULL, a);
}
gboolean app_keyboard_press(GtkEventControllerKey *controller, guint keyval, guint keycode,
                            GdkModifierType state, gpointer data) {
    (void)controller;
    (void)keycode;
    if (state & (GDK_CONTROL_MASK | GDK_ALT_MASK | GDK_SUPER_MASK))
        return FALSE;
    gunichar c = g_unichar_toupper(gdk_keyval_to_unicode(keyval));
    if (c < 'A' || c > 'Z')
        return FALSE;
    App *a = data;
    char *plain = app_text(a->plain);
    if (strlen(plain) + 1 >= LAB_TEXT_MAX) {
        app_status(a, "Message limit reached. Clear the plaintext to begin a new message.");
        g_free(plain);
        return TRUE;
    }
    char letter[2] = {(char)c, 0};
    char *next = g_strconcat(plain, letter, NULL);
    a->loading = true;
    app_set_text(a->plain, next);
    a->loading = false;
    g_free(plain);
    g_free(next);
    app_encrypt(NULL, a);
    if (a->trace_count) {
        a->trace_index = a->trace_count - 1;
        a->animation_start = lab_now() - (double)a->trace_index * a->animation_duration;
    }
    return TRUE;
}
void app_reset(GtkButton *button, gpointer data) {
    (void)button;
    App *a = data;
    if (!app_read_key(a))
        return;
    a->trace_count = 0;
    a->animating = false;
    app_sync_key(a);
    app_status(a, "Machine reset to starting windows. Encrypt always starts from this setting.");
}
void app_random_key(GtkButton *button, gpointer data) {
    (void)button;
    App *a = data;
    enigma_random_key(&a->key, gtk_check_button_get_active(GTK_CHECK_BUTTON(a->random_rings)),
                      &a->random_seed);
    a->trace_count = 0;
    a->animating = false;
    app_sync_key(a);
    app_status(a, "Randomized rotor order, windows and ten plugboard pairs.");
}
void app_random_plugs(GtkButton *button, gpointer data) {
    (void)button;
    App *a = data;
    plugboard_random(a->key.plug, 10, &a->random_seed);
    a->plug_selected = -1;
    app_sync_key(a);
    app_status(a, "Connected ten random plugboard pairs.");
}
void app_clear_plugs(GtkButton *button, gpointer data) {
    (void)button;
    App *a = data;
    plugboard_clear(a->key.plug);
    a->plug_selected = -1;
    app_sync_key(a);
}
void app_apply_plugs(GtkButton *button, gpointer data) {
    (void)button;
    App *a = data;
    uint8_t p[26];
    if (!plugboard_parse(p, gtk_editable_get_text(GTK_EDITABLE(a->plug_text)))) {
        app_status(a, "Invalid pairs. Each letter may occur only once, with at most ten pairs.");
        return;
    }
    memcpy(a->key.plug, p, 26);
    a->trace_count = 0;
    a->animating = false;
    app_sync_key(a);
    app_status(a, "Plugboard applied.");
}
static void set_challenge(App *a, const EnigmaKey *key, const char *plain) {
    if (a->encrypt_timer) {
        g_source_remove(a->encrypt_timer);
        a->encrypt_timer = 0;
    }
    a->challenge.present = true;
    a->challenge.visible = false;
    a->challenge.key = *key;
    g_strlcpy(a->challenge.plaintext, plain, sizeof a->challenge.plaintext);
    char cipher[LAB_TEXT_MAX];
    enigma_text(key, plain, cipher, sizeof cipher);
    gtk_check_button_set_active(GTK_CHECK_BUTTON(a->auto_encrypt), FALSE);
    a->loading = true;
    app_set_text(a->plain, "");
    app_set_text(a->cipher, cipher);
    app_set_text(a->decrypted, "");
    a->loading = false;
    /* The editable machine is scrubbed. Only public training assumptions are retained. */
    enigma_key_default(&a->key);
    memcpy(a->key.ring, key->ring, 3);
    a->key.reflector = key->reflector;
    a->trace_count = 0;
    a->animating = false;
    app_sync_key(a);
    update_secret(a);
    app_menu_changed(NULL, a);
}
bool app_capture_intercept(App *a) {
    if (a->snapshot.running || a->benchmark_running) {
        app_status(a, "Stop the search or benchmark before creating an intercept.");
        return false;
    }
    if (!app_read_key(a))
        return false;
    char *input = app_text(a->plain), plain[LAB_TEXT_MAX], cipher[LAB_TEXT_MAX];
    if (strlen(input) >= LAB_TEXT_MAX) {
        g_free(input);
        app_status(a, "Plaintext exceeds the message limit.");
        return false;
    }
    enigma_normalize(input, plain, sizeof plain);
    g_free(input);
    enigma_text(&a->key, plain, cipher, sizeof cipher);
    char *shown = app_text(a->cipher);
    bool matches = *plain && strcmp(shown, cipher) == 0;
    g_free(shown);
    if (!matches) {
        app_status(a, "Encrypt your message with the current settings before hiding it.");
        return false;
    }
    EnigmaKey key = a->key;
    set_challenge(a, &key, plain);
    app_status(a, "Your ciphertext is now an intercept. The same key and plaintext are hidden; "
                  "rings and reflector remain public for training.");
    return true;
}
void app_keep_intercept(GtkButton *button, gpointer data) {
    (void)button;
    app_capture_intercept(data);
}
void app_intercept(GtkButton *button, gpointer data) {
    (void)button;
    App *a = data;
    char *input = app_text(a->plain), plain[LAB_TEXT_MAX];
    if (strlen(input) >= LAB_TEXT_MAX) {
        g_free(input);
        app_status(a, "Plaintext exceeds the message limit.");
        return;
    }
    enigma_normalize(input, plain, sizeof plain);
    g_free(input);
    if (!*plain)
        g_strlcpy(plain, example_plain, sizeof plain);
    EnigmaKey k;
    enigma_random_key(&k, gtk_check_button_get_active(GTK_CHECK_BUTTON(a->random_rings)),
                      &a->random_seed);
    k.reflector = (uint8_t)gtk_drop_down_get_selected(GTK_DROP_DOWN(a->reflector));
    set_challenge(a, &k, plain);
    app_status(a, "Random intercept created. Plaintext and secret key are hidden. Rings and "
                  "reflector remain public in training mode.");
}
void app_example(GtkButton *button, gpointer data) {
    (void)button;
    App *a = data;
    EnigmaKey k;
    enigma_key_default(&k);
    k.start[2] = 7;
    plugboard_parse(k.plug, "AV BS CG DL FU HZ IN KM OW RX");
    set_challenge(a, &k, example_plain);
    gtk_editable_set_text(GTK_EDITABLE(a->crib), example_crib);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(a->alignment), 0);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(a->mode), 0);
    gtk_stack_set_visible_child_name(GTK_STACK(a->stack), "bombe");
    app_status(a, "Example ready. Its long pangram crib determines all plugboard letters. Press "
                  "START BOMBE.");
}
void app_reveal(GtkButton *button, gpointer data) {
    (void)button;
    App *a = data;
    a->challenge.visible = true;
    update_secret(a);
    if (a->challenge.present) {
        a->loading = true;
        app_set_text(a->plain, a->challenge.plaintext);
        a->loading = false;
    }
}
void app_hide(GtkButton *button, gpointer data) {
    (void)button;
    App *a = data;
    a->challenge.visible = false;
    update_secret(a);
    if (a->challenge.present) {
        a->loading = true;
        app_set_text(a->plain, "");
        a->loading = false;
    }
}
void app_copy(GtkButton *button, gpointer data) {
    (void)button;
    App *a = data;
    char *s = app_text(a->cipher);
    gdk_clipboard_set_text(gtk_widget_get_clipboard(a->window), s);
    g_free(s);
    app_status(a, "Ciphertext copied.");
}
void app_menu_changed(GtkWidget *widget, gpointer data) {
    App *a = data;
    if (a->loading || !a->crib)
        return;
    char *raw = app_text(a->cipher), cipher[LAB_TEXT_MAX], crib[LAB_CRIB_MAX + 1];
    enigma_normalize(raw, cipher, sizeof cipher);
    g_free(raw);
    if (a->challenge.present &&
        (gpointer)widget == (gpointer)gtk_text_view_get_buffer(GTK_TEXT_VIEW(a->cipher))) {
        char expected[LAB_TEXT_MAX];
        enigma_text(&a->challenge.key, a->challenge.plaintext, expected, sizeof expected);
        if (strcmp(expected, cipher)) {
            a->challenge.present = false;
            update_secret(a);
        }
    }
    enigma_normalize(gtk_editable_get_text(GTK_EDITABLE(a->crib)), crib, sizeof crib);
    unsigned off = (unsigned)gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(a->alignment));
    a->menu_valid = menu_build(&a->menu, cipher, crib, off);
    char label[240];
    if (a->menu_valid)
        g_snprintf(label, sizeof label,
                   "VALID ALIGNMENT  /  offset %u  /  %d edges  /  %d independent cycles  /  seed "
                   "letter %c",
                   off, a->menu.count, a->menu.cycles, 'A' + a->menu.root);
    else
        g_snprintf(label, sizeof label,
                   "REJECTED  /  Crib is empty, extends beyond the intercept, or shares a letter "
                   "with ciphertext at this offset.");
    gtk_label_set_text(GTK_LABEL(a->menu_label), label);
    gtk_widget_queue_draw(a->menu_canvas);
}
void app_auto_alignment(GtkButton *button, gpointer data) {
    (void)button;
    App *a = data;
    char *raw = app_text(a->cipher), cipher[LAB_TEXT_MAX], crib[LAB_CRIB_MAX + 1];
    enigma_normalize(raw, cipher, sizeof cipher);
    enigma_normalize(gtk_editable_get_text(GTK_EDITABLE(a->crib)), crib, sizeof crib);
    g_free(raw);
    int off = menu_best_alignment(cipher, crib);
    if (off < 0)
        app_status(a, "No possible crib alignment. Try a different crib.");
    else {
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(a->alignment), off);
        app_menu_changed(NULL, a);
    }
}
void app_previous_alignment(GtkButton *button, gpointer data) {
    (void)button;
    App *a = data;
    gtk_spin_button_spin(GTK_SPIN_BUTTON(a->alignment), GTK_SPIN_STEP_BACKWARD, 1);
}
void app_next_alignment(GtkButton *button, gpointer data) {
    (void)button;
    App *a = data;
    gtk_spin_button_spin(GTK_SPIN_BUTTON(a->alignment), GTK_SPIN_STEP_FORWARD, 1);
}
static void clear_results(App *a) {
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(a->results)))
        gtk_list_box_remove(GTK_LIST_BOX(a->results), child);
    a->result_count = a->solved_count = 0;
}
void app_start(GtkButton *button, gpointer data) {
    (void)button;
    App *a = data;
    if (a->snapshot.running || a->benchmark_running) {
        app_status(a, "Stop the current search or benchmark before starting another.");
        return;
    }
    if (!app_read_key(a))
        return;
    char *cipher = app_text(a->cipher);
    const char *crib = gtk_editable_get_text(GTK_EDITABLE(a->crib));
    SearchSpec spec;
    bool valid =
        search_spec_init(&spec, cipher, crib,
                         (unsigned)gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(a->alignment)),
                         a->key.ring, a->key.reflector);
    g_free(cipher);
    if (!valid || strlen(spec.crib) < 8) {
        app_status(a, "Use a valid crib alignment with at least eight letters. More cycles reduce "
                      "ambiguity.");
        return;
    }
    spec.advanced = gtk_drop_down_get_selected(GTK_DROP_DOWN(a->mode)) == 2;
    spec.max_stops_per_state =
        (unsigned)gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(a->stop_limit));
    unsigned threads = (unsigned)gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(a->threads));
    if (!a->pool || threads != a->pool_threads) {
        search_pool_free(a->pool);
        a->pool = NULL;
        a->pool_threads = 0;
        g_free(a->workers);
        g_free(a->previous_workers);
        g_free(a->worker_rates);
        g_free(a->worker_flashes);
        a->workers = g_new0(WorkerSnapshot, threads);
        a->previous_workers = g_new0(WorkerSnapshot, threads);
        a->worker_rates = g_new0(double, threads);
        a->worker_flashes = g_new0(double, threads);
        a->pool = search_pool_new(threads);
        if (!a->pool) {
            app_status(a, "Cannot create %u worker threads. Try a smaller count.", threads);
            return;
        }
        a->pool_threads = threads;
    }
    clear_results(a);
    memset(a->workers, 0, sizeof(WorkerSnapshot) * threads);
    memset(a->previous_workers, 0, sizeof(WorkerSnapshot) * threads);
    memset(a->worker_flashes, 0, sizeof(double) * threads);
    a->active_spec = spec;
    a->history_count = a->history_head = 0;
    a->history_max = 0;
    a->last_tested = 0;
    a->last_sample = lab_now();
    if (!search_pool_start(a->pool, &spec)) {
        app_status(a, "Previous search is still shutting down.");
        return;
    }
    a->started_search = true;
    search_pool_snapshot(a->pool, &a->snapshot, a->workers);
    gtk_stack_set_visible_child_name(GTK_STACK(a->stack), "bombe");
    app_status(
        a,
        "Searching %" G_GUINT64_FORMAT
        " rotor states with %u virtual Bombe workers. No secret key is passed to the solver.",
        bombe_total(&spec), threads);
}
void app_pause(GtkButton *button, gpointer data) {
    (void)button;
    App *a = data;
    search_pool_pause(a->pool, true);
    app_status(a, "Pause requested. Workers finish their current 64-state work unit.");
}
void app_resume(GtkButton *button, gpointer data) {
    (void)button;
    App *a = data;
    search_pool_pause(a->pool, false);
    app_status(a, "Search resumed.");
}
void app_stop(GtkButton *button, gpointer data) {
    (void)button;
    App *a = data;
    search_pool_stop(a->pool);
    atomic_store(&a->benchmark.cancel, true);
    app_status(a, "Stop requested.");
}
void app_benchmark(GtkButton *button, gpointer data) {
    (void)button;
    App *a = data;
    if (a->snapshot.running) {
        app_status(a, "Stop the search before benchmarking to avoid competing CPU workloads.");
        return;
    }
    if (benchmark_start(&a->benchmark, lab_cpu_count())) {
        a->benchmark_running = true;
        a->benchmark_count = 0;
        gtk_stack_set_visible_child_name(GTK_STACK(a->stack), "benchmark");
        app_status(
            a, "Benchmark running in a background coordinator with persistent pthread workers.");
    } else
        app_status(a, "Benchmark is already running or could not start.");
}
static void candidate_description(const Candidate *c, char *out, size_t capacity) {
    char pairs[80], unknown[27];
    plugboard_format(c->key.plug, pairs);
    int n = 0;
    for (int i = 0; i < 26; i++)
        if (c->deductions[i] < 0)
            unknown[n++] = (char)('A' + i);
    unknown[n] = 0;
    g_snprintf(out, capacity,
               "%s %s %s    %c%c%c    rings %c%c%c    UKW %c    worker %u    +%.2fs\n%s   |   "
               "unresolved: %s   |   crib verified   |   score %.3f\n%.110s",
               rotor_names[c->key.order[0]], rotor_names[c->key.order[1]],
               rotor_names[c->key.order[2]], 'A' + c->key.start[0], 'A' + c->key.start[1],
               'A' + c->key.start[2], 'A' + c->key.ring[0], 'A' + c->key.ring[1],
               'A' + c->key.ring[2], 'B' + c->key.reflector, c->worker + 1, c->elapsed,
               *pairs ? pairs : "No paired letters", *unknown ? unknown : "none", c->score,
               c->plaintext);
}
static void add_candidate(App *a, const Candidate *c) {
    if (c->worker < a->pool_threads)
        a->worker_flashes[c->worker] = lab_now();
    /* Validation happens after independent menu solving and full-message decryption. */
    char challenge_cipher[LAB_TEXT_MAX] = "";
    if (a->challenge.present)
        enigma_text(&a->challenge.key, a->challenge.plaintext, challenge_cipher,
                    sizeof challenge_cipher);
    if (a->challenge.present && !strcmp(challenge_cipher, a->active_spec.cipher) &&
        !strcmp(c->plaintext, a->challenge.plaintext)) {
        a->solved_count++;
        app_set_text(a->decrypted, c->plaintext);
        app_status(
            a,
            "Challenge plaintext recovered independently. Candidate %s %s %s / %c%c%c, worker %u.",
            rotor_names[c->key.order[0]], rotor_names[c->key.order[1]],
            rotor_names[c->key.order[2]], 'A' + c->key.start[0], 'A' + c->key.start[1],
            'A' + c->key.start[2], c->worker + 1);
    }
    if (a->result_count >= 2000) {
        GtkListBoxRow *last = gtk_list_box_get_row_at_index(GTK_LIST_BOX(a->results), 1999);
        Candidate *lowest = g_object_get_data(G_OBJECT(last), "candidate");
        if (lowest && lowest->score >= c->score)
            return;
        gtk_list_box_remove(GTK_LIST_BOX(a->results), GTK_WIDGET(last));
        a->result_count--;
    }
    char text[640];
    candidate_description(c, text, sizeof text);
    GtkWidget *row = gtk_list_box_row_new(), *label = gtk_label_new(text);
    gtk_label_set_xalign(GTK_LABEL(label), 0);
    gtk_label_set_wrap(GTK_LABEL(label), TRUE);
    gtk_label_set_wrap_mode(GTK_LABEL(label), PANGO_WRAP_WORD_CHAR);
    gtk_label_set_yalign(GTK_LABEL(label), 0);
    PangoAttrList *attributes = pango_attr_list_new();
    pango_attr_list_insert(attributes, pango_attr_line_height_new(1.25));
    gtk_label_set_attributes(GTK_LABEL(label), attributes);
    pango_attr_list_unref(attributes);
    gtk_widget_add_css_class(label, "stats");
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
    Candidate *copy = g_new(Candidate, 1);
    *copy = *c;
    g_object_set_data_full(G_OBJECT(row), "candidate", copy, g_free);
    gtk_list_box_append(GTK_LIST_BOX(a->results), row);
    a->result_count++;
}
void app_candidate_activated(GtkListBox *box, GtkListBoxRow *row, gpointer data) {
    (void)box;
    App *a = data;
    Candidate *c = g_object_get_data(G_OBJECT(row), "candidate");
    if (!c)
        return;
    a->key = c->key;
    a->trace_count = 0;
    a->animating = false;
    app_sync_key(a);
    app_set_text(a->decrypted, c->plaintext);
    gtk_stack_set_visible_child_name(GTK_STACK(a->stack), "message");
    app_status(a,
               "Candidate loaded into Enigma. %u unresolved letters use identity in the preview; a "
               "crib-consistent stop may still be wrong.",
               c->unresolved);
}
static gboolean refresh(gpointer data) {
    App *a = data;
    double now = lab_now(), dt = now - a->last_sample;
    bool was_running = a->snapshot.running;
    if (a->pool) {
        search_pool_snapshot(a->pool, &a->snapshot, a->workers);
        Candidate candidate;
        unsigned drained = 0;
        while (drained++ < 32 && search_pool_pop(a->pool, &candidate))
            add_candidate(a, &candidate);
        if (dt >= .20) {
            double rate = dt > 0 ? (double)(a->snapshot.tested - a->last_tested) / dt : 0;
            if (was_running || a->snapshot.running) {
                a->history[a->history_head] = rate;
                a->history_head = (a->history_head + 1) % 240;
                a->history_count = MIN(a->history_count + 1, 240);
                a->history_max = fmax(a->history_max, rate);
            }
            for (unsigned i = 0; i < a->pool_threads; i++) {
                a->worker_rates[i] =
                    (double)(a->workers[i].tested - a->previous_workers[i].tested) / dt;
                a->previous_workers[i] = a->workers[i];
            }
            a->last_tested = a->snapshot.tested;
            a->last_sample = now;
            gtk_widget_queue_draw(a->graph_canvas);
            SearchSnapshot *s = &a->snapshot;
            double average = s->elapsed > 0 ? (double)s->tested / s->elapsed : 0;
            char stats[1000];
            g_snprintf(stats, sizeof stats,
                       "States tested  %" G_GUINT64_FORMAT " / %" G_GUINT64_FORMAT
                       "     Remaining  %" G_GUINT64_FORMAT
                       "     Elapsed  %.1fs\nActual  %.0f states/s     Average  %.0f /s     "
                       "Contradictions  %" G_GUINT64_FORMAT
                       "\nBombe stops / verified  %" G_GUINT64_FORMAT
                       "     Challenge matches  %u     Workers  %u     Rotor orders completed  "
                       "%u\nStop-capped states  %" G_GUINT64_FORMAT
                       "     Queue overflow  %" G_GUINT64_FORMAT "     Displayed  %u / 2000",
                       s->tested, s->total, s->total - s->tested, s->elapsed, rate, average,
                       s->rejected, s->stops, a->solved_count, s->threads, s->orders_completed,
                       s->truncated, s->dropped, a->result_count);
            gtk_label_set_text(GTK_LABEL(a->stats), stats);
            double fraction = s->total ? (double)s->tested / (double)s->total : 0;
            gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(a->progress), fraction);
            char p[80];
            g_snprintf(p, sizeof p, "%.3f%%  %s", fraction * 100,
                       s->running              ? (s->paused ? "PAUSED" : "SEARCHING")
                       : s->tested == s->total ? "COMPLETE"
                                               : "STOPPED");
            gtk_progress_bar_set_text(GTK_PROGRESS_BAR(a->progress), p);
        }
        int columns = MAX(1, gtk_widget_get_width(a->bombe_canvas) / 290);
        int h = (int)((a->pool_threads + (unsigned)columns - 1) / (unsigned)columns) * 190 + 10;
        if (gtk_drawing_area_get_content_height(GTK_DRAWING_AREA(a->bombe_canvas)) != h)
            gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(a->bombe_canvas), h);
        gtk_widget_queue_draw(a->bombe_canvas);
        if (was_running && !a->snapshot.running)
            app_status(a,
                       "Search %s. %" G_GUINT64_FORMAT " states, %" G_GUINT64_FORMAT
                       " verified stops, %u challenge plaintext matches.",
                       a->snapshot.tested == a->snapshot.total ? "completed" : "stopped",
                       a->snapshot.tested, a->snapshot.stops, a->solved_count);
    }
    benchmark_snapshot(&a->benchmark, a->benchmark_rows, &a->benchmark_count,
                       &a->benchmark_running);
    if (a->benchmark_count || a->benchmark_running) {
        GString *s = g_string_new("Threads       states/sec       speedup       efficiency\n");
        for (unsigned i = 0; i < a->benchmark_count; i++) {
            BenchmarkRow *r = &a->benchmark_rows[i];
            g_string_append_printf(s, "%7u  %14.0f  %10.2fx  %13.1f%%\n", r->threads, r->rate,
                                   r->speedup, r->efficiency * 100);
        }
        g_string_append(s, a->benchmark_running
                               ? "\nMeasuring..."
                               : "\nBenchmark finished. Values measure this workload on this CPU.");
        gtk_label_set_text(GTK_LABEL(a->benchmark_text), s->str);
        g_string_free(s, TRUE);
        gtk_widget_queue_draw(a->benchmark_canvas);
    }
    gtk_widget_set_sensitive(a->start_button, !a->snapshot.running && !a->benchmark_running);
    gtk_widget_set_sensitive(a->pause_button, a->snapshot.running && !a->snapshot.paused);
    gtk_widget_set_sensitive(a->resume_button, a->snapshot.running && a->snapshot.paused);
    gtk_widget_set_sensitive(a->stop_button, a->snapshot.running || a->benchmark_running);
    gtk_widget_set_sensitive(a->threads, !a->snapshot.running && !a->benchmark_running);
    gtk_widget_set_sensitive(a->mode, !a->snapshot.running);
    gtk_widget_set_sensitive(a->stop_limit, !a->snapshot.running);
    gtk_widget_set_sensitive(a->benchmark_button, !a->snapshot.running && !a->benchmark_running);
    tutorial_refresh(a);
    return G_SOURCE_CONTINUE;
}
static void write_key(GKeyFile *file, const char *group, const EnigmaKey *k) {
    gint order[3];
    for (int i = 0; i < 3; i++)
        order[i] = k->order[i] + 1;
    g_key_file_set_integer_list(file, group, "rotors", order, 3);
    char s[4], pairs[80];
    format3(k->ring, s);
    g_key_file_set_string(file, group, "rings", s);
    format3(k->start, s);
    g_key_file_set_string(file, group, "positions", s);
    plugboard_format(k->plug, pairs);
    g_key_file_set_string(file, group, "plugboard", pairs);
    g_key_file_set_string(file, group, "reflector", k->reflector ? "C" : "B");
}
static bool read_key(GKeyFile *file, const char *group, EnigmaKey *k) {
    enigma_key_default(k);
    gsize n = 0;
    gint *order = g_key_file_get_integer_list(file, group, "rotors", &n, NULL);
    bool ok = order && n == 3;
    if (ok)
        for (int i = 0; i < 3; i++) {
            if (order[i] < 1 || order[i] > 5)
                ok = false;
            else
                k->order[i] = (uint8_t)(order[i] - 1);
        }
    g_free(order);
    char *rings = g_key_file_get_string(file, group, "rings", NULL),
         *positions = g_key_file_get_string(file, group, "positions", NULL),
         *plugs = g_key_file_get_string(file, group, "plugboard", NULL),
         *reflector = g_key_file_get_string(file, group, "reflector", NULL);
    ok = ok && rings && positions && plugs && reflector && letters3(rings, k->ring) &&
         letters3(positions, k->start) && plugboard_parse(k->plug, plugs) &&
         (!strcmp(reflector, "B") || !strcmp(reflector, "C"));
    if (ok)
        k->reflector = (uint8_t)(*reflector == 'C');
    g_free(rings);
    g_free(positions);
    g_free(plugs);
    g_free(reflector);
    return ok && enigma_key_valid(k);
}
void app_save(GtkButton *button, gpointer data) {
    (void)button;
    App *a = data;
    if (!app_read_key(a))
        return;
    GKeyFile *file = g_key_file_new();
    write_key(file, "Machine", &a->key);
    char *plain = app_text(a->plain), *cipher = app_text(a->cipher);
    if (strlen(plain) >= LAB_TEXT_MAX || strlen(cipher) >= LAB_TEXT_MAX) {
        app_status(a, "Scenario messages must each be shorter than %d bytes.", LAB_TEXT_MAX);
        g_free(plain);
        g_free(cipher);
        g_key_file_unref(file);
        return;
    }
    g_key_file_set_string(file, "Message", "plaintext", plain);
    g_key_file_set_string(file, "Message", "ciphertext", cipher);
    g_key_file_set_string(file, "Message", "crib", gtk_editable_get_text(GTK_EDITABLE(a->crib)));
    g_key_file_set_integer(file, "Message", "offset",
                           gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(a->alignment)));
    g_key_file_set_integer(file, "Message", "mode",
                           (int)gtk_drop_down_get_selected(GTK_DROP_DOWN(a->mode)));
    g_key_file_set_integer(file, "Message", "stop_limit",
                           gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(a->stop_limit)));
    g_key_file_set_boolean(file, "Message", "random_rings",
                           gtk_check_button_get_active(GTK_CHECK_BUTTON(a->random_rings)));
    g_free(plain);
    g_free(cipher);
    if (a->challenge.present) {
        write_key(file, "Challenge", &a->challenge.key);
        g_key_file_set_string(file, "Challenge", "plaintext", a->challenge.plaintext);
    }
    GError *error = NULL;
    const char *path = gtk_editable_get_text(GTK_EDITABLE(a->scenario_path));
    if (!g_key_file_save_to_file(file, path, &error)) {
        app_status(a, "Save failed: %s", error->message);
        g_clear_error(&error);
    } else
        app_status(a, "Saved scenario to %s. Challenge secrets are included in the file.", path);
    g_key_file_unref(file);
}
static bool load_path(App *a, const char *path) {
    GKeyFile *file = g_key_file_new();
    GError *error = NULL;
    if (!scenario_load_file(file, path, &error)) {
        app_status(a, "Load failed: %s", error->message);
        g_clear_error(&error);
        g_key_file_unref(file);
        return false;
    }
    EnigmaKey key;
    Challenge challenge = {0};
    bool ok = read_key(file, "Machine", &key);
    char *plain = g_key_file_get_string(file, "Message", "plaintext", NULL),
         *cipher = g_key_file_get_string(file, "Message", "ciphertext", NULL),
         *crib = g_key_file_get_string(file, "Message", "crib", NULL);
    int off = g_key_file_get_integer(file, "Message", "offset", NULL),
        mode = g_key_file_get_integer(file, "Message", "mode", NULL);
    int stop_limit = g_key_file_has_key(file, "Message", "stop_limit", NULL)
                         ? g_key_file_get_integer(file, "Message", "stop_limit", NULL)
                         : 64;
    ok = ok && stop_limit >= 0 && stop_limit <= 4096;
    ok = ok && plain && cipher && crib && strlen(plain) < LAB_TEXT_MAX &&
         strlen(cipher) < LAB_TEXT_MAX && strlen(crib) <= LAB_CRIB_MAX && off >= 0 &&
         off < LAB_TEXT_MAX && mode >= 0 && mode < 3;
    if (g_key_file_has_group(file, "Challenge")) {
        char *secret = g_key_file_get_string(file, "Challenge", "plaintext", NULL);
        challenge.present = true;
        ok = ok && read_key(file, "Challenge", &challenge.key) && secret &&
             strlen(secret) < LAB_TEXT_MAX;
        if (ok) {
            enigma_normalize(secret, challenge.plaintext, sizeof challenge.plaintext);
            char encrypted[LAB_TEXT_MAX], normalized[LAB_TEXT_MAX];
            enigma_text(&challenge.key, challenge.plaintext, encrypted, sizeof encrypted);
            enigma_normalize(cipher, normalized, sizeof normalized);
            ok = !strcmp(encrypted, normalized);
        }
        g_free(secret);
    }
    if (ok) {
        if (a->encrypt_timer) {
            g_source_remove(a->encrypt_timer);
            a->encrypt_timer = 0;
        }
        a->loading = true;
        gtk_check_button_set_active(GTK_CHECK_BUTTON(a->auto_encrypt), FALSE);
        a->key = key;
        a->challenge = challenge;
        a->trace_count = 0;
        a->animating = false;
        app_sync_key(a);
        app_set_text(a->plain, challenge.present ? "" : plain);
        app_set_text(a->cipher, cipher);
        app_set_text(a->decrypted, "");
        gtk_editable_set_text(GTK_EDITABLE(a->crib), crib);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(a->alignment), off);
        gtk_drop_down_set_selected(GTK_DROP_DOWN(a->mode), (guint)mode);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(a->stop_limit), stop_limit);
        gtk_check_button_set_active(GTK_CHECK_BUTTON(a->random_rings),
                                    g_key_file_get_boolean(file, "Message", "random_rings", NULL));
        a->loading = false;
        update_secret(a);
        app_menu_changed(NULL, a);
        app_status(a, "Loaded %s.", path);
    } else
        app_status(a, "Invalid scenario. Check rotor uniqueness, rings, plugboard, message lengths "
                      "and challenge consistency.");
    g_free(plain);
    g_free(cipher);
    g_free(crib);
    g_key_file_unref(file);
    return ok;
}
void app_load(GtkButton *button, gpointer data) {
    (void)button;
    App *a = data;
    if (a->snapshot.running) {
        app_status(a, "Stop the search before loading a scenario.");
        return;
    }
    load_path(a, gtk_editable_get_text(GTK_EDITABLE(a->scenario_path)));
}
static gboolean smoke_end(gpointer data) {
    App *a = data;
    if (a->result_count) {
        GtkListBoxRow *row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(a->results), 0);
        app_candidate_activated(GTK_LIST_BOX(a->results), row, a);
        char *preview = app_text(a->decrypted);
        g_assert_cmpstr(preview, ==, example_plain);
        g_free(preview);
    }
    g_print("GUI smoke: tested=%" G_GUINT64_FORMAT " stops=%" G_GUINT64_FORMAT " recovered=%u\n",
            a->snapshot.tested, a->snapshot.stops, a->solved_count);
    if (a->benchmark_requested) {
        g_assert_cmpint(a->benchmark_running, ==, FALSE);
        g_assert_cmpuint(a->benchmark_count, >, 0);
        for (unsigned i = 0; i < a->benchmark_count; i++)
            g_print("GUI benchmark: %u workers, %.0f states/s, %.2fx, %.1f%% efficiency\n",
                    a->benchmark_rows[i].threads, a->benchmark_rows[i].rate,
                    a->benchmark_rows[i].speedup, a->benchmark_rows[i].efficiency * 100);
    }
    g_application_quit(G_APPLICATION(a->application));
    return G_SOURCE_REMOVE;
}
static void view_action(GSimpleAction *action, GVariant *parameter, gpointer data) {
    (void)action;
    App *a = data;
    const char *name = g_variant_get_string(parameter, NULL);
    gtk_stack_set_visible_child_name(GTK_STACK(a->stack), name);
}
/* Exercise real UI callbacks without adding a second implementation of the workflow. */
static void smoke_prepare(App *a) {
    gtk_check_button_set_active(GTK_CHECK_BUTTON(a->auto_encrypt), FALSE);
    enigma_key_default(&a->key);
    app_sync_key(a);
    app_set_text(a->plain, "");
    for (int i = 0; i < 5; i++)
        g_assert_cmpint(app_keyboard_press(NULL, GDK_KEY_A, 0, 0, a), ==, TRUE);
    char *typed = app_text(a->cipher);
    g_assert_cmpstr(typed, ==, "BDZGO");
    g_assert_cmpuint(a->trace_index, ==, 4);
    g_free(typed);
    app_set_text(a->plain, "AAAAA");
    app_encrypt(NULL, a);
    char *cipher = app_text(a->cipher);
    g_assert_cmpstr(cipher, ==, "BDZGO");
    g_assert_cmpuint(a->trace_count, ==, 5);
    app_set_text(a->plain, cipher);
    g_free(cipher);
    app_reset(NULL, a);
    app_encrypt(NULL, a);
    cipher = app_text(a->cipher);
    g_assert_cmpstr(cipher, ==, "AAAAA");
    g_free(cipher);
    gtk_editable_set_text(GTK_EDITABLE(a->plug_text), "AG BL CZ");
    app_apply_plugs(NULL, a);
    g_assert_cmpuint(a->key.plug[0], ==, 6);
    gtk_editable_set_text(GTK_EDITABLE(a->plug_text), "AG AB");
    app_apply_plugs(NULL, a);
    g_assert_cmpuint(a->key.plug[1], ==, 11);
    app_clear_plugs(NULL, a);
    g_assert_cmpuint(a->key.plug[0], ==, 0);
    app_random_plugs(NULL, a);
    unsigned pairs = 0;
    for (int i = 0; i < 26; i++)
        pairs += a->key.plug[i] > i;
    g_assert_cmpuint(pairs, ==, 10);
    app_random_key(NULL, a);
    g_assert_cmpint((enigma_key_valid(&a->key)), ==, TRUE);
    app_set_text(a->plain, example_plain);
    app_intercept(NULL, a);
    g_assert_cmpint((a->challenge.present), ==, TRUE);
    app_reveal(NULL, a);
    char *plain = app_text(a->plain);
    g_assert_cmpstr(plain, ==, example_plain);
    g_free(plain);
    app_hide(NULL, a);
    plain = app_text(a->plain);
    g_assert_cmpstr(plain, ==, "");
    g_free(plain);
    GError *error = NULL;
    char *dir = g_dir_make_tmp("enigma-lab-test-XXXXXX", &error);
    g_assert_no_error(error);
    char *path = g_build_filename(dir, "roundtrip.ini", NULL);
    gtk_editable_set_text(GTK_EDITABLE(a->scenario_path), path);
    app_save(NULL, a);
    Challenge saved = a->challenge;
    app_example(NULL, a);
    g_assert_cmpint((load_path(a, path)), ==, TRUE);
    g_assert_cmpmem(&saved.key, sizeof saved.key, &a->challenge.key, sizeof a->challenge.key);
    g_assert_cmpstr(saved.plaintext, ==, a->challenge.plaintext);
    g_assert_cmpint((g_file_set_contents(path, "[Machine]\nrotors=1;1;3;\n", -1, &error)), ==,
                    TRUE);
    g_assert_no_error(error);
    g_assert_cmpint((load_path(a, path)), ==, FALSE);
    g_assert_cmpmem(&saved.key, sizeof saved.key, &a->challenge.key, sizeof a->challenge.key);
    g_remove(path);
    g_rmdir(dir);
    g_free(path);
    g_free(dir);
    gtk_editable_set_text(GTK_EDITABLE(a->scenario_path), "scenario.ini");
    app_example(NULL, a);
    app_auto_alignment(NULL, a);
    g_assert_cmpint((a->menu_valid), ==, TRUE);
    g_assert_cmpint(gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(a->alignment)), ==, 0);
    g_print("GUI callbacks: encryption, reciprocity, plugboard, randomization, secret hiding, "
            "scenario round-trip and invalid-file rejection passed\n");
}
void app_activate(GtkApplication *application, gpointer data) {
    App *a = data;
    if (a->window) {
        gtk_window_present(GTK_WINDOW(a->window));
        return;
    }
    a->application = application;
    a->plug_selected = -1;
    a->random_seed = (uint64_t)g_get_real_time();
    enigma_key_default(&a->key);
    benchmark_init(&a->benchmark);
    gui_build(a);
    GSimpleAction *view = g_simple_action_new("view", G_VARIANT_TYPE_STRING);
    g_signal_connect(view, "activate", G_CALLBACK(view_action), a);
    g_action_map_add_action(G_ACTION_MAP(application), G_ACTION(view));
    g_object_unref(view);
    const char *pages[] = {"enigma", "plugboard", "message", "menu", "bombe", "benchmark"};
    for (unsigned i = 0; i < G_N_ELEMENTS(pages); i++) {
        char *name = g_strdup_printf("app.view('%s')", pages[i]);
        char *shortcut = g_strdup_printf("<Alt>%u", i + 1);
        const char *accels[] = {shortcut, NULL};
        gtk_application_set_accels_for_action(application, name, accels);
        g_free(name);
        g_free(shortcut);
    }
    app_sync_key(a);
    a->last_sample = lab_now();
    gboolean animations = TRUE;
    g_object_get(gtk_settings_get_default(), "gtk-enable-animations", &animations, NULL);
    a->reduced_motion = !animations;
    a->loading = true;
    app_set_text(a->plain, "WETTERBERICHT");
    a->loading = false;
    gtk_window_present(GTK_WINDOW(a->window));
    a->timer = g_timeout_add(100, refresh, a);
    if (a->initial_file) {
        gtk_editable_set_text(GTK_EDITABLE(a->scenario_path), a->initial_file);
        load_path(a, a->initial_file);
    }
    if (a->smoke_ms) {
        smoke_prepare(a);
        if (a->benchmark_requested)
            app_benchmark(NULL, a);
        else
            app_start(NULL, a);
        g_timeout_add(a->smoke_ms, smoke_end, a);
    } else if (a->benchmark_requested)
        app_benchmark(NULL, a);
    else {
        char *dir = g_build_filename(g_get_user_config_dir(), "enigma-bombe-lab", NULL),
             *path = g_build_filename(dir, "tutorial-seen", NULL);
        if (a->tutorial_requested || !g_file_test(path, G_FILE_TEST_EXISTS)) {
            app_tutorial(NULL, a);
            if (g_mkdir_with_parents(dir, 0700) == 0)
                g_file_set_contents(path, "1\n", -1, NULL);
        }
        g_free(path);
        g_free(dir);
    }
}
void app_destroy(App *a) {
    if (a->timer)
        g_source_remove(a->timer);
    if (a->encrypt_timer)
        g_source_remove(a->encrypt_timer);
    a->animating = false;
    search_pool_free(a->pool);
    if (a->window)
        benchmark_destroy(&a->benchmark);
    g_free(a->workers);
    g_free(a->previous_workers);
    g_free(a->worker_rates);
    g_free(a->worker_flashes);
    g_free(a->initial_file);
}
