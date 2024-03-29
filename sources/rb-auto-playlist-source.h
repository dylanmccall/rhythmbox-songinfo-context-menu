/*
 *  Copyright (C) 2002 Jorn Baayen <jorn@nl.linux.org>
 *  Copyright (C) 2003,2004 Colin Walters <walters@redhat.com>
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

/*
 * Header for auto playlist class
 */

#ifndef __RB_AUTO_PLAYLIST_SOURCE_H
#define __RB_AUTO_PLAYLIST_SOURCE_H

#include <sources/rb-playlist-source.h>

G_BEGIN_DECLS

#define RB_TYPE_AUTO_PLAYLIST_SOURCE         (rb_auto_playlist_source_get_type ())
#define RB_AUTO_PLAYLIST_SOURCE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), RB_TYPE_AUTO_PLAYLIST_SOURCE, RBAutoPlaylistSource))
#define RB_AUTO_PLAYLIST_SOURCE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), RB_TYPE_AUTO_PLAYLIST_SOURCE, RBAutoPlaylistSourceClass))
#define RB_IS_AUTO_PLAYLIST_SOURCE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), RB_TYPE_AUTO_PLAYLIST_SOURCE))
#define RB_IS_AUTO_PLAYLIST_SOURCE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), RB_TYPE_AUTO_PLAYLIST_SOURCE))
#define RB_AUTO_PLAYLIST_SOURCE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), RB_TYPE_AUTO_PLAYLIST_SOURCE, RBAutoPlaylistSourceClass))

typedef struct _RBAutoPlaylistSource RBAutoPlaylistSource;
typedef struct _RBAutoPlaylistSourceClass RBAutoPlaylistSourceClass;

struct _RBAutoPlaylistSource
{
	RBPlaylistSource parent;
};

struct _RBAutoPlaylistSourceClass
{
	RBPlaylistSourceClass parent;
};

GType		rb_auto_playlist_source_get_type 	(void);

RBSource *	rb_auto_playlist_source_new		(RBShell *shell,
							 const char *name,
							 gboolean local);

RBSource *	rb_auto_playlist_source_new_from_xml	(RBShell *shell,
							 xmlNodePtr node);

void		rb_auto_playlist_source_set_query	(RBAutoPlaylistSource *source,
							 GPtrArray *query,
							 RhythmDBQueryModelLimitType limit_type,
							 GArray *limit_value,
							 const char *sort_key,
						 	 gint sort_order);

void		rb_auto_playlist_source_get_query	(RBAutoPlaylistSource *source,
							 GPtrArray **query,
							 RhythmDBQueryModelLimitType *limit_type,
							 GArray **limit_value,
							 char **sort_key,
							 gint *sort_order);

G_END_DECLS

#endif /* __RB_AUTO_PLAYLIST_SOURCE_H */
