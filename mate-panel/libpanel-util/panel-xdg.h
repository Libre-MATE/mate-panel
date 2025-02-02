/*
 * panel-xdg.h: miscellaneous XDG-related functions.
 *
 * Copyright (C) 2010 Novell, Inc.
 * Copyright (C) 2012-2021 MATE Developers
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * Authors:
 *	Vincent Untz <vuntz@gnome.org>
 */

#ifndef PANEL_XDG_H
#define PANEL_XDG_H

#include <glib.h>
#include <gdk/gdk.h>

G_BEGIN_DECLS

char *panel_xdg_icon_remove_extension(const char *icon);

char *panel_xdg_icon_name_from_icon_path(const char *path, GdkScreen *screen);

G_END_DECLS

#endif /* PANEL_XDG_H */
