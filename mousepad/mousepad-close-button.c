#include <mousepad/mousepad-close-button.h>



struct MousepadCloseButton_
{
  GtkButton parent;
};

struct MousepadCloseButtonClass_
{
  GtkButtonClass  parent_class;
#if GTK_CHECK_VERSION(3, 0, 0)
  GtkCssProvider *css_provider;
#endif
};



G_DEFINE_TYPE (MousepadCloseButton, mousepad_close_button, GTK_TYPE_BUTTON)



#if ! GTK_CHECK_VERSION(3, 0, 0)
static void
mousepad_close_button_style_set (GtkWidget *widget,
                                 GtkStyle  *previous_style)
{
  GtkSettings *settings;
  gint         w, h;

  settings = gtk_widget_get_settings (widget);
  gtk_icon_size_lookup_for_settings (settings, GTK_ICON_SIZE_MENU, &w, &h);
  gtk_widget_set_size_request(widget, w + 2, h + 2);
}
#endif



static void
mousepad_close_button_class_init (MousepadCloseButtonClass *klass)
{
#if GTK_CHECK_VERSION(3, 0, 0)
  static const gchar *button_style =
    "* {\n"
    "  -GtkButton-default-border: 0;\n"
    "  -GtkButton-default-outside-border: 0;\n"
    "  -GtkButton-inner-border: 0;\n"
    "  -GtkWidget-focus-line-width: 0;\n"
    "  -GtkWidget-focus-padding: 0;\n"
    "  padding: 0;\n"
    "}\n";

  klass->css_provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (klass->css_provider, button_style, -1, NULL);
#else
  GtkWidgetClass *widget_class;

  gtk_rc_parse_string (
    "style \"mousepad-close-button-style\" {\n"
    "  GtkWidget::focus-padding = 0\n"
    "  GtkWidget::focus-line-width = 0\n"
    "  xthickness = 0\n"
    "  ythickness = 0\n"
    "}\n"
    "widget \"*.mousepad-close-button\" style \"mousepad-close-button-style\"\n");

  widget_class = GTK_WIDGET_CLASS (klass);
  widget_class->style_set = mousepad_close_button_style_set;
#endif
}



static void
mousepad_close_button_init (MousepadCloseButton *button)
{
  GtkWidget *image;

#if GTK_CHECK_VERSION(3, 0, 0)
  GtkStyleContext *context;

  context = gtk_widget_get_style_context (GTK_WIDGET (button));
  gtk_style_context_add_provider (context,
                                  GTK_STYLE_PROVIDER (MOUSEPAD_CLOSE_BUTTON_GET_CLASS(button)->css_provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
#else
  gtk_widget_set_name (GTK_WIDGET (button), "mousepad-close-button");
#endif

  image = gtk_image_new_from_icon_name ("gtk-close", GTK_ICON_SIZE_MENU);
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
