/*
 * panel-bindings.h: panel keybindings support module
 *
 * Copyright (C) 2003 Sun Microsystems, Inc.
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
 *      Mark McLoughlin <mark@skynet.ie>
 */

#ifndef __PANEL_BINDINGS_H__
#define __PANEL_BINDINGS_H__

#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

void panel_bindings_set_entries(GtkBindingSet *binding_set);
guint panel_bindings_get_mouse_button_modifier_keymask(void);

G_END_DECLS

#endif /* __PANEL_BINDINGS_H__ */
