/*
 * sdvt.c
 * Copyright (C) 2013 Dmitry Medvedev <barkdarker@gmail.com>
 *
 * Distributed under terms of the GPL2 license. TODO
 */

#include <glib.h>
#include <glib/gprintf.h>
#include <gtk/gtk.h>
#include <vte/vte.h>
#include <stdlib.h>
#include <sys/types.h>
#include <pwd.h>

#define CHECK_FLAGS(a, f) (((a) & (f)) == (f))


static const gchar   *opt_workdir = ".";
static const gchar   *opt_command = NULL;
static const gchar   *opt_title   = "sdvt";
static const gchar   *opt_font    = "monospace 10";
static       gboolean opt_bold    = FALSE;
static       gint     opt_scroll  = 1024;
static       gboolean opt_one_screen  = FALSE;
static       gboolean opt_show_version = FALSE;

static       gboolean opt_bg_transparent;
static       gdouble  opt_bg_saturation = 1.0;
static       gchar   *opt_bg_image = NULL;

static       gboolean opt_scroll_on_keystroke = TRUE;
static       gboolean opt_scroll_on_output = FALSE;

static       gboolean opt_audible_bell = FALSE;
static       gboolean opt_visible_bell = FALSE;

static       gchar   *opt_browser_command = NULL;

static       gboolean opt_restart_if_exit = FALSE;

static const gchar   *VERSION   = "0.666";

static const GOptionEntry option_entries[] =
{
    { "command",    'e', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING, &opt_command,      "Execute the argument to this option inside the terminal, instead of the user shell", "COMMAND", },
    { "workdir",    'w', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING, &opt_workdir,      "Set working directory before running the command/shell (or any other command)", "PATH", },
    { "font",       'f', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING, &opt_font,         "Font used by the terminal, in FontConfig syntax (the default is \"monospace 10\")", "FONT", },
    { "scrollback", 's', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_INT,    &opt_scroll,       "Number of scrollback lines (the default is 1024)", "NUMBER" },
    { "bold",       'b', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,   &opt_bold,         "Allow usage of bold font variants", NULL, },
    { "one-screen", 'o', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,   &opt_one_screen,   "One screen", NULL, },

    { "bg-transparent",   '\0', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,     &opt_bg_transparent, "Background transparent", NULL, },
    { "bg-saturation",    '\0', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_DOUBLE,   &opt_bg_saturation,  "Background saturation", "DOUBLE", },
    { "bg-image",         '\0', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_FILENAME, &opt_bg_image,       "Background file image", "FILENAME", },

    { "scroll-keystroke", '\0', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,   &opt_scroll_on_keystroke,  "Scroll on keystroke", NULL, },
    { "scroll-output",    '\0', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,   &opt_scroll_on_output,     "Scroll on output", NULL, },
    { "audible-bell",     '\0', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,   &opt_audible_bell,         "Audible bell", NULL, },
    { "visible-bell",     '\0', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,   &opt_visible_bell,         "Visible bell", NULL, },

    { "browser",          '\0', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING, &opt_browser_command,      "Browser command", "COMMAND", },

    { "restart-if-exit",  '\0', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,   &opt_restart_if_exit,      "Restart child if exit (useful when running a command shell)", NULL, },

    { "version",    'v', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,   &opt_show_version, "Print version information and exit", NULL, },
    { NULL },
};


/*
 * Set of colors as used by GNOME-Terminal for the “Linux” color scheme:
 * http://git.gnome.org/browse/gnome-terminal/tree/src/terminal-profile.c
 */
static const GdkColor color_palette[] =
{
    { 0, 0x0000, 0x0000, 0x0000 },
    { 0, 0xaaaa, 0x0000, 0x0000 },
    { 0, 0x0000, 0xaaaa, 0x0000 },
    { 0, 0xaaaa, 0x5555, 0x0000 },
    { 0, 0x0000, 0x0000, 0xaaaa },
    { 0, 0xaaaa, 0x0000, 0xaaaa },
    { 0, 0x0000, 0xaaaa, 0xaaaa },
    { 0, 0xaaaa, 0xaaaa, 0xaaaa },
    { 0, 0x5555, 0x5555, 0x5555 },
    { 0, 0xffff, 0x5555, 0x5555 },
    { 0, 0x5555, 0xffff, 0x5555 },
    { 0, 0xffff, 0xffff, 0x5555 },
    { 0, 0x5555, 0x5555, 0xffff },
    { 0, 0xffff, 0x5555, 0xffff },
    { 0, 0x5555, 0xffff, 0xffff },
    { 0, 0xffff, 0xffff, 0xffff },
};
#define COLOR_PALETTE_LENGTH 16

/* Use light grey on black */
static const GdkColor color_bg = { 0, 0x0000, 0x0000, 0x0000 };
static const GdkColor color_fg = { 0, 0xdddd, 0xdddd, 0xdddd };

/* Regexp used to match URIs and allow clicking them */
static const gchar uri_regexp[] = "(ftp|http)s?://[-a-zA-Z0-9.?$%&/=_~#.,:;+]*";

/* Characters considered part of a word. Simplifies double-click selection */
static const gchar word_chars[] = "-A-Za-z0-9,./?%&#@_~";


static void
configure_term_widget (VteTerminal *vtterm)
{
    gint match_tag;

    g_assert (vtterm);
    g_assert (opt_font);

    vte_terminal_set_mouse_autohide      (vtterm, FALSE);

    vte_terminal_set_background_transparent(vtterm, opt_bg_transparent);
    vte_terminal_set_background_saturation (vtterm, opt_bg_saturation);
    vte_terminal_set_background_image_file (vtterm, opt_bg_image);

    vte_terminal_set_visible_bell        (vtterm, opt_visible_bell);
    vte_terminal_set_audible_bell        (vtterm, opt_audible_bell);

    vte_terminal_set_scroll_on_keystroke (vtterm, opt_scroll_on_keystroke);
    vte_terminal_set_scroll_on_output    (vtterm, opt_scroll_on_output);

    vte_terminal_set_font_from_string    (vtterm, opt_font);
    vte_terminal_set_allow_bold          (vtterm, opt_bold);
    vte_terminal_set_scrollback_lines    (vtterm, opt_scroll);
    vte_terminal_set_word_chars          (vtterm, word_chars);
    vte_terminal_set_cursor_blink_mode   (vtterm, VTE_CURSOR_BLINK_OFF);
    vte_terminal_set_cursor_shape        (vtterm, VTE_CURSOR_SHAPE_BLOCK);
    vte_terminal_set_colors              (vtterm, &color_fg, &color_bg, color_palette, COLOR_PALETTE_LENGTH);

    match_tag = vte_terminal_match_add_gregex (vtterm,
                                               g_regex_new (uri_regexp,
                                                            G_REGEX_CASELESS,
                                                            G_REGEX_MATCH_NOTEMPTY,
                                                            NULL),
                                               0);
    vte_terminal_match_set_cursor_type (vtterm, match_tag, GDK_HAND2);
}


/*
config->colour_palette = (GdkColor *) g_malloc(sizeof(GdkColor) * DEFAULT_PALETTE_SIZE);
for (i=0; i < DEFAULT_PALETTE_SIZE; i++){
  g_snprintf(addid, 3, "%d", i);
  gdk_color_parse(g_key_file_get_string(keyfile, "colour scheme", addid , NULL), &config->colour_palette[i]);
}

if (!gdk_color_parse(g_key_file_get_string( keyfile, "colour scheme", "foreground", NULL), &config->foreground)){
  gdk_color_parse(DEFAULT_FOREGROUND_COLOR, &config->foreground);
  g_warning("Using default foreground color");
}

if (!gdk_color_parse(g_key_file_get_string( keyfile, "colour scheme", "background", NULL), &config->background)){
  gdk_color_parse(DEFAULT_BACKGROUND_COLOR, &config->background);
  g_warning("Using default background color");
}
*/


static char* guess_shell(void) {
	char *shell = getenv("SHELL");
	if (!shell) {
		struct passwd *pw = getpwuid(getuid());
		shell = (pw) ? pw->pw_shell : "/bin/sh";
	}
	return g_strdup(shell); // Return a copy
}


static gboolean
handle_key_press (GtkWidget *widget, GdkEventKey *event, gpointer userdata)
{
    g_assert (widget);
    g_assert (event);
    g_assert (userdata);
    g_assert (VTE_IS_TERMINAL (userdata));

    if (event->type != GDK_KEY_PRESS) {
        return FALSE;
    }

    //TODO
    /*
    window *w = userdata;
    guint(g) = event->keyval;
    if ((event->state & (GDK_CONTROL_MASK|GDK_SHIFT_MASK)) == (GDK_CONTROL_MASK|GDK_SHIFT_MASK))
    {
        if (g == GDK_KEY_N) {
          new_window();
          return TRUE;
        }
        if (g == GDK_KEY_V) {
          vte_terminal_paste_clipboard(VTE_TERMINAL(get_current_term(w)->vte));
          return TRUE;
        }
        if (g == GDK_KEY_C) {
          vte_terminal_copy_clipboard(VTE_TERMINAL(get_current_term(w)->vte));
          return TRUE;
        }
    }
    */

    return FALSE;
}


/* setup the whacky geometry hints for gtk */
/*
static void tab_geometry_hints(term *t) {
  // I dont need to call this every time, since the char width only changes
  // once, maybe I'll make hints and border global and reuse them
  GdkGeometry hints;
  GtkBorder *border;
  gint char_width, char_height;
  gtk_widget_style_get(GTK_WIDGET(t->vte), "inner-border", &border, NULL);

  char_width = vte_terminal_get_char_width(VTE_TERMINAL(t->vte));
  char_height = vte_terminal_get_char_height(VTE_TERMINAL(t->vte));

  hints.min_width = char_width + border->left + border->right;
  hints.min_height = char_height + border->top + border->bottom;
  hints.base_width = border->left + border->right;
  hints.base_height = border->top + border->bottom;
  hints.width_inc = char_width;
  hints.height_inc = char_height;

  gtk_window_set_geometry_hints(
      GTK_WINDOW(t->w->win),
      GTK_WIDGET(t->vte),
      &hints,
      GDK_HINT_RESIZE_INC | GDK_HINT_MIN_SIZE | GDK_HINT_BASE_SIZE);
}
*/


static const gchar*
guess_browser (void)
{
    static gchar *browser = NULL;

    if (opt_browser_command)
    	browser = opt_browser_command; //TODO проверить

    if (!browser) {
        if (g_getenv ("BROWSER")) {
            browser = g_strdup (g_getenv ("BROWSER"));
        }
        else {
            browser = g_find_program_in_path ("xdg-open");
            if (!browser) {
                browser = g_find_program_in_path ("gnome-open");
                if (!browser) {
                    browser = g_find_program_in_path ("exo-open");
                    if (!browser) {
                        browser = g_find_program_in_path ("firefox");
                    }
                }
            }
        }
    }

    return browser;
}


static gboolean
handle_mouse_press (VteTerminal *vtterm, GdkEventButton *event, gpointer userdata)
{
    gchar *match = NULL;
    glong col, row;
    gint match_tag;

    g_assert (vtterm);
    g_assert (event);

    if (event->type != GDK_BUTTON_PRESS)
        return FALSE;

    row = (glong) (event->y) / vte_terminal_get_char_height (vtterm);
    col = (glong) (event->x) / vte_terminal_get_char_width  (vtterm);

    if ((match = vte_terminal_match_check (vtterm, col, row, &match_tag)) != NULL) {
        if (event->button == 1 && CHECK_FLAGS (event->state, GDK_CONTROL_MASK)) {
            GError *error = NULL;
            gchar *cmdline[] = {
                (gchar*) guess_browser (),
                match,
                NULL
            };

            if (!cmdline[0]) {
                g_printerr ("Could not determine browser to use.\n");
            }
            else if (!g_spawn_async (NULL,
                                     cmdline,
                                     NULL,
                                     G_SPAWN_SEARCH_PATH,
                                     NULL,
                                     NULL,
                                     NULL,
                                     &error))
            {
                g_printerr ("Could not launch browser: %s", error->message);
                g_error_free (error);
            }
        }
        g_free (match);
        return TRUE;
    }

    return FALSE;
}

/*
static void launch_url(char *url) {
  g_spawn_command_line_async(g_strconcat(config->browser_command, " ", url, NULL), NULL);
}

gboolean event_button(GtkWidget *widget, GdkEventButton *button_event) {

  int ret = 0;
  gchar *match;

  if(button_event->button == 1) {
    match = vte_terminal_match_check(VTE_TERMINAL(widget),
        button_event->x / vte_terminal_get_char_width (VTE_TERMINAL (widget)),
        button_event->y / vte_terminal_get_char_height (VTE_TERMINAL (widget)),
        &ret);
    if (match) {
      launch_url(match);
      return TRUE;
    }
  }
  return FALSE;
}
*/

//----------------------------------------------------------------------------

static void terminal_fork(VteTerminal *vtterm)
{
	GError *gerror = NULL;

    gchar **command = NULL;
    gint cmdlen = 0;

    if (!opt_command)
        opt_command = guess_shell ();
    if (!g_shell_parse_argv (opt_command, &cmdlen, &command, &gerror)) {
        g_printerr ("could not parse command: %s\n", gerror->message);
        g_error_free (gerror);
        exit (EXIT_FAILURE);
    }

    g_assert (opt_workdir);

    if (!vte_terminal_fork_command_full (vtterm,
                                         VTE_PTY_DEFAULT,
                                         opt_workdir,
                                         command,
                                         NULL,
                                         G_SPAWN_SEARCH_PATH,
                                         NULL,
                                         NULL,
                                         NULL,
                                         &gerror))
    {
        g_printerr ("could not spawn shell: %s\n", gerror->message);
        g_error_free (gerror);
        exit (EXIT_FAILURE);
    }
}


static void on_child_exited(VteTerminal *vtterm, gpointer userdata) {
	if (opt_restart_if_exit) {
		terminal_fork(vtterm);
	} else {
		gtk_main_quit(); // TODO hereinafter if > 1 terminal?
	}
}

//----------------------------------------------------------------------------

GtkWidget *
new_desktop_window(GdkScreen* screen, gint mon_init)
{
	GtkWidget *window = NULL;
	GtkWidget *vtterm = NULL;

    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_icon_name (GTK_WINDOW (window), "terminal");
    gtk_window_set_title (GTK_WINDOW (window), opt_title);

	// we are desktop-window
    gtk_window_set_decorated (GTK_WINDOW (window), FALSE);
    gtk_window_set_has_resize_grip (GTK_WINDOW (window), FALSE);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW (window), TRUE);
    gtk_window_set_skip_pager_hint(GTK_WINDOW (window), TRUE);

    vtterm = vte_terminal_new ();
    configure_term_widget (VTE_TERMINAL (vtterm));

    // Exit when the child process is exited, or when the window is closed.
    g_signal_connect (G_OBJECT (window), "delete-event", G_CALLBACK (gtk_main_quit), NULL);
    g_signal_connect (G_OBJECT (vtterm), "child-exited", G_CALLBACK (on_child_exited), NULL);
    // Handle keyboard shortcuts.
    g_signal_connect (G_OBJECT (window), "key-press-event", G_CALLBACK (handle_key_press), vtterm);
    // Handles clicks un URIs
    g_signal_connect (G_OBJECT (vtterm), "button-press-event", G_CALLBACK (handle_mouse_press), NULL);

    // run terminal
    terminal_fork(VTE_TERMINAL (vtterm));

    gtk_container_add (GTK_CONTAINER (window), vtterm);

    // sizes TODO listen changes?
    GdkRectangle geom;
    gdk_screen_get_monitor_geometry(screen, mon_init, &geom);
    gtk_window_set_default_size(GTK_WINDOW(window), geom.width, geom.height);
    gtk_window_move(GTK_WINDOW(window), geom.x, geom.y);
    gtk_window_set_type_hint(GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_DESKTOP);

    //gtk_widget_realize(window);
    gtk_widget_show_all(window);

    return window;
}


//static guint n_screens = 0;


void
sdvt_desktop_manager_init(gint on_screen)
{
	GdkDisplay * gdpy;
	guint n_scr, n_mon, scr, mon;

    gdpy = gdk_display_get_default();
    n_scr = gdk_display_get_n_screens(gdpy);
    g_printf("n_scr=%d\n",n_scr);
//    n_screens = 0;
//    for(i = 0; i < n_scr; i++)
//        n_screens += gdk_screen_get_n_monitors(gdk_display_get_screen(gdpy, i));
//    g_printf("n_screens=%d\n",n_screens);
//    desktops = g_new(FmDesktop*, n_screens);
    for(scr = 0; scr < n_scr; scr++)
    {
        GdkScreen* screen = gdk_display_get_screen(gdpy, scr);
        n_mon = gdk_screen_get_n_monitors(screen);
        g_printf("n_mon[%d]=%d\n",scr,n_mon);
        for(mon = 0; mon < n_mon; mon++)
        {
            gint mon_init = (on_screen < 0 || on_screen == (int)scr) ? (int)mon : (mon ? -2 : -1);
//            GtkWidget* desktop = (GtkWidget*)fm_desktop_new(screen, mon_init);
//            desktops[i++] = (FmDesktop*)desktop;
            if(mon_init < 0)
                continue;
//            gtk_widget_realize(desktop);  /* without this, setting wallpaper won't work */
//            gtk_widget_show_all(desktop);
//            gdk_window_lower(gtk_widget_get_window(desktop));
            new_desktop_window(screen, mon_init);
        }
    }

    //gtk_window_set_title(GTK_WINDOW(t->w->win), title); TODO
}


static gboolean parse_command_line_options(int argc, char* argv[]) {
	gboolean retval = TRUE;
	GOptionContext *optctx = NULL;
	GError *gerror = NULL;
	optctx = g_option_context_new("- simple desktop virtual terminal");
	g_option_context_set_help_enabled(optctx, TRUE);
	g_option_context_add_main_entries(optctx, option_entries, NULL);
	g_option_context_add_group(optctx, gtk_get_option_group(TRUE));
	if (!g_option_context_parse(optctx, &argc, &argv, &gerror)) {
		g_printerr("%s: could not parse command line: %s\n", argv[0], gerror->message);
		g_error_free(gerror);
		retval = FALSE;
	}
	g_option_context_free(optctx);
	optctx = NULL;
	return retval;
}


int main(int argc, char *argv[]) {
	if (!parse_command_line_options(argc, argv)) {
		return EXIT_FAILURE;
	}

	if (opt_show_version) {
		printf("%s \n", VERSION);
		return EXIT_SUCCESS;
	}

	gtk_init(&argc, &argv);

	sdvt_desktop_manager_init(opt_one_screen ? gdk_screen_get_number(gdk_screen_get_default()) : -1);

	gtk_main();
	return EXIT_SUCCESS;
}
