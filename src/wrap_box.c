#include "wrap_box.h"
#include <stdbool.h>

/* Toolbars wrap at actual control widths, without FlowBox's aligned grid columns. */
struct _LabWrapBox {
    GtkWidget parent_instance;
    int spacing;
};
G_DEFINE_TYPE(LabWrapBox, lab_wrap_box, GTK_TYPE_WIDGET)

typedef struct {
    GtkWidget *widget;
    int width;
    bool expand;
} Item;

static int layout(LabWrapBox *box, int width, bool allocate) {
    GArray *items = g_array_new(FALSE, FALSE, sizeof(Item));
    for (GtkWidget *child = gtk_widget_get_first_child(GTK_WIDGET(box)); child;
         child = gtk_widget_get_next_sibling(child)) {
        if (!gtk_widget_should_layout(child))
            continue;
        int natural;
        gtk_widget_measure(child, GTK_ORIENTATION_HORIZONTAL, -1, NULL, &natural, NULL, NULL);
        Item item = {child, MIN(natural, MAX(width, 1)),
                     gtk_widget_compute_expand(child, GTK_ORIENTATION_HORIZONTAL)};
        g_array_append_val(items, item);
    }
    int y = 0;
    for (guint first = 0; first < items->len;) {
        guint end = first, expand = 0;
        int used = 0;
        while (end < items->len) {
            Item *item = &g_array_index(items, Item, end);
            int next = used + (end > first ? box->spacing : 0) + item->width;
            if (end > first && next > width)
                break;
            used = next;
            expand += item->expand;
            end++;
        }
        int extra = MAX(0, width - used), height = 0;
        for (guint i = first; i < end; i++) {
            Item *item = &g_array_index(items, Item, i);
            if (item->expand) {
                int share = extra / (int)expand;
                item->width += share;
                extra -= share;
                expand--;
            }
            int child_height;
            gtk_widget_measure(item->widget, GTK_ORIENTATION_VERTICAL, item->width, NULL,
                               &child_height, NULL, NULL);
            height = MAX(height, child_height);
        }
        if (allocate) {
            int x = 0;
            bool rtl = gtk_widget_get_direction(GTK_WIDGET(box)) == GTK_TEXT_DIR_RTL;
            for (guint i = first; i < end; i++) {
                Item *item = &g_array_index(items, Item, i);
                graphene_point_t point = GRAPHENE_POINT_INIT(rtl ? width - x - item->width : x, y);
                gtk_widget_allocate(item->widget, item->width, height, -1,
                                    gsk_transform_translate(NULL, &point));
                x += item->width + box->spacing;
            }
        }
        y += height + (end < items->len ? box->spacing : 0);
        first = end;
    }
    g_array_free(items, TRUE);
    return y;
}
static GtkSizeRequestMode request_mode(GtkWidget *widget) {
    (void)widget;
    return GTK_SIZE_REQUEST_HEIGHT_FOR_WIDTH;
}
static void measure(GtkWidget *widget, GtkOrientation orientation, int for_size, int *minimum,
                    int *natural, int *minimum_baseline, int *natural_baseline) {
    LabWrapBox *box = LAB_WRAP_BOX(widget);
    *minimum = *natural = 0;
    *minimum_baseline = *natural_baseline = -1;
    if (orientation == GTK_ORIENTATION_VERTICAL) {
        *minimum = *natural = layout(box, for_size < 0 ? G_MAXINT : for_size, false);
        return;
    }
    bool first = true;
    for (GtkWidget *child = gtk_widget_get_first_child(widget); child;
         child = gtk_widget_get_next_sibling(child)) {
        if (!gtk_widget_should_layout(child))
            continue;
        int min, nat;
        gtk_widget_measure(child, orientation, -1, &min, &nat, NULL, NULL);
        *minimum = MAX(*minimum, min);
        *natural += nat + (first ? 0 : box->spacing);
        first = false;
    }
}
static void size_allocate(GtkWidget *widget, int width, int height, int baseline) {
    (void)height;
    (void)baseline;
    layout(LAB_WRAP_BOX(widget), width, true);
}
static void snapshot(GtkWidget *widget, GtkSnapshot *snapshot) {
    for (GtkWidget *child = gtk_widget_get_first_child(widget); child;
         child = gtk_widget_get_next_sibling(child))
        gtk_widget_snapshot_child(widget, child, snapshot);
}
static gboolean focus(GtkWidget *widget, GtkDirectionType direction) {
    bool backwards =
        direction == GTK_DIR_TAB_BACKWARD || direction == GTK_DIR_LEFT || direction == GTK_DIR_UP;
    GtkWidget *child = gtk_widget_get_focus_child(widget);
    if (!child)
        child = backwards ? gtk_widget_get_last_child(widget) : gtk_widget_get_first_child(widget);
    for (; child; child = backwards ? gtk_widget_get_prev_sibling(child)
                                    : gtk_widget_get_next_sibling(child))
        if (gtk_widget_get_visible(child) && gtk_widget_child_focus(child, direction))
            return TRUE;
    return FALSE;
}
static void dispose(GObject *object) {
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(GTK_WIDGET(object))))
        gtk_widget_unparent(child);
    G_OBJECT_CLASS(lab_wrap_box_parent_class)->dispose(object);
}
static void lab_wrap_box_class_init(LabWrapBoxClass *klass) {
    GtkWidgetClass *widget = GTK_WIDGET_CLASS(klass);
    widget->get_request_mode = request_mode;
    widget->measure = measure;
    widget->size_allocate = size_allocate;
    widget->snapshot = snapshot;
    widget->focus = focus;
    G_OBJECT_CLASS(klass)->dispose = dispose;
    gtk_widget_class_set_css_name(widget, "wrapbox");
}
static void lab_wrap_box_init(LabWrapBox *box) {
    gtk_widget_set_valign(GTK_WIDGET(box), GTK_ALIGN_START);
}
GtkWidget *lab_wrap_box_new(int spacing) {
    LabWrapBox *box = g_object_new(LAB_TYPE_WRAP_BOX, NULL);
    box->spacing = spacing;
    return GTK_WIDGET(box);
}
void lab_wrap_box_append(LabWrapBox *box, GtkWidget *child) {
    gtk_widget_set_parent(child, GTK_WIDGET(box));
}
