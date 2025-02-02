/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2004 Sun Microsystems, Inc.
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
 *      Matt Keenan  <matt.keenan@sun.com>
 *      Mark McLoughlin  <mark@skynet.ie>
 */

#ifndef __PANEL_LOCKDOWN_H__
#define __PANEL_LOCKDOWN_H__

#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

void panel_lockdown_init(void);
void panel_lockdown_finalize(void);

gboolean panel_lockdown_get_locked_down(void);
gboolean panel_lockdown_get_disable_command_line(void);
gboolean panel_lockdown_get_disable_lock_screen(void);
gboolean panel_lockdown_get_disable_log_out(void);
gboolean panel_lockdown_get_disable_force_quit(void);

gboolean panel_lockdown_is_applet_disabled(const char *iid);

void panel_lockdown_notify_add(GCallback callback_func, gpointer user_data);
void panel_lockdown_notify_remove(GCallback callback_func, gpointer user_data);

G_END_DECLS

#endif /* __PANEL_LOCKDOWN_H__ */
