#include <mousepad/mousepad-close-button.h>



struct MousepadCloseButton_
{
  GtkButton parent;
};

struct MousepadCloseButtonClass_
{
  GtkButtonClass  parent_class;
};



G_DEFINE_TYPE (MousepadCloseButton, mousepad_close_button, GTK_TYPE_BUTTON)



static void
mousepad_close_button_class_init (MousepadCloseButtonClass *klass)
{
}



static void
mousepad_close_button_init (MousepadCloseButton *button)
{
  GtkWidget *image;
  GtkCssProvider  *css_provider;
  GtkStyleContext *context;

  static const gchar *button_style =
    "* {\n"
    "  outline-width: 0;\n"
    "  outline-offset: 0;\n"
    "  padding: 0;\n"
    "}\n";

  css_provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (css_provider, button_style, -1, NULL);

  context = gtk_widget_get_style_context (GTK_WIDGET (button));
  gtk_style_context_add_provider (context,
                                  GTK_STYLE_PROVIDER (css_provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref (css_provider);

  image = gtk_image_new_from_icon_name ("window-close", GTK_ICON_SIZE_MENU);
  gtk_container_add (GTK_CONTAINER (button), image);
  gtk_widget_show (image);

  g_object_set (G_OBJECT (button),
                "relief", GTK_RELIEF_NONE,
                "focus-on-click", FALSE,
                NULL);
}



GtkWidget *
mousepad_close_button_new (void)
{
  return g_object_new (MOUSEPAD_TYPE_CLOSE_BUTTON, NULL);
}
