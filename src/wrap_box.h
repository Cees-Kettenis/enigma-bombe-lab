#ifndef WRAP_BOX_H
#define WRAP_BOX_H
#include <gtk/gtk.h>
#define LAB_TYPE_WRAP_BOX (lab_wrap_box_get_type())
G_DECLARE_FINAL_TYPE(LabWrapBox, lab_wrap_box, LAB, WRAP_BOX, GtkWidget)
GtkWidget *lab_wrap_box_new(int spacing);
void lab_wrap_box_append(LabWrapBox *box, GtkWidget *child);
#endif
