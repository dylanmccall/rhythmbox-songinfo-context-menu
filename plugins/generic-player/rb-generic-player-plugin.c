/*
 * rb-generic-player-plugin.c
 *
 * Copyright (C) 2006  Jonathan Matthew
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 *  The Rhythmbox authors hereby grant permission for non-GPL compatible
 *  GStreamer plugins to be used and distributed together with GStreamer
 *  and Rhythmbox. This permission is above and beyond the permissions granted
 *  by the GPL license by which Rhythmbox is covered. If you modify this code
 *  you may extend this exception to your version of the code, but you are not
 *  obligated to do so. If you do not wish to do so, delete this exception
 *  statement from your version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.
 */

#define __EXTENSIONS__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h> /* For strlen */
#include <glib/gi18n-lib.h>
#include <gmodule.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <glib-object.h>

#include "rb-plugin-macros.h"
#include "rb-debug.h"
#include "rb-shell.h"
#include "rb-dialog.h"
#include "rb-removable-media-manager.h"
#include "rb-generic-player-source.h"
#include "rb-generic-player-playlist-source.h"
#include "rb-file-helpers.h"
#include "rb-stock-icons.h"
#include "rb-nokia770-source.h"
#include "rb-psp-source.h"
#include "rb-display-page-tree.h"


#define RB_TYPE_GENERIC_PLAYER_PLUGIN		(rb_generic_player_plugin_get_type ())
#define RB_GENERIC_PLAYER_PLUGIN(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), RB_TYPE_GENERIC_PLAYER_PLUGIN, RBGenericPlayerPlugin))
#define RB_GENERIC_PLAYER_PLUGIN_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), RB_TYPE_GENERIC_PLAYER_PLUGIN, RBGenericPlayerPluginClass))
#define RB_IS_GENERIC_PLAYER_PLUGIN(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), RB_TYPE_GENERIC_PLAYER_PLUGIN))
#define RB_IS_GENERIC_PLAYER_PLUGIN_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), RB_TYPE_GENERIC_PLAYER_PLUGIN))
#define RB_GENERIC_PLAYER_PLUGIN_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), RB_TYPE_GENERIC_PLAYER_PLUGIN, RBGenericPlayerPluginClass))

typedef struct
{
	PeasExtensionBase parent;

	GList *player_sources;
} RBGenericPlayerPlugin;

typedef struct
{
	PeasExtensionBaseClass parent_class;
} RBGenericPlayerPluginClass;


G_MODULE_EXPORT void peas_register_types (PeasObjectModule  *module);

static void rb_generic_player_plugin_init (RBGenericPlayerPlugin *plugin);

RB_DEFINE_PLUGIN(RB_TYPE_GENERIC_PLAYER_PLUGIN, RBGenericPlayerPlugin, rb_generic_player_plugin,)

static void
rb_generic_player_plugin_init (RBGenericPlayerPlugin *plugin)
{
	rb_debug ("RBGenericPlayerPlugin initialising");
}

static void
rb_generic_player_plugin_source_deleted (RBGenericPlayerSource *source, RBGenericPlayerPlugin *plugin)
{
	plugin->player_sources = g_list_remove (plugin->player_sources, source);
}

static RBSource *
create_source_cb (RBRemovableMediaManager *rmm, GMount *mount, MPIDDevice *device_info, RBGenericPlayerPlugin *plugin)
{
	RBSource *source = NULL;
	RBShell *shell;

	g_object_get (plugin, "object", &shell, NULL);

	if (rb_psp_is_mount_player (mount, device_info))
		source = rb_psp_source_new (G_OBJECT (plugin), shell, mount, device_info);
	if (source == NULL && rb_nokia770_is_mount_player (mount, device_info))
		source = rb_nokia770_source_new (G_OBJECT (plugin), shell, mount, device_info);
	if (source == NULL && rb_generic_player_is_mount_player (mount, device_info))
		source = rb_generic_player_source_new (G_OBJECT (plugin), shell, mount, device_info);

	if (source) {
		plugin->player_sources = g_list_prepend (plugin->player_sources, source);
		g_signal_connect_object (G_OBJECT (source),
					 "deleted", G_CALLBACK (rb_generic_player_plugin_source_deleted),
					 plugin, 0);
	}

	g_object_unref (shell);
	return source;
}



static void
impl_activate (PeasActivatable *plugin)
{
	RBGenericPlayerPlugin *pi = RB_GENERIC_PLAYER_PLUGIN (plugin);
	RBRemovableMediaManager *rmm;
	RBShell *shell;
	gboolean scanned;

	g_object_get (plugin, "object", &shell, NULL);
	g_object_get (shell, "removable-media-manager", &rmm, NULL);

	/* watch for new removable media.  use connect_after so
	 * plugins for more specific device types can get in first.
	 */
	g_signal_connect_after (G_OBJECT (rmm),
				"create-source-mount", G_CALLBACK (create_source_cb),
				pi);

	/* only scan if we're being loaded after the initial scan has been done */
	g_object_get (rmm, "scanned", &scanned, NULL);
	if (scanned)
		rb_removable_media_manager_scan (rmm);

	g_object_unref (rmm);
	g_object_unref (shell);
}

static void
impl_deactivate	(PeasActivatable *bplugin)
{
	RBGenericPlayerPlugin *plugin = RB_GENERIC_PLAYER_PLUGIN (bplugin);
	RBRemovableMediaManager *rmm;
	RBShell *shell;

	g_object_get (plugin, "object", &shell, NULL);
	g_object_get (shell,
		      "removable-media-manager", &rmm,
		      NULL);

	g_signal_handlers_disconnect_by_func (G_OBJECT (rmm), create_source_cb, plugin);

	g_list_foreach (plugin->player_sources, (GFunc)rb_display_page_delete_thyself, NULL);
	g_list_free (plugin->player_sources);
	plugin->player_sources = NULL;

	g_object_unref (rmm);
	g_object_unref (shell);
}

G_MODULE_EXPORT void
peas_register_types (PeasObjectModule *module)
{
	rb_generic_player_plugin_register_type (G_TYPE_MODULE (module));
	_rb_generic_player_source_register_type (G_TYPE_MODULE (module));
	_rb_generic_player_playlist_source_register_type (G_TYPE_MODULE (module));
	_rb_nokia770_source_register_type (G_TYPE_MODULE (module));
	_rb_psp_source_register_type (G_TYPE_MODULE (module));

	peas_object_module_register_extension_type (module,
						    PEAS_TYPE_ACTIVATABLE,
						    RB_TYPE_GENERIC_PLAYER_PLUGIN);
}
