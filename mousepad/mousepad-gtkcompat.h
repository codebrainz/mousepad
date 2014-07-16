#ifndef __MOUSEPAD_GTK_COMPAT_H__
#define __MOUSEPAD_GTK_COMPAT_H__ 1

#include <gtk/gtk.h>

#if ! GTK_CHECK_VERSION(2, 24, 0)
# define gtk_combo_box_text_new_with_entry gtk_combo_box_entry_new_text
# define gtk_combo_box_text_new            gtk_combo_box_new_text
#endif

#if ! GTK_CHECK_VERSION(3, 0, 0)
static inline gint
gtk_widget_get_allocated_width (GtkWidget *widget)
{
  GtkAllocation alloc = { 0, 0, 0, 0 };
  g_return_val_if_fail (GTK_IS_WIDGET (widget), -1);
  gtk_widget_get_allocation (widget, &alloc);
  return alloc.width;
}
static inline gint
gtk_widget_get_allocated_height (GtkWidget *widget)
{
  GtkAllocation alloc = { 0, 0, 0, 0 };
  g_return_val_if_fail (GTK_IS_WIDGET (widget), -1);
  gtk_widget_get_allocation (widget, &alloc);
  return alloc.height;
}
#endif

#endif /* __MOUSEPAD_GTK_COMPAT_H__ */
