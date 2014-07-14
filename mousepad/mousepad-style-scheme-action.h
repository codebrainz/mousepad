#ifndef MOUSEPAD_STYLESCHEME_ACTION_H_
#define MOUSEPAD_STYLESCHEME_ACTION_H_ 1

#include <gtk/gtk.h>
#include <gtksourceview/gtksourcestylescheme.h>

G_BEGIN_DECLS

#define MOUSEPAD_TYPE_STYLE_SCHEME_ACTION            (mousepad_style_scheme_action_get_type ())
#define MOUSEPAD_STYLE_SCHEME_ACTION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOUSEPAD_TYPE_STYLE_SCHEME_ACTION, MousepadStyleSchemeAction))
#define MOUSEPAD_STYLE_SCHEME_ACTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), MOUSEPAD_TYPE_STYLE_SCHEME_ACTION, MousepadStyleSchemeActionClass))
#define MOUSEPAD_IS_STYLE_SCHEME_ACTION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MOUSEPAD_TYPE_STYLE_SCHEME_ACTION))
#define MOUSEPAD_IS_STYLE_SCHEME_ACTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), MOUSEPAD_TYPE_STYLE_SCHEME_ACTION))
#define MOUSEPAD_STYLE_SCHEME_ACTION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), MOUSEPAD_TYPE_STYLE_SCHEME_ACTION, MousepadStyleSchemeActionClass))

typedef struct MousepadStyleSchemeAction_      MousepadStyleSchemeAction;
typedef struct MousepadStyleSchemeActionClass_ MousepadStyleSchemeActionClass;

GType                 mousepad_style_scheme_action_get_type         (void);

GtkAction            *mousepad_style_scheme_action_new              (GtkSourceStyleScheme      *scheme);

GtkSourceStyleScheme *mousepad_style_scheme_action_get_style_scheme (MousepadStyleSchemeAction *action);

G_END_DECLS

#endif /* MOUSEPAD_STYLESCHEME_ACTION_H_ */
