#ifndef MOUSEPAD_LANGUAGES_H_
#define MOUSEPAD_LANGUAGES_H_ 1

#include <gtksourceview/gtksourcelanguage.h>

G_BEGIN_DECLS

GtkActionGroup    *mousepad_languages_action_group_new     (void);

GtkWidget         *mousepad_languages_create_menu          (GtkActionGroup    *group);

GtkSourceLanguage *mousepad_languages_get_active           (GtkActionGroup    *group);

void               mousepad_languages_set_active           (GtkActionGroup    *group,
                                                            GtkSourceLanguage *language);

GtkAction         *mousepad_languages_get_action           (GtkActionGroup    *group,
                                                            GtkSourceLanguage *language);

GtkSourceLanguage *mousepad_languages_get_action_language  (GtkAction         *action);

GSList           *mousepad_languages_get_sorted_sections    (void);

GSList           *mousepad_languages_get_sorted_for_section (const gchar      *section);

G_END_DECLS

#endif /* MOUSEPAD_LANGUAGES_H_ */
