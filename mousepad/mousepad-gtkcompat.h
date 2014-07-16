#ifndef __MOUSEPAD_GTK_COMPAT_H__
#define __MOUSEPAD_GTK_COMPAT_H__ 1

#include <gtk/gtk.h>

#if ! GTK_CHECK_VERSION(2, 24, 0)
# define gtk_combo_box_text_new_with_entry gtk_combo_box_entry_new_text
# define gtk_combo_box_text_new            gtk_combo_box_new_text
#endif

#endif /* __MOUSEPAD_GTK_COMPAT_H__ */
