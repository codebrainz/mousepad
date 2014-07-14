#ifndef MOUSEPAD_LANGUAGE_ACTION_H_
#define MOUSEPAD_LANGUAGE_ACTION_H_ 1

#include <gtk/gtk.h>
#include <gtksourceview/gtksourcelanguage.h>

G_BEGIN_DECLS

#define MOUSEPAD_TYPE_LANGUAGE_ACTION            (mousepad_language_action_get_type ())
#define MOUSEPAD_LANGUAGE_ACTION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MOUSEPAD_TYPE_LANGUAGE_ACTION, MousepadLanguageAction))
#define MOUSEPAD_LANGUAGE_ACTION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), MOUSEPAD_TYPE_LANGUAGE_ACTION, MousepadLanguageActionClass))
#define MOUSEPAD_IS_LANGUAGE_ACTION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MOUSEPAD_TYPE_LANGUAGE_ACTION))
#define MOUSEPAD_IS_LANGUAGE_ACTION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), MOUSEPAD_TYPE_LANGUAGE_ACTION))
#define MOUSEPAD_LANGUAGE_ACTION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), MOUSEPAD_TYPE_LANGUAGE_ACTION, MousepadLanguageActionClass))

typedef struct MousepadLanguageAction_      MousepadLanguageAction;
typedef struct MousepadLanguageActionClass_ MousepadLanguageActionClass;

GType              mousepad_language_action_get_type     (void);

GtkAction         *mousepad_language_action_new          (GtkSourceLanguage      *language);

GtkSourceLanguage *mousepad_language_action_get_language (MousepadLanguageAction *action);

G_END_DECLS

#endif /* MOUSEPAD_LANGUAGE_ACTION_H_ */
