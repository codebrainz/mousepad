#ifndef __MOUSEPAD_GTK_COMPAT_H__
#define __MOUSEPAD_GTK_COMPAT_H__ 1

#include <gtk/gtk.h>

#if GTK_CHECK_VERSION(3, 0, 0)
# include <gdk/gdkkeysyms-compat.h>
#else
# include <gdk/gdkkeysyms.h>
#endif

G_BEGIN_DECLS

#if ! GTK_CHECK_VERSION(2, 24, 0)
# define gtk_combo_box_text_new_with_entry gtk_combo_box_entry_new_text
# define gtk_combo_box_text_new            gtk_combo_box_new_text
# define gtk_combo_box_text_append_text    gtk_combo_box_append_text
# define GtkComboBoxText                   GtkComboBox
# define GTK_COMBO_BOX_TEXT                GTK_COMBO_BOX
# define gtk_notebook_set_group_name       gtk_notebook_set_group
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
#else
/* Does nothing */
# define gtk_dialog_set_has_separator(dialog, setting) do { } while (0)
#endif

/* GtkSourceView3 compatibility */
#if ! GTK_CHECK_VERSION(3, 0, 0)
# define GTK_SOURCE_TYPE_LANGUAGE     GTK_TYPE_SOURCE_LANGUAGE
# define GTK_SOURCE_IS_LANGUAGE       GTK_IS_SOURCE_LANGUAGE
# define GTK_SOURCE_TYPE_STYLE_SCHEME GTK_TYPE_SOURCE_STYLE_SCHEME
# define GTK_SOURCE_IS_STYLE_SCHEME   GTK_IS_SOURCE_STYLE_SCHEME
# define GTK_SOURCE_IS_BUFFER         GTK_IS_SOURCE_BUFFER
# define GTK_SOURCE_TYPE_VIEW         GTK_TYPE_SOURCE_VIEW
#endif

G_END_DECLS

#endif /* __MOUSEPAD_GTK_COMPAT_H__ */
