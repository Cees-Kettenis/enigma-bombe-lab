#include "app.h"
#include <string.h>

static const char *practice_crib =
    "WETTERBERICHTTHEQUICKBROWNFOXJUMPSOVERTHELAZYDOGPACKMYBOXWITHFIVEDOZENLIQUORJUGS";
static const char *practice_message = "WETTERBERICHT THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG "
                                      "PACK MY BOX WITH FIVE DOZEN LIQUOR JUGS\n"
                                      "MEET ME AT THE STATION AT NINE";

typedef struct {
    const char *page, *title, *text, *action;
} Lesson;
static const Lesson lessons[] = {
    {"enigma", "Your first secret message",
     "Encrypt a message, then let your PC recover it using a known phrase. Each button below "
     "performs the step in the real app. Starting replaces the current message and settings. "
     "Save your scenario first if you want to keep it. You can exit the tutorial at any time.",
     "Start with practice message"},
    {"enigma", "Scramble the machine",
     "The three rotors scramble each letter. Their order and starting letters are part of the "
     "secret key. Randomize machine picks these and ten plugboard pairs. For this first lesson "
     "we keep Ringstellung at AAA and reflector B; the PC will know those two settings.",
     "Randomize machine"},
    {"plugboard", "Mix up the plugboard",
     "Each cable swaps two letters before and after the rotors. AG means A and G are swapped. "
     "Click two sockets to try connecting them; click a connected socket to remove its cable. "
     "Continue below to choose a fresh set of ten pairs for the lesson.",
     "Randomize 10 pairs"},
    {"message", "Write and encrypt",
     "Scroll to Your message. Keep the supplied first line for this lesson, but change the final "
     "sentence to your own message. That first line will be the codebreaker's known phrase. "
     "Encrypt turns the text into ciphertext using your scrambled settings. Spaces and "
     "punctuation disappear; only A-Z are encrypted.",
     "Encrypt my message"},
    {"enigma", "Watch the encryption",
     "The drums step and the output lamp lights as each letter passes through the machine. "
     "The ciphertext is already calculated; the animation just explains it. See it on the "
     "MESSAGE / INTERCEPT tab. Continue to hide this exact key and plaintext, keeping the "
     "ciphertext for the PC to attack. Do not change the settings between these steps.",
     "Hide this message as an intercept"},
    {"menu", "Give the PC a clue",
     "A crib is text you suspect is inside the message. We deliberately know the first line "
     "of our practice message. It is entered below at offset 0, the beginning. The menu graph "
     "connects this clue to the ciphertext. A long clue covering all letters makes this first "
     "search reliable; short guesses can leave many possible answers.",
     "Use this crib and choose workers"},
    {"bombe", "Let your PC search",
     "Choose CPU workers below: 4 is a useful starting point, or choose All CPUs. More workers "
     "can share the work. Stay in Training mode for this lesson. START BOMBE tries rotor orders "
     "and starting letters while deducing plugboard connections from the crib. It never reads "
     "the hidden key or message.",
     "START BOMBE"},
    {"bombe", "Watch for candidate answers",
     "Watch the progress, then scroll down to the worker drums and Candidate answers. Each stop is "
     "a "
     "candidate that fits the clue, not automatically the right answer. Pause, Resume and Stop "
     "are available below. When the search finishes, inspect the highest-ranked candidate. "
     "If you stop early, resume this lesson by pressing START BOMBE to run a fresh search.",
     "Waiting for search..."},
    {"message", "Read your recovered message",
     "Scroll to Candidate decryption and check your final sentence. The selected candidate's "
     "settings are also loaded in ENIGMA and PLUGBOARD. Reveal secret key lets you compare "
     "with the original afterward. For another message, encrypt it, use Hide as intercept, "
     "enter a phrase you expect in it, then START BOMBE. Or try English detective to explore "
     "a longer English message without a clue. Tutorial in the header starts this lesson again.",
     "Finish tutorial"},
};

static void show_step(App *a) {
    const Lesson *lesson = &lessons[a->tutorial_step];
    char title[160];
    g_snprintf(title, sizeof title, "Tutorial %u / %u: %s", a->tutorial_step + 1,
               (unsigned)G_N_ELEMENTS(lessons), lesson->title);
    gtk_label_set_text(GTK_LABEL(a->tutorial_title), title);
    gtk_label_set_text(GTK_LABEL(a->tutorial_text), lesson->text);
    gtk_button_set_label(GTK_BUTTON(a->tutorial_next), lesson->action);
    gtk_widget_set_sensitive(a->tutorial_next, TRUE);
    gtk_stack_set_visible_child_name(GTK_STACK(a->stack), lesson->page);
    tutorial_refresh(a);
}

static void exit_tutorial(GtkButton *button, gpointer data) {
    (void)button;
    App *a = data;
    gtk_widget_set_visible(a->tutorial_panel, FALSE);
}

static bool practice_text_valid(App *a) {
    char *text = app_text(a->plain), normalized[LAB_TEXT_MAX];
    enigma_normalize(text, normalized, sizeof normalized);
    bool valid = strlen(text) < LAB_TEXT_MAX && g_str_has_prefix(normalized, practice_crib);
    g_free(text);
    if (!valid)
        app_status(a, "For this lesson, keep the supplied first line and edit only the ending. "
                      "Exit and reopen Tutorial to start over with the practice text.");
    return valid;
}

static void advance(GtkButton *button, gpointer data) {
    (void)button;
    App *a = data;
    if (a->tutorial_step < 7 && (a->snapshot.running || a->benchmark_running)) {
        app_status(a, "Stop the current search or benchmark before continuing the lesson.");
        return;
    }
    switch (a->tutorial_step) {
    case 0:
        if (a->encrypt_timer) {
            g_source_remove(a->encrypt_timer);
            a->encrypt_timer = 0;
        }
        gtk_check_button_set_active(GTK_CHECK_BUTTON(a->auto_encrypt), FALSE);
        gtk_check_button_set_active(GTK_CHECK_BUTTON(a->random_rings), FALSE);
        enigma_key_default(&a->key);
        app_sync_key(a);
        a->challenge.present = false;
        a->trace_count = 0;
        a->animating = false;
        a->plug_selected = -1;
        app_set_text(a->plain, practice_message);
        app_set_text(a->cipher, "");
        app_set_text(a->decrypted, "");
        gtk_label_set_text(GTK_LABEL(a->secret_label), "No challenge generated.");
        gtk_drop_down_set_selected(GTK_DROP_DOWN(a->mode), 0);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(a->threads), MIN(4, lab_cpu_count()));
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(a->stop_limit), 64);
        gtk_drop_down_set_selected(GTK_DROP_DOWN(a->speed), 1);
        break;
    case 1:
        gtk_check_button_set_active(GTK_CHECK_BUTTON(a->random_rings), FALSE);
        app_random_key(NULL, a);
        memset(a->key.ring, 0, sizeof a->key.ring);
        a->key.reflector = 0;
        app_sync_key(a);
        break;
    case 2:
        app_random_plugs(NULL, a);
        break;
    case 3:
        if (!practice_text_valid(a) || !app_read_key(a))
            return;
        app_encrypt(NULL, a);
        break;
    case 4:
        if (!practice_text_valid(a) || !app_capture_intercept(a))
            return;
        gtk_editable_set_text(GTK_EDITABLE(a->crib), practice_crib);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(a->alignment), 0);
        app_menu_changed(NULL, a);
        break;
    case 5:
        if (strcmp(gtk_editable_get_text(GTK_EDITABLE(a->crib)), practice_crib) != 0 ||
            gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(a->alignment)) != 0) {
            app_status(a, "Keep the supplied crib and offset 0 for this lesson.");
            return;
        }
        app_menu_changed(NULL, a);
        if (!a->menu_valid) {
            app_status(a,
                       "The ciphertext no longer matches the lesson. Exit and restart Tutorial.");
            return;
        }
        gtk_drop_down_set_selected(GTK_DROP_DOWN(a->mode), 0);
        break;
    case 6:
        gtk_drop_down_set_selected(GTK_DROP_DOWN(a->mode), 0);
        app_start(NULL, a);
        if (!a->snapshot.running)
            return;
        break;
    case 7: {
        if (a->snapshot.running || !a->result_count)
            return;
        GtkListBoxRow *row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(a->results), 0);
        app_candidate_activated(GTK_LIST_BOX(a->results), row, a);
        break;
    }
    default:
        exit_tutorial(NULL, a);
        return;
    }
    a->tutorial_step++;
    show_step(a);
    if (a->tutorial_step == 8)
        gtk_widget_grab_focus(a->decrypted);
}

void tutorial_refresh(App *a) {
    if (!a->tutorial_panel || !gtk_widget_get_visible(a->tutorial_panel) || a->tutorial_step != 7)
        return;
    bool ready = !a->snapshot.running && a->result_count > 0;
    gtk_widget_set_sensitive(a->tutorial_next, ready);
    gtk_button_set_label(GTK_BUTTON(a->tutorial_next),
                         ready                 ? "Inspect best candidate"
                         : a->snapshot.running ? "Waiting for search..."
                                               : "No candidate yet: use START BOMBE below");
}

void app_tutorial(GtkButton *button, gpointer data) {
    (void)button;
    App *a = data;
    if (!gtk_widget_get_visible(a->tutorial_panel))
        a->tutorial_step = 0;
    gtk_widget_set_visible(a->tutorial_panel, TRUE);
    show_step(a);
}

void tutorial_build(App *a, GtkWidget *parent) {
    a->tutorial_panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_add_css_class(a->tutorial_panel, "tutorial");
    gtk_widget_set_margin_start(a->tutorial_panel, 16);
    gtk_widget_set_margin_end(a->tutorial_panel, 16);
    gtk_widget_set_margin_bottom(a->tutorial_panel, 8);
    a->tutorial_title = gtk_label_new("");
    gtk_widget_add_css_class(a->tutorial_title, "section");
    a->tutorial_text = gtk_label_new("");
    GtkWidget *labels[] = {a->tutorial_title, a->tutorial_text};
    for (unsigned i = 0; i < G_N_ELEMENTS(labels); i++) {
        gtk_label_set_xalign(GTK_LABEL(labels[i]), 0);
        gtk_label_set_wrap(GTK_LABEL(labels[i]), TRUE);
        gtk_label_set_wrap_mode(GTK_LABEL(labels[i]), PANGO_WRAP_WORD_CHAR);
        gtk_box_append(GTK_BOX(a->tutorial_panel), labels[i]);
    }
    GtkWidget *buttons = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    a->tutorial_next = gtk_button_new_with_label("Start");
    gtk_widget_add_css_class(a->tutorial_next, "suggested-action");
    g_signal_connect(a->tutorial_next, "clicked", G_CALLBACK(advance), a);
    gtk_box_append(GTK_BOX(buttons), a->tutorial_next);
    GtkWidget *exit = gtk_button_new_with_label("Exit tutorial");
    g_signal_connect(exit, "clicked", G_CALLBACK(exit_tutorial), a);
    gtk_box_append(GTK_BOX(buttons), exit);
    gtk_box_append(GTK_BOX(a->tutorial_panel), buttons);
    gtk_box_append(GTK_BOX(parent), a->tutorial_panel);
    gtk_widget_set_visible(a->tutorial_panel, FALSE);
}
