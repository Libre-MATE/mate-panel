/*
 * Copyright (C) 1997 - 2000 The Free Software Foundation
 * Copyright (C) 2000 Helix Code, Inc.
 * Copyright (C) 2000 Eazel, Inc.
 * Copyright (C) 2004 Red Hat Inc.
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
 */

#ifndef __MENU_H__
#define __MENU_H__

#include <gio/gio.h>
#include <glib.h>

#include "applet.h"
#include "panel-widget.h"

G_BEGIN_DECLS

void setup_menuitem(GtkWidget *menuitem, GtkIconSize icon_size,
                    GtkWidget *pixmap, const char *title);
void setup_menuitem_with_icon(GtkWidget *menuitem, GtkIconSize icon_size,
                              GIcon *gicon, const char *image_filename,
                              const char *title);

GtkWidget *create_empty_menu(void);
GtkWidget *create_applications_menu(const char *menu_file,
                                    const char *menu_path,
                                    gboolean always_show_image);
GtkWidget *create_main_menu(PanelWidget *panel);

void setup_internal_applet_drag(GtkWidget *menuitem,
                                PanelActionButtonType type);
void setup_uri_drag(GtkWidget *menuitem, const char *uri, const char *icon,
                    GdkDragAction action);

GtkWidget *panel_create_menu(void);

GtkWidget *panel_image_menu_item_new(void);

GdkPixbuf *panel_make_menu_icon(GtkIconTheme *icon_theme, const char *icon,
                                const char *fallback, int size,
                                gboolean *long_operation);

GdkScreen *menuitem_to_screen(GtkWidget *menuitem);
PanelWidget *menu_get_panel(GtkWidget *menu);
GtkWidget *add_menu_separator(GtkWidget *menu);

gboolean menu_dummy_button_press_event(GtkWidget *menuitem,
                                       GdkEventButton *event);

G_END_DECLS

#endif /* __MENU_H__ */
