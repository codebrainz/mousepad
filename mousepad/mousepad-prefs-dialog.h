#ifndef MOUSEPADPREFSDIALOG_H_
#define MOUSEPADPREFSDIALOG_H_ 1

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define MOUSEPAD_TYPE_PREFS_DIALOG             (mousepad_prefs_dialog_get_type ())
#define MOUSEPAD_PREFS_DIALOG(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOUSEPAD_TYPE_PREFS_DIALOG, MousepadPrefsDialog))
#define MOUSEPAD_PREFS_DIALOG_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), MOUSEPAD_TYPE_PREFS_DIALOG, MousepadPrefsDialogClass))
#define MOUSEPAD_IS_PREFS_DIALOG(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MOUSEPAD_TYPE_PREFS_DIALOG))
#define MOUSEPAD_IS_PREFS_DIALOG_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), MOUSEPAD_TYPE_PREFS_DIALOG))
#define MOUSEPAD_PREFS_DIALOG_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), MOUSEPAD_TYPE_PREFS_DIALOG, MousepadPrefsDialogClass))

typedef struct MousepadPrefsDialog_         MousepadPrefsDialog;
typedef struct MousepadPrefsDialogClass_    MousepadPrefsDialogClass;

GType      mousepad_prefs_dialog_get_type (void);

GtkWidget *mousepad_prefs_dialog_new      (void);

G_END_DECLS

#endif /* MOUSEPADPREFSDIALOG_H_ */
