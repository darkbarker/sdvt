/*
 * sdvt.c
 * Copyright (C) 2013 Dmitry Medvedev <barkdarker@gmail.com>
 * Distributed under terms of the GPL2 license.
 */

#include <glib.h>
#include <glib/gprintf.h>
#include <gtk/gtk.h>
#include <vte/vte.h>
#include <stdlib.h>
#include <pwd.h>
#include <X11/Xlib.h>
#include <gdk/gdkx.h>


#ifndef VERSION
#define VERSION "unknown"
#endif

static const gchar   *WIN_TITLE   = "sdvt";

static const gchar   *opt_workdir = ".";
static const gchar   *opt_command = NULL;
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

static       gboolean opt_scrollbar = FALSE;

static       gchar   *opt_browser_command = NULL;

static       gboolean opt_restart_if_exit = FALSE;


static const GOptionEntry option_entries[] =
{
    { "command",    'e', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING, &opt_command,      "Execute the argument to this option inside the terminal, instead of the user shell", "COMMAND", },
    { "workdir",    'w', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING, &opt_workdir,      "Set working directory before running the command/shell (or any other command)", "PATH", },
    { "font",       'f', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING, &opt_font,         "Font used by the terminal, in FontConfig syntax (the default is \"monospace 10\")", "FONT", },
    { "scrollback", 's', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_INT,    &opt_scroll,       "Number of scrollback lines (the default is 1024)", "NUMBER" },
    { "bold",       'b', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,   &opt_bold,         "Allow usage of bold font variants", NULL, },
    { "one-screen", 'o', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,   &opt_one_screen,   "One screen", NULL, },

    { "bg-transparent",   '\0', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,     &opt_bg_transparent, "Background transparent", NULL, },
    { "bg-saturation",    '\0', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_DOUBLE,   &opt_bg_saturation,  "Background saturation (floating point value between 0.0 and 1.0)", "FLOAT", },
    { "bg-image",         '\0', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_FILENAME, &opt_bg_image,       "Background file image", "FILENAME", },

    { "scroll-keystroke", '\0', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,   &opt_scroll_on_keystroke,  "Scroll on keystroke", NULL, },
    { "scroll-output",    '\0', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,   &opt_scroll_on_output,     "Scroll on output", NULL, },
    { "audible-bell",     '\0', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,   &opt_audible_bell,         "Audible bell", NULL, },
    { "visible-bell",     '\0', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,   &opt_visible_bell,         "Visible bell", NULL, },

    { "scrollbar",        'l',  G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,   &opt_scrollbar,            "Vertical scrollbar", NULL, },

    { "browser",          '\0', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING, &opt_browser_command,      "Browser command", "COMMAND", },

    { "restart-if-exit",  '\0', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,   &opt_restart_if_exit,      "Restart child if exit (useful when running a command shell)", NULL, },

    { "version",    'v', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE,   &opt_show_version, "Print version information and exit", NULL, },
    { NULL },
};


/*
 * Set of colors as used by GNOME-Terminal for the "Linux" color scheme:
 * http://git.gnome.org/browse/gnome-terminal/tree/src/terminal-profile.c (deleted at 2012-05-03)
 */
static const GdkColor COLOR_PALETTE[] =
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

/* light grey on black */
static const GdkColor COLOR_BG = { 0, 0x0000, 0x0000, 0x0000 };
static const GdkColor COLOR_FG = { 0, 0xdddd, 0xdddd, 0xdddd };

/* regexp to match URIs in terminal */
static const gchar URI_REGEXP_PATTERN[] = "(ftp|http)s?://[-a-zA-Z0-9.?$%&/=_~#.,:;+]*";

/* characters, which considered part of a word, for double-click selection */
static const gchar WORD_CHARS[] = "-A-Za-z0-9,./?%&#@_~";

/* atoms */
static Atom XA_NET_WORKAREA = 0;

/* desktop descriptor */
struct _Desktop
{
	gint num;
	GdkScreen* screen;
	gint mon_init;
	GtkWidget *window;
	GtkWidget *vtterm;
};
typedef struct _Desktop Desktop;

static Desktop *desktops = NULL;
static guint n_desktops = 0;


static void configure_term_widget (VteTerminal *vtterm) {
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
    vte_terminal_set_word_chars          (vtterm, WORD_CHARS);
    vte_terminal_set_cursor_blink_mode   (vtterm, VTE_CURSOR_BLINK_OFF);
    vte_terminal_set_cursor_shape        (vtterm, VTE_CURSOR_SHAPE_BLOCK);
    vte_terminal_set_colors              (vtterm, &COLOR_FG, &COLOR_BG, COLOR_PALETTE, COLOR_PALETTE_LENGTH);

    match_tag = vte_terminal_match_add_gregex (vtterm, g_regex_new (URI_REGEXP_PATTERN, G_REGEX_CASELESS, G_REGEX_MATCH_NOTEMPTY, NULL), 0);
    vte_terminal_match_set_cursor_type (vtterm, match_tag, GDK_HAND2);
}


static char* guess_shell(void) {
	char *shell = getenv("SHELL");
	if (!shell) {
		struct passwd *pw = getpwuid(getuid());
		shell = (pw) ? pw->pw_shell : "/bin/sh";
	}
	return g_strdup(shell); /* Return a copy */
}


static void execute_selection_callback (GtkClipboard *clipboard, const char *text, gpointer data) {
	GError *error = NULL;
	gchar *cmdline[] = { "xdg-open", (char*)text, NULL };

	if (!g_spawn_async (NULL, cmdline, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &error)) {
	    g_printerr ("Could not launch: %s\n", error->message);
	    g_error_free (error);
	    return;
	}
}


static void execute_selection (GtkWidget *terminal) {
	GdkDisplay *display;
	GtkClipboard *clipboard;

	vte_terminal_copy_primary (VTE_TERMINAL (terminal));
	display = gtk_widget_get_display (terminal);
	clipboard = gtk_clipboard_get_for_display (display, GDK_SELECTION_PRIMARY);
	gtk_clipboard_request_text (clipboard, execute_selection_callback, NULL);
}


static gboolean handle_key_press (GtkWidget *widget, GdkEventKey *event, gpointer userdata) {
	GtkWidget *vtterm = userdata;
	guint key = event->keyval;

    g_assert (widget);
    g_assert (event);
    g_assert (userdata);
    g_assert (VTE_IS_TERMINAL (userdata));

    if (event->type != GDK_KEY_PRESS) {
        return FALSE;
    }

	if ((event->state & (GDK_CONTROL_MASK|GDK_SHIFT_MASK)) == (GDK_CONTROL_MASK|GDK_SHIFT_MASK))
	{
		if (key == GDK_KEY_C) {
			vte_terminal_copy_clipboard(VTE_TERMINAL(vtterm));
			vte_terminal_copy_primary(VTE_TERMINAL (vtterm));
			return TRUE;
		}
		if (key == GDK_KEY_V) {
			vte_terminal_paste_clipboard(VTE_TERMINAL(vtterm));
			return TRUE;
		}
		if (key == GDK_KEY_X) {
			execute_selection(vtterm);
			return TRUE;
		}
	}

	return FALSE;
}


void resize_workarea(GdkScreen* screen, gint monitor, GtkWidget *window) {
	GdkRectangle warea;

	g_assert (screen);
	g_assert (window);

#if GTK_CHECK_VERSION(3, 4, 0)
	gdk_screen_get_monitor_workarea(screen, monitor, &warea);
#else
	/* there is the old way (you can see, for example, in PcManFm: desktop.c), but i do not want to support it */
	#warning need GTK 3.4 for get workarea
	gdk_screen_get_monitor_geometry(screen, monitor, &warea);
#endif
	gtk_window_set_default_size(GTK_WINDOW(window), warea.width, warea.height);
	gtk_window_move(GTK_WINDOW(window), warea.x, warea.y);
}


static void update_working_area(Desktop* self) {
	g_assert (self);
	
	g_debug("update_working_area: self->screen=%p, self->mon_init=%d, self->window=%p", (void*)self->screen, self->mon_init, (void*)self->window);
	resize_workarea(self->screen, self->mon_init, self->window);
}


static const gchar* guess_browser(void) {
	if (!opt_browser_command) {
		if (g_getenv("BROWSER")) {
			opt_browser_command = g_strdup(g_getenv("BROWSER"));
		} else {
			opt_browser_command = g_find_program_in_path("xdg-open");
			if (!opt_browser_command) {
				opt_browser_command = g_find_program_in_path("firefox");
			}
		}
	}
	return opt_browser_command;
}


static gboolean launch_url(char *url) {
    GError *error = NULL;
    gchar *cmdline[] = { (gchar*)guess_browser(), url, NULL };

    if (!cmdline[0]) {
        g_printerr ("Could not determine browser to use.\n");
        return FALSE;
    }
    if (!g_spawn_async (NULL, cmdline, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &error)) {
        g_printerr ("Could not launch browser: %s\n", error->message);
        g_error_free (error);
        return FALSE;
    }

    return TRUE;
}


static gboolean handle_mouse_press (VteTerminal *vtterm, GdkEventButton *event, gpointer userdata) {
    gchar *match = NULL;
    glong col, row;
    gint match_tag;

    g_assert (vtterm);
    g_assert (event);

    if (event->type != GDK_BUTTON_PRESS || event->button != 1 || !(((event->state) & (GDK_CONTROL_MASK)) == (GDK_CONTROL_MASK)) )
        return FALSE;

    row = (glong) (event->y) / vte_terminal_get_char_height (vtterm);
    col = (glong) (event->x) / vte_terminal_get_char_width  (vtterm);

    if ((match = vte_terminal_match_check (vtterm, col, row, &match_tag)) != NULL) {
        launch_url(match);
        g_free (match);
        return TRUE;
    }

    return FALSE;
}


static void terminal_fork(VteTerminal *vtterm) {
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

    if (!vte_terminal_fork_command_full (vtterm, VTE_PTY_DEFAULT, opt_workdir, command, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &gerror)) {
        g_printerr ("could not spawn shell: %s\n", gerror->message);
        g_error_free (gerror);
        exit (EXIT_FAILURE);
    }
}


static void on_child_exited(VteTerminal *vtterm, gpointer userdata) {
	if (opt_restart_if_exit) {
		terminal_fork(vtterm);
	}
	/* otherwise, close the application is not necessary, obviously. */
}


GdkFilterReturn on_root_event(GdkXEvent *xevent, GdkEvent *event, gpointer data) {
    XPropertyEvent * evt = ( XPropertyEvent* ) xevent;
    Desktop* self = (Desktop*)data;

    g_assert (self);

    if ( evt->type == PropertyNotify ) {
        if(evt->atom == XA_NET_WORKAREA) {
        	update_working_area(self);
        }
    }

    return GDK_FILTER_CONTINUE;
}


GtkWidget * new_desktop_window(GdkScreen* screen, gint mon_init, Desktop* desktop) {
	GtkWidget *window = NULL;
	GtkWidget *vtterm = NULL;
	GtkWidget *scrollbar, *dbox = NULL;
	GdkWindow* root;

    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_icon_name (GTK_WINDOW (window), "terminal");
    gtk_window_set_title (GTK_WINDOW (window), WIN_TITLE);

	/* we are desktop-window */
    gtk_window_set_decorated (GTK_WINDOW (window), FALSE);
    gtk_window_set_has_resize_grip (GTK_WINDOW (window), FALSE);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW (window), TRUE);
    gtk_window_set_skip_pager_hint(GTK_WINDOW (window), TRUE);

    vtterm = vte_terminal_new ();
    configure_term_widget (VTE_TERMINAL (vtterm));

    /* child process is exited or when the window is closed */
    g_signal_connect (G_OBJECT (window), "delete-event", G_CALLBACK (gtk_main_quit), NULL);
    g_signal_connect (G_OBJECT (vtterm), "child-exited", G_CALLBACK (on_child_exited), NULL);
    /* keyboard shortcuts */
    g_signal_connect (G_OBJECT (window), "key-press-event", G_CALLBACK (handle_key_press), vtterm);
    /* clicks un URIs */
    g_signal_connect (G_OBJECT (vtterm), "button-press-event", G_CALLBACK (handle_mouse_press), NULL);

    /* run terminal */
    terminal_fork(VTE_TERMINAL (vtterm));

    if(opt_scrollbar) {
    	scrollbar = gtk_scrollbar_new(GTK_ORIENTATION_VERTICAL, gtk_scrollable_get_vadjustment( GTK_SCROLLABLE(vtterm) ) );
    	dbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_box_pack_start (GTK_BOX (dbox), vtterm, TRUE, TRUE, 0);
        gtk_box_pack_start (GTK_BOX (dbox), scrollbar, FALSE, FALSE, 0);
        gtk_container_add (GTK_CONTAINER (window), dbox);
	} else {
    	gtk_container_add (GTK_CONTAINER (window), vtterm);
    }

    /* set root-window event filter (for listen changes workarea size) */
    root = gdk_screen_get_root_window(screen);
    gdk_window_set_events(root, gdk_window_get_events(root)|GDK_PROPERTY_CHANGE_MASK);
    gdk_window_add_filter(root, on_root_event, desktop);

    /* set currently sizes */
    resize_workarea(screen, mon_init, window);

    gtk_window_set_type_hint(GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_DESKTOP);
    /* gtk_window_set_geometry_hints not need */

    /*gtk_widget_realize(window); */
    gtk_widget_show_all(window);
    /*gdk_window_lower(gtk_widget_get_window(window));*/

    /* save pointer */
    desktop->window = window;
    desktop->vtterm = vtterm;

    return window;
}


void sdvt_desktop_manager_init() {
	GdkDisplay * gdpy;
	guint n_scr, n_mon, scr, mon, i;
	gint on_screen;

	on_screen = opt_one_screen ? gdk_screen_get_number(gdk_screen_get_default()) : -1;

    gdpy = gdk_display_get_default();
    n_scr = gdk_display_get_n_screens(gdpy);
    g_debug("n_scr=%d\n",n_scr);

    n_desktops = 0;
    for(i = 0; i < n_scr; i++)
    	n_desktops += gdk_screen_get_n_monitors(gdk_display_get_screen(gdpy, i));
    g_debug("n_desktops=%d\n",n_desktops);

    desktops = g_new(Desktop, n_desktops);
    for(scr = 0, i = 0; scr < n_scr; scr++)
    {
        GdkScreen* screen = gdk_display_get_screen(gdpy, scr);
        n_mon = gdk_screen_get_n_monitors(screen);
        g_debug("n_mon[scr=%d]=%d\n",scr,n_mon);
        for(mon = 0; mon < n_mon; mon++)
        {
            gint mon_init = (on_screen < 0 || on_screen == (int)scr) ? (int)mon : (mon ? -2 : -1);
            Desktop desktop = {0};
            desktop.num = i++;
            desktop.screen = screen;
            desktop.mon_init = mon_init;
            desktops[desktop.num] = desktop;
            if(mon_init < 0)
                continue;
            new_desktop_window(screen, mon_init, &(desktops[desktop.num]));
        }
    }
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


static void init_atoms() {
	char* atom_names[] = {"_NET_WORKAREA", };
	Atom atoms[G_N_ELEMENTS(atom_names)] = {0};

    if(XInternAtoms(gdk_x11_get_default_xdisplay(), atom_names, G_N_ELEMENTS(atom_names), False, atoms)) {
        XA_NET_WORKAREA = atoms[0];
    } else {
    	g_printerr("failed to get some atoms\n");
    }
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

	init_atoms();

	sdvt_desktop_manager_init();

	gtk_main();
	return EXIT_SUCCESS;
}
