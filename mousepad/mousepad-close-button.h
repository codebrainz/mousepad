#ifndef __MOUSEPAD_CLOSE_BUTTON_H__
#define __MOUSEPAD_CLOSE_BUTTON_H__ 1

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define MOUSEPAD_TYPE_CLOSE_BUTTON            (mousepad_close_button_get_type ())
#define MOUSEPAD_CLOSE_BUTTON(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOUSEPAD_TYPE_CLOSE_BUTTON, MousepadCloseButton))
#define MOUSEPAD_CLOSE_BUTTON_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), MOUSEPAD_TYPE_CLOSE_BUTTON, MousepadCloseButtonClass))
#define MOUSEPAD_IS_CLOSE_BUTTON(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MOUSEPAD_TYPE_CLOSE_BUTTON))
#define MOUSEPAD_IS_CLOSE_BUTTON_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), MOUSEPAD_TYPE_CLOSE_BUTTON))
#define MOUSEPAD_CLOSE_BUTTON_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), MOUSEPAD_TYPE_CLOSE_BUTTON, MousepadCloseButtonClass))

typedef struct MousepadCloseButton_      MousepadCloseButton;
typedef struct MousepadCloseButtonClass_ MousepadCloseButtonClass;

GType      mousepad_close_button_get_type (void);
GtkWidget *mousepad_close_button_new      (void);

G_END_DECLS

#endif /* __MOUSEPAD_CLOSE_BUTTON_H__ */
