/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  Copyright (C) 2012 Jonathan Matthew <jonathan@d14n.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  The Rhythmbox authors hereby grant permission for non-GPL compatible
 *  GStreamer plugins to be used and distributed together with GStreamer
 *  and Rhythmbox. This permission is above and beyond the permissions granted
 *  by the GPL license by which Rhythmbox is covered. If you modify this code
 *  you may extend this exception to your version of the code, but you are not
 *  obligated to do so. If you do not wish to do so, delete this exception
 *  statement from your version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.
 *
 */

#include <config.h>

#include <glib/gi18n.h>

#include <libpeas/peas.h>
#include <libpeas-gtk/peas-gtk.h>

#include <shell/rb-application.h>
#include <shell/rb-shell.h>
#include <lib/rb-debug.h>
#include <lib/rb-file-helpers.h>
#include <lib/rb-builder-helpers.h>
#include <lib/rb-stock-icons.h>
#include <widgets/rb-dialog.h>

/**
 * SECTION:rb-application
 * @short_description: the rhythmbox subclass of GtkApplication
 *
 * RBApplication contains some interactions with the desktop
 * environment, such as the app menu and processing of files specified
 * on the command line.
 */

static void rb_application_class_init (RBApplicationClass *klass);
static void rb_application_init (RBApplication *app);

struct _RBApplicationPrivate
{
	RBShell *shell;

	GtkWidget *plugins;

	GHashTable *shared_menus;
	GHashTable *plugin_menus;

	gboolean autostarted;
	gboolean no_update;
	gboolean no_registration;
	gboolean dry_run;
	gboolean disable_plugins;
	char *rhythmdb_file;
	char *playlists_file;
};

G_DEFINE_TYPE (RBApplication, rb_application, GTK_TYPE_APPLICATION);

enum {
	PROP_0,
	PROP_SHELL
};

static void
load_uri_action_cb (GSimpleAction *action, GVariant *parameters, gpointer user_data)
{
	RBApplication *rb = RB_APPLICATION (user_data);
	const char *uri;
	gboolean play;

	g_variant_get (parameters, "(&sb)", &uri, &play);

	rb_shell_load_uri (RB_SHELL (rb->priv->shell), uri, play, NULL);
}

static void
activate_source_action_cb (GSimpleAction *action, GVariant *parameters, gpointer user_data)
{
	RBApplication *rb = RB_APPLICATION (user_data);
	const char *source;
	guint play;

	g_variant_get (parameters, "(&su)", &source, &play);
	rb_shell_activate_source_by_uri (RB_SHELL (rb->priv->shell), source, play, NULL);
}

static void
quit_action_cb (GSimpleAction *action, GVariant *parameters, gpointer user_data)
{
	RBApplication *rb = RB_APPLICATION (user_data);
	rb_shell_quit (RB_SHELL (rb->priv->shell), NULL);
}

static gboolean
plugins_window_delete_cb (GtkWidget *window,
			  GdkEventAny *event,
			  gpointer data)
{
	gtk_widget_hide (window);
	return TRUE;
}

static void
plugins_response_cb (GtkDialog *dialog,
		     int response_id,
		     gpointer data)
{
	if (response_id == GTK_RESPONSE_CLOSE)
		gtk_widget_hide (GTK_WIDGET (dialog));
}

static void
plugins_action_cb (GSimpleAction *action, GVariant *parameters, gpointer user_data)
{
	RBApplication *app = RB_APPLICATION (user_data);

	if (app->priv->plugins == NULL) {
		GtkWidget *content_area;
		GtkWidget *manager;
		GtkWindow *window;

		g_object_get (app->priv->shell, "window", &window, NULL);

		app->priv->plugins = gtk_dialog_new_with_buttons (_("Configure Plugins"),
								  window,
								  GTK_DIALOG_DESTROY_WITH_PARENT,
								  GTK_STOCK_CLOSE,
								  GTK_RESPONSE_CLOSE,
								  NULL);
		content_area = gtk_dialog_get_content_area (GTK_DIALOG (app->priv->plugins));
		gtk_container_set_border_width (GTK_CONTAINER (app->priv->plugins), 5);
		gtk_box_set_spacing (GTK_BOX (content_area), 2);

		g_signal_connect_object (G_OBJECT (app->priv->plugins),
					 "delete_event",
					 G_CALLBACK (plugins_window_delete_cb),
					 NULL, 0);
		g_signal_connect_object (G_OBJECT (app->priv->plugins),
					 "response",
					 G_CALLBACK (plugins_response_cb),
					 NULL, 0);

		manager = peas_gtk_plugin_manager_new (NULL);
		gtk_widget_show_all (GTK_WIDGET (manager));
		gtk_box_pack_start (GTK_BOX (content_area), manager, TRUE, TRUE, 0);
		gtk_window_set_default_size (GTK_WINDOW (app->priv->plugins), 600, 400);

		g_object_unref (window);
	}

	gtk_window_present (GTK_WINDOW (app->priv->plugins));
}

static void
preferences_action_cb (GSimpleAction *action, GVariant *parameters, gpointer user_data)
{
	RBApplication *app = RB_APPLICATION (user_data);
	RBShellPreferences *prefs;

	g_object_get (app->priv->shell, "prefs", &prefs, NULL);

	gtk_window_present (GTK_WINDOW (prefs));
	g_object_unref (prefs);
}

static void
about_action_cb (GSimpleAction *action, GVariant *parameters, gpointer user_data)
{
	RBApplication *app = RB_APPLICATION (user_data);
	GtkWindow *window;
	const char **tem;
	GString *comment;

	const char *authors[] = {
		"",
#include "MAINTAINERS.tab"
		"",
		NULL,
#include "MAINTAINERS.old.tab"
		"",
		NULL,
#include "AUTHORS.tab"
		NULL
	};

	const char *documenters[] = {
#include "DOCUMENTERS.tab"
		NULL
	};

	const char *translator_credits = _("translator-credits");

	const char *license[] = {
		N_("Rhythmbox is free software; you can redistribute it and/or modify\n"
		   "it under the terms of the GNU General Public License as published by\n"
		   "the Free Software Foundation; either version 2 of the License, or\n"
		   "(at your option) any later version.\n"),
		N_("Rhythmbox is distributed in the hope that it will be useful,\n"
		   "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
		   "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
		   "GNU General Public License for more details.\n"),
		N_("You should have received a copy of the GNU General Public License\n"
		   "along with Rhythmbox; if not, write to the Free Software Foundation, Inc.,\n"
		   "51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA\n")
	};

	char *license_trans;

	authors[0] = _("Maintainers:");
	for (tem = authors; *tem != NULL; tem++)
		;
	*tem = _("Former Maintainers:");
	for (; *tem != NULL; tem++)
		;
	*tem = _("Contributors:");

	comment = g_string_new (_("Music management and playback software for GNOME."));

	license_trans = g_strconcat (_(license[0]), "\n", _(license[1]), "\n",
				     _(license[2]), "\n", NULL);

	g_object_get (app->priv->shell, "window", &window, NULL);
	gtk_show_about_dialog (GTK_WINDOW (window),
			       "version", VERSION,
			       "copyright", "Copyright \xc2\xa9 2005 - 2012 The Rhythmbox authors\nCopyright \xc2\xa9 2003 - 2005 Colin Walters\nCopyright \xc2\xa9 2002, 2003 Jorn Baayen",
			       "license", license_trans,
			       "website-label", _("Rhythmbox Website"),
			       "website", "http://www.gnome.org/projects/rhythmbox",
			       "comments", comment->str,
			       "authors", (const char **) authors,
			       "documenters", (const char **) documenters,
			       "translator-credits", strcmp (translator_credits, "translator-credits") != 0 ? translator_credits : NULL,
			       "logo-icon-name", "rhythmbox",
			       NULL);
	g_string_free (comment, TRUE);
	g_free (license_trans);
	g_object_unref (window);
}

static void
help_action_cb (GSimpleAction *action, GVariant *parameters, gpointer user_data)
{
	RBApplication *app = RB_APPLICATION (user_data);
	GError *error = NULL;
	GtkWindow *window;

	g_object_get (app->priv->shell, "window", &window, NULL);

	gtk_show_uri (gtk_widget_get_screen (GTK_WIDGET (window)),
		      "help:rhythmbox",
		      gtk_get_current_event_time (),
		      &error);

	if (error != NULL) {
		rb_error_dialog (NULL, _("Couldn't display help"),
				 "%s", error->message);
		g_error_free (error);
	}

	g_object_unref (window);
}

static void
impl_activate (GApplication *app)
{
	RBApplication *rb = RB_APPLICATION (app);
	rb_shell_present (rb->priv->shell, gtk_get_current_event_time (), NULL);
}

static void
impl_open (GApplication *app, GFile **files, int n_files, const char *hint)
{
	RBApplication *rb = RB_APPLICATION (app);
	int i;

	for (i = 0; i < n_files; i++) {
		char *uri;

		uri = g_file_get_uri (files[i]);

		/*
		 * rb_uri_exists won't work if the location isn't mounted.
		 * however, things that are interesting to mount are generally
		 * non-local, so we'll process them anyway.
		 */
		if (rb_uri_is_local (uri) == FALSE || rb_uri_exists (uri)) {
			rb_shell_load_uri (rb->priv->shell, uri, TRUE, NULL);
		}
		g_free (uri);
	}
}

static void
load_state_changed_cb (GActionGroup *action_group, const char *action_name, GVariant *state, GPtrArray *files)
{
	gboolean loaded;
	gboolean scanned;

	if (g_strcmp0 (action_name, "load-uri") != 0) {
		return;
	}

	g_variant_get (state, "(bb)", &loaded, &scanned);
	if (loaded) {
		rb_debug ("opening files now");
		g_signal_handlers_disconnect_by_func (action_group, load_state_changed_cb, files);

		g_application_open (G_APPLICATION (action_group), (GFile **)files->pdata, files->len, "");
		g_ptr_array_free (files, TRUE);
	}
}

static void
impl_startup (GApplication *app)
{
	RBApplication *rb = RB_APPLICATION (app);
	gboolean shell_shows_app_menu;
	GtkBuilder *builder;
	GMenuModel *menu;
	GtkCssProvider *provider;

	GActionEntry app_actions[] = {

		/* rhythmbox-client actions */
		{ "load-uri", load_uri_action_cb, "(sb)", "(false, false)" },
		{ "activate-source", activate_source_action_cb, "(su)" },

		/* app menu actions */
		{ "plugins", plugins_action_cb },
		{ "preferences", preferences_action_cb },
		{ "help", help_action_cb },
		{ "about", about_action_cb },
		{ "quit", quit_action_cb },
	};

	(* G_APPLICATION_CLASS (rb_application_parent_class)->startup) (app);
	
	rb_stock_icons_init ();

	g_action_map_add_action_entries (G_ACTION_MAP (app),
					 app_actions,
					 G_N_ELEMENTS (app_actions),
					 app);

	g_object_get (gtk_settings_get_default (),
		      "gtk-shell-shows-app-menu", &shell_shows_app_menu,
		      NULL);

	builder = rb_builder_load ("app-menu.ui", NULL);
	menu = G_MENU_MODEL (gtk_builder_get_object (builder, "app-menu"));
	rb_application_link_shared_menus (rb, G_MENU (menu));
	rb_application_add_shared_menu (rb, "app-menu", menu);

	/* only set the app menu if the shell shows it; otherwise, we'll
	 * stick a menu button in the toolbar.
	 */
	if (shell_shows_app_menu) {
		gtk_application_set_app_menu (GTK_APPLICATION (app), menu);
	}
	
	g_object_unref (builder);

	/* Use our own css provider */
	provider = gtk_css_provider_new ();
	if (gtk_css_provider_load_from_path (provider, rb_file ("style.css"), NULL)) {
		gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
		                                          GTK_STYLE_PROVIDER (provider),
		                                          600);
	}

	rb->priv->shell = RB_SHELL (g_object_new (RB_TYPE_SHELL,
				    "application", rb,
				    "autostarted", rb->priv->autostarted,
				    "no-registration", rb->priv->no_registration,
				    "no-update", rb->priv->no_update,
				    "dry-run", rb->priv->dry_run,
				    "rhythmdb-file", rb->priv->rhythmdb_file,
				    "playlists-file", rb->priv->playlists_file,
				    "disable-plugins", rb->priv->disable_plugins,
				    NULL));
}


static gboolean
impl_local_command_line (GApplication *app, gchar ***args, int *exit_status)
{
	RBApplication *rb = RB_APPLICATION (app);
	GError *error = NULL;
	gboolean scanned;
	gboolean loaded;
	GPtrArray *files;
	int n_files;
	int i;

	n_files = g_strv_length (*args) - 1;

	if (rb->priv->no_registration) {
		if (n_files > 0) {
			g_warning ("Unable to open files on the commandline with --no-registration");
		}
		impl_startup (app);
		return TRUE;
	}

	if (!g_application_register (app, NULL, &error)) {
		g_critical ("%s", error->message);
		g_error_free (error);
		*exit_status = 1;
		return TRUE;
	}

	if (n_files <= 0) {
		g_application_activate (app);
		*exit_status = 0;
		return TRUE;
	}

	files = g_ptr_array_new_with_free_func (g_object_unref);
	for (i = 0; i < n_files; i++) {
		g_ptr_array_add (files, g_file_new_for_commandline_arg ((*args)[i + 1]));
	}

	g_variant_get (g_action_group_get_action_state (G_ACTION_GROUP (app), "load-uri"), "(bb)", &loaded, &scanned);
	if (loaded) {
		rb_debug ("opening files immediately");
		g_application_open (app, (GFile **)files->pdata, files->len, "");
		g_ptr_array_free (files, TRUE);
	} else {
		rb_debug ("opening files once db is loaded");
		g_signal_connect (app, "action-state-changed::load-uri", G_CALLBACK (load_state_changed_cb), files);
	}

	return TRUE;
}

static void
impl_shutdown (GApplication *app)
{
	RBApplication *rb = RB_APPLICATION (app);

	if (rb->priv->shell != NULL) {
		rb_shell_quit (rb->priv->shell, NULL);
		g_object_unref (rb->priv->shell);
		rb->priv->shell = NULL;
	}

	(* G_APPLICATION_CLASS (rb_application_parent_class)->shutdown) (app);
}


static void
impl_finalize (GObject *object)
{
	RBApplication *app = RB_APPLICATION (object);
	
	g_hash_table_destroy (app->priv->shared_menus);
	g_hash_table_destroy (app->priv->plugin_menus);
	rb_file_helpers_shutdown ();
	rb_stock_icons_shutdown ();
	rb_refstring_system_shutdown ();

	G_OBJECT_CLASS (rb_application_parent_class)->finalize (object);
}

static void
impl_dispose (GObject *object)
{
	RBApplication *app = RB_APPLICATION (object);

	g_clear_object (&app->priv->shell);

	G_OBJECT_CLASS (rb_application_parent_class)->dispose (object);
}

static void
impl_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	/* RBApplication *app = RB_APPLICATION (object); */
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
	}
}

static void
impl_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	RBApplication *app = RB_APPLICATION (object);
	switch (prop_id) {
	case PROP_SHELL:
		g_value_set_object (value, app->priv->shell);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
	}
}

static void
rb_application_init (RBApplication *app)
{
	app->priv = G_TYPE_INSTANCE_GET_PRIVATE (app,
						 RB_TYPE_APPLICATION,
						 RBApplicationPrivate);
	rb_user_data_dir ();
	rb_refstring_system_init ();

#ifdef USE_UNINSTALLED_DIRS
	rb_file_helpers_init (TRUE);
#else
	rb_file_helpers_init (FALSE);
#endif

	app->priv->shared_menus = g_hash_table_new_full (g_str_hash,
							 g_str_equal,
							 (GDestroyNotify) g_free,
							 (GDestroyNotify) g_object_unref);
	app->priv->plugin_menus = g_hash_table_new_full (g_str_hash,
							 g_str_equal,
							 (GDestroyNotify) g_free,
							 (GDestroyNotify) g_object_unref);

	g_setenv ("PULSE_PROP_media.role", "music", TRUE);
}

static void
rb_application_class_init (RBApplicationClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GApplicationClass *app_class = G_APPLICATION_CLASS (klass);

	object_class->finalize = impl_finalize;
	object_class->dispose = impl_dispose;
	object_class->set_property = impl_set_property;
	object_class->get_property = impl_get_property;

	app_class->open = impl_open;
	app_class->activate = impl_activate;
	app_class->local_command_line = impl_local_command_line;
	app_class->startup = impl_startup;
	app_class->shutdown = impl_shutdown;

	g_object_class_install_property (object_class,
					 PROP_SHELL,
					 g_param_spec_object ("shell",
							      "shell",
							      "RBShell instance",
							      RB_TYPE_SHELL,
							      G_PARAM_READABLE));


	g_type_class_add_private (klass, sizeof (RBApplicationPrivate));
}

/**
 * rb_application_new:
 *
 * Creates the application instance.
 *
 * Return value: application instance
 */
GApplication *
rb_application_new (void)
{
	return G_APPLICATION (g_object_new (RB_TYPE_APPLICATION,
					    "application-id", "org.gnome.Rhythmbox3",
					    "flags", G_APPLICATION_HANDLES_OPEN,
					    NULL));
}

/**
 * rb_application_run:
 * @rb: the application instance
 * @argc: arg count
 * @argv: arg values
 *
 * Runs the application
 *
 * Return value: exit code
 */
int
rb_application_run (RBApplication *rb, int argc, char **argv)
{
	GOptionContext *context;
	gboolean debug = FALSE;
	char *debug_match = NULL;
	int nargc;
	char **nargv;

	GError *error = NULL;

	g_application_set_default (G_APPLICATION (rb));
	rb->priv->autostarted = (g_getenv ("DESKTOP_AUTOSTART_ID") != NULL);

	const GOptionEntry options []  = {
		{ "debug",           'd', 0, G_OPTION_ARG_NONE,         &debug,           N_("Enable debug output"), NULL },
		{ "debug-match",     'D', 0, G_OPTION_ARG_STRING,       &debug_match,     N_("Enable debug output matching a specified string"), NULL },
		{ "no-update",	       0, 0, G_OPTION_ARG_NONE,         &rb->priv->no_update, N_("Do not update the library with file changes"), NULL },
		{ "no-registration", 'n', 0, G_OPTION_ARG_NONE,         &rb->priv->no_registration, N_("Do not register the shell"), NULL },
		{ "dry-run",	       0, 0, G_OPTION_ARG_NONE,         &rb->priv->dry_run,         N_("Don't save any data permanently (implies --no-registration)"), NULL },
		{ "disable-plugins",   0, 0, G_OPTION_ARG_NONE,		&rb->priv->disable_plugins, N_("Disable loading of plugins"), NULL },
		{ "rhythmdb-file",     0, 0, G_OPTION_ARG_STRING,       &rb->priv->rhythmdb_file,   N_("Path for database file to use"), NULL },
		{ "playlists-file",    0, 0, G_OPTION_ARG_STRING,       &rb->priv->playlists_file,   N_("Path for playlists file to use"), NULL },
		{ NULL }
	};

	context = g_option_context_new (NULL);
	g_option_context_add_main_entries (context, options, GETTEXT_PACKAGE);
	g_option_context_add_group (context, gst_init_get_option_group ());
	g_option_context_add_group (context, gtk_get_option_group (TRUE));

	nargc = argc;
	nargv = argv;
	if (g_option_context_parse (context, &nargc, &nargv, &error) == FALSE) {
		g_print (_("%s\nRun '%s --help' to see a full list of available command line options.\n"),
			 error->message, argv[0]);
		g_error_free (error);
		g_option_context_free (context);
		return 1;
	}
	g_option_context_free (context);

	if (!debug && debug_match)
		rb_debug_init_match (debug_match);
	else
		rb_debug_init (debug);

	g_object_set (rb, "register-session", !rb->priv->no_registration, NULL);

	return g_application_run (G_APPLICATION (rb), nargc, nargv);
}

/**
 * rb_application_add_shared_menu:
 * @app: the application instance
 * @name: a name for the menu
 * @menu: #GMenuModel instance
 *
 * Adds a menu model to the set of shared menus
 * available for linking into other menus.
 */
void
rb_application_add_shared_menu (RBApplication *app, const char *name, GMenuModel *menu)
{
	g_assert (menu != NULL);
	g_hash_table_insert (app->priv->shared_menus, g_strdup (name), g_object_ref (menu));
}

/**
 * rb_application_get_shared_menu:
 * @app: the application instance
 * @name: name of menu to return
 *
 * Returns a shared menu instance added with @rb_application_add_shared_menu
 *
 * Return value: (transfer none): menu model instance, or NULL if not found
 */
GMenuModel *
rb_application_get_shared_menu (RBApplication *app, const char *name)
{
	return g_hash_table_lookup (app->priv->shared_menus, name);
}

/**
 * rb_application_get_plugin_menu:
 * @app: the application instance
 * @name: name of plugin menu to return
 *
 * Returns a plugin menu instance.  Plugin menus are like shared menus except
 * they are created empty on first access, and they consist solely of entries
 * added through @rb_application_add_plugin_item.
 *
 * Return value: (transfer none): plugin menu instance.
 */
GMenuModel *
rb_application_get_plugin_menu (RBApplication *app, const char *name)
{
	GMenuModel *menu;

	menu = g_hash_table_lookup (app->priv->plugin_menus, name);
	if (menu == NULL) {
		menu = G_MENU_MODEL (g_menu_new ());
		g_object_ref_sink (menu);
		g_hash_table_insert (app->priv->plugin_menus, g_strdup (name), menu);
	}

	return menu;
}

/**
 * rb_application_add_plugin_menu_item:
 * @app: the application instance
 * @menu: name of the menu to add to
 * @id: id of the item to add (used to remove it, must be unique within the menu)
 * @item: menu item to add
 *
 * Adds an item to a plugin menu.  The id can be used to remove the item.
 */
void
rb_application_add_plugin_menu_item (RBApplication *app, const char *menu, const char *id, GMenuItem *item)
{
	GMenuModel *pmenu;

	pmenu = rb_application_get_plugin_menu (app, menu);
	g_assert (pmenu != NULL);

	g_menu_item_set_attribute (item, "rb-plugin-item-id", "s", id);
	g_menu_append_item (G_MENU (pmenu), item);
}

/**
 * rb_application_remove_plugin_item:
 * @app: the application instance
 * @menu: plugin menu to remove the item from
 * @id: id of the item to remove
 *
 * Removes an item from a plugin menu.
 */
void
rb_application_remove_plugin_menu_item (RBApplication *app, const char *menu, const char *id)
{
	GMenuModel *pmenu;
	int i;

	pmenu = rb_application_get_plugin_menu (app, menu);
	g_assert (pmenu != NULL);

	for (i = 0; i < g_menu_model_get_n_items (pmenu); i++) {
		char *item_id;

		item_id = NULL;
		g_menu_model_get_item_attribute (pmenu, i, "rb-plugin-item-id", "s", &item_id);
		if (g_strcmp0 (item_id, id) == 0) {
			g_menu_remove (G_MENU (pmenu), i);
			g_free (item_id);
			return;
		}
		g_free (item_id);
	}
}



/**
 * rb_application_link_shared_menus:
 * @app: the #RBApplication
 * @menu: a #GMenu to process
 *
 * Processes shared menu links in the given menu.  Menu links take the
 * form of items with "rb-menu-link" or "rb-plugin-menu-link" and "rb-menu-link-type" attributes.
 * "rb-menu-link" specifies the name of a shared menu to link in,
 * "rb-plugin-menu-link" specifies the name of a plugin menu to link in,
 * "rb-menu-link-type" specifies the link type, either "section" or
 * "submenu".  A link item must have "rb-menu-link-type" and one of
 * "rb-menu-link" or "rb-plugin-menu-link".
 */
void
rb_application_link_shared_menus (RBApplication *app, GMenu *menu)
{
	int i;

	for (i = 0; i < g_menu_model_get_n_items (G_MENU_MODEL (menu)); i++) {
		GMenuModel *symlink_menu;
		GMenuLinkIter *iter;
		GMenuModel *link;
		const char *name;
		const char *symlink;

		symlink_menu = NULL;
		symlink = NULL;
		g_menu_model_get_item_attribute (G_MENU_MODEL (menu), i, "rb-menu-link", "s", &symlink);
		if (symlink != NULL) {
			symlink_menu = rb_application_get_shared_menu (app, symlink);
			if (symlink_menu == NULL) {
				g_warning ("can't find target menu for link %s", symlink);
				continue;
			}
		} else {
			g_menu_model_get_item_attribute (G_MENU_MODEL (menu), i, "rb-plugin-menu-link", "s", &symlink);
			if (symlink != NULL) {
				symlink_menu = rb_application_get_plugin_menu (app, symlink);
			}
		}

		iter = g_menu_model_iterate_item_links (G_MENU_MODEL (menu), i);

		if (symlink_menu != NULL) {
			GMenuAttributeIter *attrs;
			const char *attr;
			GVariant *value;
			GMenuItem *item;

			if (g_menu_link_iter_get_next (iter, &name, &link)) {
				/* replace the existing item, since we can't modify it */
				item = g_menu_item_new (NULL, NULL);
				attrs = g_menu_model_iterate_item_attributes (G_MENU_MODEL (menu), i);
				while (g_menu_attribute_iter_get_next (attrs, &attr, &value)) {
					g_menu_item_set_attribute_value (item, attr, value);
					g_variant_unref (value);
				}

				g_menu_item_set_link (item, name, symlink_menu);

				g_menu_remove (menu, i);
				g_menu_insert_item (menu, i, item);

				g_object_unref (link);
			}
		} else {
			/* recurse into submenus and sections */
			while (g_menu_link_iter_get_next (iter, &name, &link)) {
				if (G_IS_MENU (link)) {
					rb_application_link_shared_menus (app, G_MENU (link));
				}
				g_object_unref (link);
			}
		}
		g_object_unref (iter);
	}
}

static void
set_accelerator (RBApplication *app, GMenuModel *model, int item, gboolean enable)
{
	GMenuAttributeIter *iter;
	GVariant *value;
	GVariant *target = NULL;
	const char *key;
	const char *accel = NULL;
	const char *action = NULL;

	iter = g_menu_model_iterate_item_attributes (model, item);
	while (g_menu_attribute_iter_get_next (iter, &key, &value)) {
		if (g_str_equal (key, "action") && g_variant_is_of_type (value, G_VARIANT_TYPE_STRING))
			action = g_variant_get_string (value, NULL);
		else if (g_str_equal (key, "accel") && g_variant_is_of_type (value, G_VARIANT_TYPE_STRING))
			accel = g_variant_get_string (value, NULL);
		else if (g_str_equal (key, "target"))
			target = g_variant_ref (value);

		g_variant_unref (value);
	}
	g_object_unref (iter);

	if (accel && action) {
		if (enable)
			gtk_application_add_accelerator (GTK_APPLICATION (app), accel, action, target);
		else
			gtk_application_remove_accelerator (GTK_APPLICATION (app), action, target);
	}

	if (target)
		g_variant_unref (target);
}

/**
 * rb_application_set_menu_accelerators:
 * @app: the #RBApplication
 * @menu: a #GMenuModel for which to enable or disable accelerators
 * @enable: %TRUE to enable accelerators, %FALSE to disable
 *
 * Enables or disables accelerators for items in @menu.
 */
void
rb_application_set_menu_accelerators (RBApplication *app, GMenuModel *menu, gboolean enable)
{
	int i;

	for (i = 0; i < g_menu_model_get_n_items (menu); i++) {
		GMenuLinkIter *iter;
		GMenuModel *more;
		const char *key;

		set_accelerator (app, menu, i, enable);

		iter = g_menu_model_iterate_item_links (menu, i);
		while (g_menu_link_iter_get_next (iter, &key, &more)) {
			rb_application_set_menu_accelerators (app, more, enable);
			g_object_unref (more);
		}
		g_object_unref (iter);
	}
}
