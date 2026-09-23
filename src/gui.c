#include "gui.h"
#include "bombe_view.h"
#include "enigma_view.h"
#include "menu_view.h"
#include <math.h>
void gui_color(cairo_t *cr, double r, double g, double b) {
    cairo_set_source_rgb(cr, r, g, b);
}
void gui_draw_text(cairo_t *cr, double x, double y, double size, const char *text) {
    cairo_select_font_face(cr, "monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, size);
    cairo_move_to(cr, x, y);
    cairo_show_text(cr, text);
}
void gui_panel(cairo_t *cr, double x, double y, double w, double h) {
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - 10, y + 10, 10, -G_PI / 2, 0);
    cairo_arc(cr, x + w - 10, y + h - 10, 10, 0, G_PI / 2);
    cairo_arc(cr, x + 10, y + h - 10, 10, G_PI / 2, G_PI);
    cairo_arc(cr, x + 10, y + 10, 10, G_PI, 1.5 * G_PI);
    cairo_close_path(cr);
    gui_color(cr, .10, .15, .16);
    cairo_fill_preserve(cr);
    gui_color(cr, .31, .36, .34);
    cairo_set_line_width(cr, 1);
    cairo_stroke(cr);
}
static GtkWidget *box(GtkOrientation o, int spacing) {
    if (o == GTK_ORIENTATION_VERTICAL)
        return gtk_box_new(o, spacing);
    GtkWidget *flow = gtk_flow_box_new();
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(flow), GTK_SELECTION_NONE);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(flow), 1);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(flow), 12);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(flow), (guint)spacing);
    gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(flow), (guint)spacing);
    return flow;
}
static GtkWidget *label(const char *text, const char *css) {
    GtkWidget *w = gtk_label_new(text);
    gtk_label_set_xalign(GTK_LABEL(w), 0);
    if (css)
        gtk_widget_add_css_class(w, css);
    gtk_label_set_wrap(GTK_LABEL(w), TRUE);
    gtk_label_set_wrap_mode(GTK_LABEL(w), PANGO_WRAP_WORD_CHAR);
    gtk_label_set_yalign(GTK_LABEL(w), 0);
    PangoAttrList *attributes = pango_attr_list_new();
    pango_attr_list_insert(attributes, pango_attr_line_height_new(1.25));
    gtk_label_set_attributes(GTK_LABEL(w), attributes);
    pango_attr_list_unref(attributes);
    return w;
}
static void add(GtkWidget *b, GtkWidget *w) {
    if (GTK_IS_FLOW_BOX(b))
        gtk_flow_box_insert(GTK_FLOW_BOX(b), w, -1);
    else
        gtk_box_append(GTK_BOX(b), w);
}
static GtkWidget *button(GtkWidget *b, const char *text, GCallback cb, App *a) {
    GtkWidget *w = gtk_button_new_with_label(text);
    g_signal_connect(w, "clicked", cb, a);
    add(b, w);
    return w;
}
static GtkWidget *entry(const char *value, int width) {
    GtkWidget *e = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(e), value);
    gtk_editable_set_width_chars(GTK_EDITABLE(e), width);
    return e;
}
static GtkWidget *field(GtkWidget *b, const char *title, GtkWidget *w) {
    GtkWidget *v = box(GTK_ORIENTATION_VERTICAL, 5);
    add(v, label(title, "subtitle"));
    add(v, w);
    add(b, v);
    return w;
}
static GtkWidget *page(App *a, const char *name, const char *title, const char *heading) {
    GtkWidget *container = box(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *header = label(heading, "title");
    gtk_widget_set_margin_start(header, 22);
    gtk_widget_set_margin_end(header, 22);
    gtk_widget_set_margin_top(header, 12);
    gtk_widget_set_margin_bottom(header, 12);
    add(container, header);
    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER,
                                   GTK_POLICY_AUTOMATIC);
    GtkWidget *v = box(GTK_ORIENTATION_VERTICAL, 14);
    gtk_widget_set_margin_start(v, 22);
    gtk_widget_set_margin_end(v, 22);
    gtk_widget_set_margin_top(v, 10);
    gtk_widget_set_margin_bottom(v, 18);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), v);
    gtk_scrolled_window_set_overlay_scrolling(GTK_SCROLLED_WINDOW(scroll), FALSE);
    gtk_widget_set_vexpand(scroll, TRUE);
    add(container, scroll);
    g_object_set_data(G_OBJECT(container), "page-scroll", scroll);
    gtk_stack_add_titled(GTK_STACK(a->stack), container, name, title);
    return v;
}
static void page_changed(GObject *stack, GParamSpec *property, gpointer data) {
    (void)property;
    App *a = data;
    GtkWidget *visible = gtk_stack_get_visible_child(GTK_STACK(stack));
    if (!visible) return;
    GtkWidget *scroll = g_object_get_data(G_OBJECT(visible), "page-scroll");
    GtkWidget *focus = gtk_window_get_focus(GTK_WINDOW(a->window));
    /* GtkStack restores the last focused descendant. That can scroll a newly opened
     * page straight to a text editor or the keyboard near its bottom. Start at the
     * top on navigation; normal Tab navigation still scrolls focused controls. */
    if (focus && gtk_widget_is_ancestor(focus, visible))
        gtk_window_set_focus(GTK_WINDOW(a->window), NULL);
    GtkAdjustment *adjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scroll));
    gtk_adjustment_set_value(adjustment, gtk_adjustment_get_lower(adjustment));
}
static GtkWidget *canvas(GtkWidget *b, int height, GtkDrawingAreaDrawFunc fn, App *a) {
    GtkWidget *w = gtk_drawing_area_new();
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(w), height);
    gtk_widget_set_hexpand(w, TRUE);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(w), fn, a, NULL);
    add(b, w);
    return w;
}
static GtkWidget *text_view(GtkWidget *b, const char *title, bool editable, int height) {
    add(b, label(title, "section"));
    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_widget_set_size_request(scroll, -1, height);
    gtk_widget_set_vexpand(scroll, TRUE);
    GtkWidget *v = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(v), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(v), editable);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(v), 14);
    gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(v), 14);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(v), 12);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(v), 12);
    gtk_text_view_set_pixels_above_lines(GTK_TEXT_VIEW(v), 3);
    gtk_text_view_set_pixels_below_lines(GTK_TEXT_VIEW(v), 3);
    gtk_text_view_set_pixels_inside_wrap(GTK_TEXT_VIEW(v), 2);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), v);
    add(b, scroll);
    return v;
}
static void thread_preset(GtkButton *b, gpointer data) {
    App *a = data;
    unsigned n = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(b), "threads"));
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(a->threads), (double)n);
}
static int candidate_sort(GtkListBoxRow *left, GtkListBoxRow *right, gpointer data) {
    (void)data;
    const Candidate *a = g_object_get_data(G_OBJECT(left), "candidate");
    const Candidate *b = g_object_get_data(G_OBJECT(right), "candidate");
    if (!a || !b)
        return 0;
    if (a->score != b->score)
        return a->score > b->score ? -1 : 1;
    return a->elapsed < b->elapsed ? -1 : a->elapsed > b->elapsed;
}
void gui_build(App *a) {
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_resource(css, "/org/enigmabombelab/style.css");
    gtk_style_context_add_provider_for_display(gdk_display_get_default(), GTK_STYLE_PROVIDER(css),
                                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);
    a->window = gtk_application_window_new(a->application);
    gtk_window_set_title(GTK_WINDOW(a->window), "Enigma Bombe Lab");
    gtk_window_set_default_size(GTK_WINDOW(a->window), 1400, 900);
    GtkWidget *header = gtk_header_bar_new();
    gtk_header_bar_set_title_widget(GTK_HEADER_BAR(header),
                                    gtk_label_new("ENIGMA BOMBE LAB  /  HISTORICAL CRYPTOGRAPHY"));
    gtk_window_set_titlebar(GTK_WINDOW(a->window), header);
    GtkWidget *help = gtk_button_new_with_label("Tutorial");
    g_signal_connect(help, "clicked", G_CALLBACK(app_tutorial), a);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), help);
    GtkWidget *root = box(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(a->window), root);
    a->stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(a->stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_widget_set_vexpand(a->stack, TRUE);
    g_signal_connect(a->stack, "notify::visible-child", G_CALLBACK(page_changed), a);
    GtkWidget *switcher = gtk_stack_switcher_new();
    gtk_stack_switcher_set_stack(GTK_STACK_SWITCHER(switcher), GTK_STACK(a->stack));
    gtk_widget_set_halign(switcher, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(switcher, 12);
    gtk_widget_set_margin_bottom(switcher, 8);
    add(root, switcher);
    tutorial_build(a, root);
    add(root, a->stack);
    a->status = label("Ready. Load the example intercept to begin.", "status");
    add(root, a->status);
    GtkWidget *v = page(a, "enigma", "ENIGMA", "ENIGMA I / M3"),
              *row = box(GTK_ORIENTATION_HORIZONTAL, 12);
    add(v, label("Three moving rotors. One reciprocal circuit. Every keypress steps before the "
                 "current flows.",
                 "subtitle"));
    add(v, row);
    const char *rotors[] = {"I", "II", "III", "IV", "V", NULL};
    const char *reflectors[] = {"B", "C", NULL};
    for (int i = 0; i < 3; i++) {
        a->rotor[i] = gtk_drop_down_new_from_strings(rotors);
        field(row, i == 0 ? "Left rotor" : i == 1 ? "Middle rotor" : "Right rotor", a->rotor[i]);
    }
    a->rings = field(row, "Ringstellung", entry("AAA", 5));
    a->positions = field(row, "Start windows", entry("AAA", 5));
    a->reflector = field(row, "Reflector", gtk_drop_down_new_from_strings(reflectors));
    gtk_entry_set_max_length(GTK_ENTRY(a->rings), 3);
    gtk_entry_set_max_length(GTK_ENTRY(a->positions), 3);
    row = box(GTK_ORIENTATION_HORIZONTAL, 8);
    add(v, row);
    button(row, "Apply / reset", G_CALLBACK(app_reset), a);
    button(row, "Randomize machine", G_CALLBACK(app_random_key), a);
    a->random_rings = gtk_check_button_new_with_label("Randomize rings too");
    add(row, a->random_rings);
    const char *speeds[] = {"Slow educational", "Normal", "Fast", "Instant", NULL};
    a->speed = field(row, "Animation", gtk_drop_down_new_from_strings(speeds));
    gtk_drop_down_set_selected(GTK_DROP_DOWN(a->speed), 1);
    a->enigma_canvas = canvas(v, 485, enigma_view_draw, a);
    gtk_widget_add_tick_callback(a->enigma_canvas, enigma_view_tick, a, NULL);
    row = box(GTK_ORIENTATION_HORIZONTAL, 12);
    add(v, row);
    a->key_entry = field(row, "Keyboard / click here and press A-Z", entry("", 30));
    gtk_entry_set_placeholder_text(GTK_ENTRY(a->key_entry), "Each letter advances the machine");
    GtkEventController *keyboard = gtk_event_controller_key_new();
    gtk_event_controller_set_propagation_phase(keyboard, GTK_PHASE_CAPTURE);
    g_signal_connect(keyboard, "key-pressed", G_CALLBACK(app_keyboard_press), a);
    gtk_widget_add_controller(a->key_entry, keyboard);
    g_signal_connect(a->key_entry, "activate", G_CALLBACK(app_key_entered), a);
    add(v, label("The illuminated route shows the actual electrical trace. Letter windows are "
                 "sampled from precomputed keypresses; animation never changes the ciphertext.",
                 "subtitle"));
    v = page(a, "plugboard", "PLUGBOARD", "STECKERBRETT");
    add(v, label("Click two sockets to connect a pair. Click a connected socket to remove its "
                 "cable. Up to ten pairs; unconnected letters map to themselves.",
                 "subtitle"));
    a->plug_canvas = canvas(v, 420, plugboard_view_draw, a);
    GtkGesture *click = gtk_gesture_click_new();
    g_signal_connect(click, "pressed", G_CALLBACK(plugboard_pressed), a);
    gtk_widget_add_controller(a->plug_canvas, GTK_EVENT_CONTROLLER(click));
    row = box(GTK_ORIENTATION_HORIZONTAL, 10);
    add(v, row);
    a->plug_text = entry("", 40);
    gtk_widget_set_hexpand(a->plug_text, TRUE);
    add(row, a->plug_text);
    button(row, "Apply pairs", G_CALLBACK(app_apply_plugs), a);
    button(row, "Randomize 10 pairs", G_CALLBACK(app_random_plugs), a);
    button(row, "Clear", G_CALLBACK(app_clear_plugs), a);
    v = page(a, "message", "MESSAGE / INTERCEPT", "MESSAGE DESK");
    row = box(GTK_ORIENTATION_HORIZONTAL, 8);
    add(v, row);
    button(row, "Encrypt", G_CALLBACK(app_encrypt), a);
    button(row, "Reset", G_CALLBACK(app_reset), a);
    button(row, "Hide as intercept", G_CALLBACK(app_keep_intercept), a);
    button(row, "Create random intercept", G_CALLBACK(app_intercept), a);
    button(row, "Example intercept", G_CALLBACK(app_example), a);
    button(row, "Copy ciphertext", G_CALLBACK(app_copy), a);
    a->auto_encrypt = gtk_check_button_new_with_label("Encrypt as I type");
    gtk_check_button_set_active(GTK_CHECK_BUTTON(a->auto_encrypt), TRUE);
    add(v, a->auto_encrypt);
    a->plain = text_view(
        v, "Plaintext  /  A-Z letters are retained; spaces and punctuation are removed", true, 115);
    a->cipher =
        text_view(v, "Intercepted ciphertext  /  editable for imported challenges", true, 115);
    a->decrypted =
        text_view(v, "Candidate decryption  /  activate a Bombe stop to inspect", false, 115);
    row = box(GTK_ORIENTATION_HORIZONTAL, 8);
    add(v, row);
    button(row, "Reveal secret key", G_CALLBACK(app_reveal), a);
    button(row, "Hide secret key", G_CALLBACK(app_hide), a);
    a->secret_label = label("No challenge generated.", "subtitle");
    add(row, a->secret_label);
    row = box(GTK_ORIENTATION_HORIZONTAL, 8);
    add(v, row);
    a->scenario_path = entry("scenario.ini", 30);
    gtk_widget_set_hexpand(a->scenario_path, TRUE);
    add(row, a->scenario_path);
    button(row, "Save scenario", G_CALLBACK(app_save), a);
    button(row, "Load scenario", G_CALLBACK(app_load), a);
    add(v,
        label("Scenarios are local INI files. Saved challenges include their secret key and "
              "plaintext for later reveal; do not distribute them if the secret must stay private.",
              "subtitle"));
    g_signal_connect(gtk_text_view_get_buffer(GTK_TEXT_VIEW(a->plain)), "changed",
                     G_CALLBACK(app_plain_changed), a);
    g_signal_connect(gtk_text_view_get_buffer(GTK_TEXT_VIEW(a->cipher)), "changed",
                     G_CALLBACK(app_menu_changed), a);
    v = page(a, "menu", "CRIB / MENU", "BUILD THE MENU");
    row = box(GTK_ORIENTATION_HORIZONTAL, 10);
    add(v, row);
    a->crib =
        field(row, "Suspected plaintext, 8-256 letters for a search", entry("WETTERBERICHT", 45));
    gtk_widget_set_hexpand(a->crib, TRUE);
    gtk_entry_set_max_length(GTK_ENTRY(a->crib), LAB_CRIB_MAX);
    a->alignment =
        field(row, "Offset, zero based", gtk_spin_button_new_with_range(0, LAB_TEXT_MAX - 1, 1));
    row = box(GTK_ORIENTATION_HORIZONTAL, 8);
    add(v, row);
    button(row, "Previous alignment", G_CALLBACK(app_previous_alignment), a);
    button(row, "Next alignment", G_CALLBACK(app_next_alignment), a);
    button(row, "Auto choose promising alignment", G_CALLBACK(app_auto_alignment), a);
    a->menu_label = label("Enter ciphertext to build the menu.", "stats");
    add(v, a->menu_label);
    a->menu_canvas = canvas(v, 540, menu_view_draw, a);
    add(v, label("Each edge enforces P(cipher) = S(position, P(plain)). Amber edges close cycles "
                 "in a spanning forest. Labels use zero-based message positions. More cycles "
                 "usually give stronger contradictions.",
                 "subtitle"));
    g_signal_connect(a->crib, "changed", G_CALLBACK(app_menu_changed), a);
    g_signal_connect(a->alignment, "value-changed", G_CALLBACK(app_menu_changed), a);
    v = page(a, "bombe", "BOMBE", "THE BOMBE ROOM");
    add(v, label("One virtual rack per modern CPU worker. The racks display sampled states, not "
                 "individual tests or the speed of wartime machinery.",
                 "subtitle"));
    row = box(GTK_ORIENTATION_HORIZONTAL, 8);
    add(v, row);
    a->threads = field(row, "CPU workers", gtk_spin_button_new_with_range(1, LAB_MAX_WORKERS, 1));
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(a->threads), a->initial_threads);
    unsigned presets[] = {1, 2, 4, 8, lab_cpu_count()};
    for (unsigned i = 0; i < 5; i++) {
        char name[32];
        g_snprintf(name, sizeof name, i == 4 ? "All CPUs (%u)" : "%u", presets[i]);
        GtkWidget *b = button(row, name, G_CALLBACK(thread_preset), a);
        g_object_set_data(G_OBJECT(b), "threads", GUINT_TO_POINTER(presets[i]));
    }
    const char *modes[] = {"Training / modern accelerated", "Training / historical display",
                           "Advanced / unknown rings", NULL};
    a->mode = field(row, "Search mode", gtk_drop_down_new_from_strings(modes));
    a->stop_limit = field(row, "Stops/state, 0 = all", gtk_spin_button_new_with_range(0, 4096, 1));
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(a->stop_limit), 64);
    add(v, label("Training: known rings and reflector, 1,054,560 rotor states. Advanced: all "
                 "rings, 18,534,946,560 states. Reflector remains known. Historical display uses "
                 "the same unrestricted search.",
                 "subtitle"));
    row = box(GTK_ORIENTATION_HORIZONTAL, 8);
    add(v, row);
    a->start_button = button(row, "START BOMBE", G_CALLBACK(app_start), a);
    gtk_widget_add_css_class(a->start_button, "suggested-action");
    a->pause_button = button(row, "Pause", G_CALLBACK(app_pause), a);
    a->resume_button = button(row, "Resume", G_CALLBACK(app_resume), a);
    a->stop_button = button(row, "Stop", G_CALLBACK(app_stop), a);
    gtk_widget_add_css_class(a->stop_button, "destructive-action");
    a->benchmark_button = button(row, "Benchmark CPU", G_CALLBACK(app_benchmark), a);
    a->progress = gtk_progress_bar_new();
    gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(a->progress), TRUE);
    add(v, a->progress);
    a->stats = label("No search running.", "stats");
    add(v, a->stats);
    a->graph_canvas = canvas(v, 115, graph_view_draw, a);
    GtkWidget *worker_scroll = gtk_scrolled_window_new();
    gtk_widget_set_size_request(worker_scroll, -1, 320);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(worker_scroll), GTK_POLICY_NEVER,
                                   GTK_POLICY_AUTOMATIC);
    add(v, worker_scroll);
    a->bombe_canvas = gtk_drawing_area_new();
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(a->bombe_canvas), 320);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(a->bombe_canvas), bombe_view_draw, a, NULL);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(worker_scroll), a->bombe_canvas);
    add(v, label("BOMBE STOPS  /  activate a row to load its machine and decryption", "section"));
    GtkWidget *results_scroll = gtk_scrolled_window_new();
    gtk_widget_set_size_request(results_scroll, -1, 220);
    add(v, results_scroll);
    a->results = gtk_list_box_new();
    gtk_list_box_set_activate_on_single_click(GTK_LIST_BOX(a->results), FALSE);
    gtk_list_box_set_sort_func(GTK_LIST_BOX(a->results), candidate_sort, NULL, NULL);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(results_scroll), a->results);
    g_signal_connect(a->results, "row-activated", G_CALLBACK(app_candidate_activated), a);
    add(v, label("Stops satisfy the crib; they are not proof of readable plaintext. Unresolved "
                 "letters are listed and use identity for preview. Rows are ranked by a simple "
                 "language score. The GUI retains 2,000 "
                 "rows; queue overflow and per-state stop limits are reported.",
                 "subtitle"));
    v = page(a, "benchmark", "BENCHMARK", "CPU SCALING");
    row = box(GTK_ORIENTATION_HORIZONTAL, 8);
    add(v, row);
    button(row, "Run benchmark", G_CALLBACK(app_benchmark), a);
    button(row, "Stop benchmark / search", G_CALLBACK(app_stop), a);
    add(v, label("Measured constraint-kernel work, using CLOCK_MONOTONIC. Each row repeats the "
                 "same 65,536-state workload for at least 0.75 seconds. Pool creation is excluded. "
                 "Job dispatch and completion wakeups are included. Pause other CPU-heavy work for "
                 "useful "
                 "comparisons.",
                 "subtitle"));
    a->benchmark_text = label("Threads        states/sec       speedup     efficiency", "stats");
    add(v, a->benchmark_text);
    a->benchmark_canvas = canvas(v, 420, benchmark_view_draw, a);
}
