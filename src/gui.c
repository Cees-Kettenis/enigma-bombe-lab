#include "gui.h"
#include "wrap_box.h"
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
    return lab_wrap_box_new(spacing);
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
    pango_attr_list_insert(attributes, pango_attr_line_height_new(1.1));
    gtk_label_set_attributes(GTK_LABEL(w), attributes);
    pango_attr_list_unref(attributes);
    return w;
}
static void add(GtkWidget *b, GtkWidget *w) {
    if (LAB_IS_WRAP_BOX(b))
        lab_wrap_box_append(LAB_WRAP_BOX(b), w);
    else
        gtk_box_append(GTK_BOX(b), w);
}
static void explanation(GtkWidget *parent, const char *title, const char *text) {
    GtkWidget *expander = gtk_expander_new(title);
    gtk_widget_add_css_class(expander, "explanation");
    GtkWidget *body = label(text, "subtitle");
    gtk_widget_set_margin_top(body, 10);
    gtk_expander_set_child(GTK_EXPANDER(expander), body);
    add(parent, expander);
}
static GtkWidget *button(GtkWidget *b, const char *text, GCallback cb, App *a) {
    GtkWidget *w = gtk_button_new_with_label(text);
    gtk_widget_set_valign(w, GTK_ALIGN_END);
    g_signal_connect(w, "clicked", cb, a);
    add(b, w);
    return w;
}
static GtkWidget *entry(const char *value, int width) {
    GtkWidget *e = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(e), value);
    gtk_editable_set_width_chars(GTK_EDITABLE(e), width);
    gtk_editable_set_max_width_chars(GTK_EDITABLE(e), width);
    return e;
}
static GtkWidget *field(GtkWidget *b, const char *title, GtkWidget *w) {
    if (GTK_IS_SPIN_BUTTON(w))
        gtk_editable_set_width_chars(GTK_EDITABLE(w), 4);
    GtkWidget *v = box(GTK_ORIENTATION_VERTICAL, 2);
    add(v, label(title, "subtitle"));
    add(v, w);
    add(b, v);
    return w;
}
static GtkWidget *page(App *a, const char *name, const char *title, const char *heading) {
    GtkWidget *container = box(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *header = label(heading, "title");
    gtk_widget_set_margin_start(header, 14);
    gtk_widget_set_margin_end(header, 14);
    gtk_widget_set_margin_top(header, 6);
    gtk_widget_set_margin_bottom(header, 4);
    add(container, header);
    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER,
                                   GTK_POLICY_AUTOMATIC);
    GtkWidget *v = box(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_margin_start(v, 14);
    gtk_widget_set_margin_end(v, 14);
    gtk_widget_set_margin_top(v, 4);
    gtk_widget_set_margin_bottom(v, 8);
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
    if (!visible)
        return;
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
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(v), 8);
    gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(v), 8);
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
static void search_mode_changed(GObject *object, GParamSpec *property, gpointer data) {
    (void)object;
    (void)property;
    App *a = data;
    bool no_crib = gtk_drop_down_get_selected(GTK_DROP_DOWN(a->mode)) == 3;
    GtkWidget *attempts_row = g_object_get_data(G_OBJECT(a->blind_restarts), "controls-row");
    gtk_widget_set_visible(attempts_row, no_crib);
    gtk_widget_set_sensitive(gtk_widget_get_parent(a->stop_limit), !no_crib);
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
    gtk_header_bar_set_title_widget(
        GTK_HEADER_BAR(header), gtk_label_new("Enigma Bombe Lab  ·  A codebreaking playground"));
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
    gtk_widget_set_margin_top(switcher, 4);
    gtk_widget_set_margin_bottom(switcher, 4);
    add(root, switcher);
    tutorial_build(a, root);
    a->status = label("Ready to play. Write a message or open the tutorial for a guided first run.",
                      "status");
    add(root, a->status);
    add(root, a->stack);
    GtkWidget *v = page(a, "enigma", "ENIGMA", "Inside the Enigma"),
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
    a->rings = field(row, "Ring settings", entry("AAA", 5));
    a->positions = field(row, "Start windows", entry("AAA", 5));
    a->reflector = field(row, "Reflector", gtk_drop_down_new_from_strings(reflectors));
    gtk_entry_set_max_length(GTK_ENTRY(a->rings), 3);
    gtk_entry_set_max_length(GTK_ENTRY(a->positions), 3);
    button(row, "Apply / reset", G_CALLBACK(app_reset), a);
    button(row, "Randomize machine", G_CALLBACK(app_random_key), a);
    a->random_rings = gtk_check_button_new_with_label("Randomize rings too");
    add(row, a->random_rings);
    const char *speeds[] = {"Step by step", "Normal", "Fast", "Instant", NULL};
    a->speed = field(row, "Animation", gtk_drop_down_new_from_strings(speeds));
    gtk_drop_down_set_selected(GTK_DROP_DOWN(a->speed), 1);
    a->key_entry = field(row, "Keyboard / press A-Z", entry("", 24));
    gtk_entry_set_placeholder_text(GTK_ENTRY(a->key_entry), "Type A-Z to encrypt");
    GtkEventController *keyboard = gtk_event_controller_key_new();
    gtk_event_controller_set_propagation_phase(keyboard, GTK_PHASE_CAPTURE);
    g_signal_connect(keyboard, "key-pressed", G_CALLBACK(app_keyboard_press), a);
    gtk_widget_add_controller(a->key_entry, keyboard);
    g_signal_connect(a->key_entry, "activate", G_CALLBACK(app_key_entered), a);
    a->enigma_canvas = canvas(v, 340, enigma_view_draw, a);
    gtk_widget_set_vexpand(a->enigma_canvas, TRUE);
    gtk_widget_add_tick_callback(a->enigma_canvas, enigma_view_tick, a, NULL);
    gtk_widget_set_tooltip_text(a->enigma_canvas,
        "Follow the glowing path to the lamp. Try Step by step to see each letter change.");
    v = page(a, "plugboard", "PLUGBOARD", "Connect the plugboard");
    add(v, label("Click two sockets to connect a pair. Click a connected socket to remove its "
                 "cable. Up to ten pairs; unconnected letters map to themselves.",
                 "subtitle"));
    row = box(GTK_ORIENTATION_HORIZONTAL, 10);
    add(v, row);
    a->plug_text = entry("", 40);
    gtk_widget_set_hexpand(a->plug_text, TRUE);
    add(row, a->plug_text);
    button(row, "Apply pairs", G_CALLBACK(app_apply_plugs), a);
    button(row, "Randomize 10 pairs", G_CALLBACK(app_random_plugs), a);
    button(row, "Clear", G_CALLBACK(app_clear_plugs), a);
    a->plug_canvas = canvas(v, 320, plugboard_view_draw, a);
    GtkGesture *click = gtk_gesture_click_new();
    g_signal_connect(click, "pressed", G_CALLBACK(plugboard_pressed), a);
    gtk_widget_add_controller(a->plug_canvas, GTK_EVENT_CONTROLLER(click));
    gtk_widget_set_vexpand(a->plug_canvas, TRUE);
    v = page(a, "message", "MESSAGE / INTERCEPT", "Your message desk");
    add(v, label("Write a secret, encrypt it, then turn it into a codebreaking challenge. "
                 "Or paste a ciphertext of your own below.",
                 "subtitle"));
    row = box(GTK_ORIENTATION_HORIZONTAL, 8);
    add(v, row);
    gtk_widget_add_css_class(button(row, "Encrypt", G_CALLBACK(app_encrypt), a),
                             "suggested-action");
    button(row, "Reset machine", G_CALLBACK(app_reset), a);
    button(row, "Hide as intercept", G_CALLBACK(app_keep_intercept), a);
    button(row, "Create random intercept", G_CALLBACK(app_intercept), a);
    button(row, "Example intercept", G_CALLBACK(app_example), a);
    button(row, "Copy ciphertext", G_CALLBACK(app_copy), a);
    a->auto_encrypt = gtk_check_button_new_with_label("Encrypt as I type");
    gtk_check_button_set_active(GTK_CHECK_BUTTON(a->auto_encrypt), TRUE);
    gtk_widget_set_valign(a->auto_encrypt, GTK_ALIGN_CENTER);
    add(row, a->auto_encrypt);
    GtkWidget *editors = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_set_homogeneous(GTK_BOX(editors), TRUE);
    gtk_widget_set_vexpand(editors, TRUE);
    add(v, editors);
    GtkWidget *plain_column = box(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *cipher_column = box(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *answer_column = box(GTK_ORIENTATION_VERTICAL, 4);
    add(editors, plain_column);
    add(editors, cipher_column);
    add(editors, answer_column);
    a->plain = text_view(plain_column, "Your message / A-Z", true, 220);
    a->cipher = text_view(cipher_column, "Ciphertext / paste here", true, 220);
    a->decrypted = text_view(answer_column, "Candidate decryption", false, 220);
    gtk_widget_set_tooltip_text(a->cipher,
        "Paste encrypted text here. Use CRIB / MENU for a known phrase, or choose English "
        "detective on BOMBE for at least 50 letters. Supply the rings and reflector on ENIGMA.");
    row = box(GTK_ORIENTATION_HORIZONTAL, 8);
    add(v, row);
    button(row, "Reveal secret key", G_CALLBACK(app_reveal), a);
    button(row, "Hide secret key", G_CALLBACK(app_hide), a);
    a->secret_label = label("No challenge generated.", "subtitle");
    a->scenario_path = entry("scenario.ini", 30);
    gtk_widget_set_hexpand(a->scenario_path, TRUE);
    add(row, a->scenario_path);
    button(row, "Save scenario", G_CALLBACK(app_save), a);
    button(row, "Load scenario", G_CALLBACK(app_load), a);
    add(v, a->secret_label);
    explanation(
        v, "What gets saved?",
        "Save a scenario to pick up where you left off. It keeps your settings and message. "
        "Challenges also keep the original message and key for the Reveal button, so anyone "
        "with the file can look them up. The search never uses them to rate answers.");
    g_signal_connect(gtk_text_view_get_buffer(GTK_TEXT_VIEW(a->plain)), "changed",
                     G_CALLBACK(app_plain_changed), a);
    g_signal_connect(gtk_text_view_get_buffer(GTK_TEXT_VIEW(a->cipher)), "changed",
                     G_CALLBACK(app_menu_changed), a);
    v = page(a, "menu", "CRIB / MENU", "Follow a clue");
    add(v, label("Think you know a phrase in the message? That clue is called a crib. "
                 "Slide it along the ciphertext and watch the connections appear.",
                 "subtitle"));
    row = box(GTK_ORIENTATION_HORIZONTAL, 10);
    add(v, row);
    a->crib =
        field(row, "Suspected plaintext, 8-256 letters for a search", entry("WETTERBERICHT", 45));
    gtk_widget_set_hexpand(a->crib, TRUE);
    gtk_entry_set_max_length(GTK_ENTRY(a->crib), LAB_CRIB_MAX);
    a->alignment = field(row, "Clue position, 0 = beginning",
                         gtk_spin_button_new_with_range(0, LAB_TEXT_MAX - 1, 1));
    button(row, "Previous alignment", G_CALLBACK(app_previous_alignment), a);
    button(row, "Next alignment", G_CALLBACK(app_next_alignment), a);
    button(row, "Auto choose promising alignment", G_CALLBACK(app_auto_alignment), a);
    a->menu_label = label("Enter ciphertext to build the menu.", "stats");
    add(v, a->menu_label);
    a->menu_canvas = canvas(v, 360, menu_view_draw, a);
    gtk_widget_set_vexpand(a->menu_canvas, TRUE);
    explanation(v, "Reading the connection map",
                "Each line links a letter in your clue to a letter in the ciphertext. "
                "Amber lines complete loops, which help rule out wrong settings. "
                "The numbers show message positions, starting at zero. In the circuit, each "
                "connection must satisfy P(cipher) = S(position, P(plain)).");
    g_signal_connect(a->crib, "changed", G_CALLBACK(app_menu_changed), a);
    g_signal_connect(a->alignment, "value-changed", G_CALLBACK(app_menu_changed), a);
    v = page(a, "bombe", "BOMBE", "The codebreaking room");
    row = box(GTK_ORIENTATION_HORIZONTAL, 8);
    add(v, row);
    const char *modes[] = {"Training / with a clue", "Training / classic display",
                           "Advanced / unknown rings", "No crib / English detective", NULL};
    a->mode = field(row, "Search mode", gtk_drop_down_new_from_strings(modes));
    a->threads = field(row, "CPU workers", gtk_spin_button_new_with_range(1, LAB_MAX_WORKERS, 1));
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(a->threads), a->initial_threads);
    a->stop_confidence = field(row, "Stop confidence %", gtk_spin_button_new_with_range(0, 100, 1));
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(a->stop_confidence), 80);
    gtk_widget_set_tooltip_text(a->stop_confidence,
        "Stop when a candidate reaches this English confidence. 0 disables automatic stopping. "
        "This is a language-fit estimate, not proof of the correct answer.");
    a->stop_limit = field(row, "Stops/state, 0 = all", gtk_spin_button_new_with_range(0, 4096, 1));
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(a->stop_limit), 64);
    a->blind_restarts = field(row, "Attempts/state", gtk_spin_button_new_with_range(1, 16, 1));
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(a->blind_restarts), 2);
    g_object_set_data(G_OBJECT(a->blind_restarts), "controls-row",
                     gtk_widget_get_parent(a->blind_restarts));
    g_signal_connect(a->mode, "notify::selected", G_CALLBACK(search_mode_changed), a);
    search_mode_changed(NULL, NULL, a);
    a->start_button = button(row, "START BOMBE", G_CALLBACK(app_start), a);
    gtk_widget_add_css_class(a->start_button, "suggested-action");
    a->pause_button = button(row, "Pause", G_CALLBACK(app_pause), a);
    a->resume_button = button(row, "Resume", G_CALLBACK(app_resume), a);
    a->stop_button = button(row, "Stop", G_CALLBACK(app_stop), a);
    gtk_widget_add_css_class(a->stop_button, "destructive-action");
    a->benchmark_button = button(row, "Benchmark CPU", G_CALLBACK(app_benchmark), a);
    GtkWidget *all_cpus = button(row, "All CPUs", G_CALLBACK(thread_preset), a);
    g_object_set_data(G_OBJECT(all_cpus), "threads", GUINT_TO_POINTER(lab_cpu_count()));
    a->progress = gtk_progress_bar_new();
    gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(a->progress), TRUE);
    add(v, a->progress);
    a->stats = label("Your next puzzle starts here. Choose a mode and press START BOMBE.", "stats");
    add(v, a->stats);
    a->graph_canvas = canvas(v, 65, graph_view_draw, a);
    GtkWidget *dashboard = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_set_position(GTK_PANED(dashboard), 620);
    gtk_widget_set_vexpand(dashboard, TRUE);
    add(v, dashboard);
    GtkWidget *workers_column = box(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *results_column = box(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_end(workers_column, 8);
    gtk_widget_set_margin_start(results_column, 8);
    gtk_paned_set_start_child(GTK_PANED(dashboard), workers_column);
    gtk_paned_set_end_child(GTK_PANED(dashboard), results_column);
    add(workers_column, label("Worker drums", "section"));
    add(results_column, label("Candidates / highest confidence first", "section"));
    GtkWidget *worker_scroll = gtk_scrolled_window_new();
    gtk_widget_set_size_request(worker_scroll, 290, 250);
    gtk_widget_set_vexpand(worker_scroll, TRUE);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(worker_scroll), GTK_POLICY_NEVER,
                                   GTK_POLICY_AUTOMATIC);
    add(workers_column, worker_scroll);
    a->bombe_canvas = gtk_drawing_area_new();
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(a->bombe_canvas), 320);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(a->bombe_canvas), bombe_view_draw, a, NULL);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(worker_scroll), a->bombe_canvas);
    GtkWidget *results_scroll = gtk_scrolled_window_new();
    gtk_widget_set_size_request(results_scroll, 290, 250);
    gtk_widget_set_vexpand(results_scroll, TRUE);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(results_scroll), GTK_POLICY_NEVER,
                                   GTK_POLICY_AUTOMATIC);
    add(results_column, results_scroll);
    gtk_widget_set_tooltip_text(results_scroll,
                               "Double-click a candidate or press Enter to read the full message.");
    a->results = gtk_list_box_new();
    gtk_list_box_set_activate_on_single_click(GTK_LIST_BOX(a->results), FALSE);
    gtk_list_box_set_sort_func(GTK_LIST_BOX(a->results), candidate_sort, NULL, NULL);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(results_scroll), a->results);
    g_signal_connect(a->results, "row-activated", G_CALLBACK(app_candidate_activated), a);
    explanation(v, "Search help and confidence",
                "Stop confidence defaults to 80%. Set it to 0 to keep searching. On reaching the "
                "target, workers stop and the best received answer appears in Candidate decryption. "
                "Double-click a candidate to inspect it and load its machine settings.\n\n"
                "Training needs a clue of at least eight letters and a valid position. Advanced "
                "also searches all rings. English detective needs at least 50 ciphertext letters; "
                "300-500 letters of ordinary English are a good first puzzle. It ignores the clue "
                "and tries plugboards at every rotor state. More attempts take longer. Supply "
                "the rings and reflector on ENIGMA. A full no-crib search can take hours.\n\n"
                "Stops/state limits clue-based answers per state; 0 removes that limit. "
                "CPU workers share the search across your processor.\n\n"
                "The percentage rates how closely the decoded letters resemble English. Higher "
                "scores appear first. It is a reading guide, not a measured probability of the "
                "correct key. Read the whole message and decide for yourself.\n\n"
                "The original answer is never used to score, rank or approve candidates. "
                "In clue-based modes, answers also fit your crib. Hover over a row for its raw "
                "language score. The list keeps the best 2,000 received answers; the dashboard "
                "reports any search or queue limits.");
    v = page(a, "benchmark", "BENCHMARK", "How fast can you crack it?");
    row = box(GTK_ORIENTATION_HORIZONTAL, 8);
    add(v, row);
    button(row, "Run benchmark", G_CALLBACK(app_benchmark), a);
    button(row, "Stop benchmark / search", G_CALLBACK(app_stop), a);
    add(v, label("See how sharing the work across CPU cores changes search speed. "
                 "Run the comparison, then try a different worker count in the Bombe room.",
                 "subtitle"));
    explanation(v, "How the speed test works",
                "Each row repeats the same 65,536-state workload for at least 0.75 seconds. "
                "Timing includes dispatch and completion, but excludes starting the worker pool. "
                "For a clearer comparison, pause other CPU-heavy work while the test runs.");
    a->benchmark_text = label("Threads        states/sec       speedup     efficiency", "stats");
    add(v, a->benchmark_text);
    a->benchmark_canvas = canvas(v, 300, benchmark_view_draw, a);
}
