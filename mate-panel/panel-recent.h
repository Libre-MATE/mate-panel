/*
 * panel-recent.h
 *
 * Copyright (C) 2002 James Willcox <jwillcox@gnome.org>
 * Copyright (C) 2012-2021 MATE Developers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Authors:
 * 	James Willcox <jwillcox@gnome.org>
 */

#ifndef __PANEL_RECENT_H__
#define __PANEL_RECENT_H__

#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

void panel_recent_append_documents_menu(GtkWidget *menu,
                                        GtkRecentManager *manager,
                                        int recent_items_limit);

G_END_DECLS

#endif /* __PANEL_RECENT_H__ */
