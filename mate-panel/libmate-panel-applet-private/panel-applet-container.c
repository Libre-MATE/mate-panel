/*
 * panel-applet-container.c: a container for applets.
 *
 * Copyright (C) 2010 Carlos Garcia Campos <carlosgc@gnome.org>
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <string.h>

#ifdef HAVE_X11
#include <gtk/gtkx.h>
#endif

#include <panel-applets-manager.h>

#include "panel-applet-container.h"
#include "panel-marshal.h"

struct _MatePanelAppletContainerPrivate {
  GDBusProxy *applet_proxy;

  guint name_watcher_id;
  gchar *bus_name;

  gchar *iid;
  gboolean out_of_process;
  guint32 xid;
  guint32 uid;
  GtkWidget *socket;

  GHashTable *pending_ops;
};

enum {
  APPLET_BROKEN,
  APPLET_MOVE,
  APPLET_REMOVE,
  APPLET_LOCK,
  CHILD_PROPERTY_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};

typedef struct {
  const gchar *name;
  const gchar *dbus_name;
} AppletPropertyInfo;

static const AppletPropertyInfo applet_properties[] = {
    {"prefs-path", "PrefsPath"},
    {"orient", "Orient"},
    {"size", "Size"},
    {"size-hints", "SizeHints"},
    {"background", "Background"},
    {"flags", "Flags"},
    {"locked", "Locked"},
    {"locked-down", "LockedDown"}};

#define MATE_PANEL_APPLET_BUS_NAME "org.mate.panel.applet.%s"
#define MATE_PANEL_APPLET_FACTORY_INTERFACE \
  "org.mate.panel.applet.AppletFactory"
#define MATE_PANEL_APPLET_FACTORY_OBJECT_PATH "/org/mate/panel/applet/%s"
#define MATE_PANEL_APPLET_INTERFACE "org.mate.panel.applet.Applet"

#ifdef HAVE_X11
static gboolean mate_panel_applet_container_plug_removed(
    MatePanelAppletContainer *container);
#endif

G_DEFINE_TYPE_WITH_PRIVATE(MatePanelAppletContainer,
                           mate_panel_applet_container, GTK_TYPE_EVENT_BOX);

GQuark mate_panel_applet_container_error_quark(void) {
  return g_quark_from_static_string("mate-panel-applet-container-error-quark");
}

static void mate_panel_applet_container_init(
    MatePanelAppletContainer *container) {
  container->priv = mate_panel_applet_container_get_instance_private(container);

  container->priv->pending_ops = g_hash_table_new_full(
      g_direct_hash, g_direct_equal, NULL, (GDestroyNotify)g_object_unref);
}

static void panel_applet_container_setup(MatePanelAppletContainer *container) {
  if (container->priv->out_of_process) {
#ifdef HAVE_X11
    if (GDK_IS_X11_DISPLAY(gdk_display_get_default())) {
      container->priv->socket = gtk_socket_new();

      g_signal_connect_swapped(
          container->priv->socket, "plug-removed",
          G_CALLBACK(mate_panel_applet_container_plug_removed), container);

      gtk_container_add(GTK_CONTAINER(container), container->priv->socket);
      gtk_widget_show(container->priv->socket);
    } else
#endif
    { /* Not using X11 */
      g_warning(
          "%s requested out-of-process container, which is only supported on "
          "X11",
          container->priv->iid);
    }
  } else {
    GtkWidget *applet;

    applet = mate_panel_applets_manager_get_applet_widget(container->priv->iid,
                                                          container->priv->uid);

    gtk_container_add(GTK_CONTAINER(container), applet);
  }
}

static void mate_panel_applet_container_cancel_pending_operations(
    MatePanelAppletContainer *container) {
  GList *keys, *l;

  if (!container->priv->pending_ops) return;

  keys = g_hash_table_get_keys(container->priv->pending_ops);
  for (l = keys; l; l = g_list_next(l)) {
    GCancellable *cancellable;

    cancellable = G_CANCELLABLE(
        g_hash_table_lookup(container->priv->pending_ops, l->data));
    g_cancellable_cancel(cancellable);
  }
  g_list_free(keys);
}

static void mate_panel_applet_container_dispose(GObject *object) {
  MatePanelAppletContainer *container = MATE_PANEL_APPLET_CONTAINER(object);

  if (container->priv->pending_ops) {
    mate_panel_applet_container_cancel_pending_operations(container);
    g_hash_table_destroy(container->priv->pending_ops);
    container->priv->pending_ops = NULL;
  }

  g_clear_pointer(&container->priv->bus_name, g_free);
  g_clear_pointer(&container->priv->iid, g_free);

  if (container->priv->name_watcher_id > 0) {
    g_bus_unwatch_name(container->priv->name_watcher_id);
    container->priv->name_watcher_id = 0;
  }

  g_clear_object(&container->priv->applet_proxy);

  G_OBJECT_CLASS(mate_panel_applet_container_parent_class)->dispose(object);
}

static void mate_panel_applet_container_class_init(
    MatePanelAppletContainerClass *klass) {
  GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

  gobject_class->dispose = mate_panel_applet_container_dispose;

  signals[APPLET_BROKEN] = g_signal_new(
      "applet-broken", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET(MatePanelAppletContainerClass, applet_broken), NULL, NULL,
      g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
  signals[APPLET_MOVE] =
      g_signal_new("applet-move", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST,
                   G_STRUCT_OFFSET(MatePanelAppletContainerClass, applet_move),
                   NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
  signals[APPLET_REMOVE] = g_signal_new(
      "applet-remove", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET(MatePanelAppletContainerClass, applet_remove), NULL, NULL,
      g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
  signals[APPLET_LOCK] = g_signal_new(
      "applet-lock", G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_LAST,
      G_STRUCT_OFFSET(MatePanelAppletContainerClass, applet_lock), NULL, NULL,
      g_cclosure_marshal_VOID__BOOLEAN, G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
  signals[CHILD_PROPERTY_CHANGED] = g_signal_new(
      "child-property-changed", G_TYPE_FROM_CLASS(klass),
      G_SIGNAL_RUN_FIRST | G_SIGNAL_NO_RECURSE | G_SIGNAL_DETAILED |
          G_SIGNAL_NO_HOOKS,
      G_STRUCT_OFFSET(MatePanelAppletContainerClass, child_property_changed),
      NULL, NULL, panel_marshal_VOID__STRING_POINTER, G_TYPE_NONE, 2,
      G_TYPE_STRING, G_TYPE_POINTER);
}

static const AppletPropertyInfo *
mate_panel_applet_container_child_property_get_info(
    const gchar *property_name) {
  gsize i;

  g_assert(property_name != NULL);

  for (i = 0; i < G_N_ELEMENTS(applet_properties); i++) {
    if (g_ascii_strcasecmp(applet_properties[i].name, property_name) == 0)
      return &applet_properties[i];
  }

  return NULL;
}

GtkWidget *mate_panel_applet_container_new(void) {
  GtkWidget *container;

  container = GTK_WIDGET(g_object_new(PANEL_TYPE_APPLET_CONTAINER, NULL));

  return container;
}

#ifdef HAVE_X11
static gboolean mate_panel_applet_container_plug_removed(
    MatePanelAppletContainer *container) {
  g_return_val_if_fail(
      GDK_IS_X11_DISPLAY(gtk_widget_get_display(GTK_WIDGET(container))), FALSE);

  if (!container->priv->applet_proxy) return FALSE;

  mate_panel_applet_container_cancel_pending_operations(container);

  if (container->priv->name_watcher_id > 0) {
    g_bus_unwatch_name(container->priv->name_watcher_id);
    container->priv->name_watcher_id = 0;
  }

  g_object_unref(container->priv->applet_proxy);
  container->priv->applet_proxy = NULL;

  g_signal_emit(container, signals[APPLET_BROKEN], 0);

  /* Continue destroying, in case of reloading
   * a new frame widget is created
   */
  return FALSE;
}
#endif /* HAVE_X11 */

static void mate_panel_applet_container_child_signal(
    GDBusProxy *proxy, gchar *sender_name, gchar *signal_name,
    GVariant *parameters, MatePanelAppletContainer *container) {
  if (g_strcmp0(signal_name, "Move") == 0) {
    g_signal_emit(container, signals[APPLET_MOVE], 0);
  } else if (g_strcmp0(signal_name, "RemoveFromPanel") == 0) {
    g_signal_emit(container, signals[APPLET_REMOVE], 0);
  } else if (g_strcmp0(signal_name, "Lock") == 0) {
    g_signal_emit(container, signals[APPLET_LOCK], 0, TRUE);
  } else if (g_strcmp0(signal_name, "Unlock") == 0) {
    g_signal_emit(container, signals[APPLET_LOCK], 0, FALSE);
  }
}

static void on_property_changed(GDBusConnection *connection,
                                const gchar *sender_name,
                                const gchar *object_path,
                                const gchar *interface_name,
                                const gchar *signal_name, GVariant *parameters,
                                MatePanelAppletContainer *container) {
  GVariant *props;
  GVariantIter iter;
  GVariant *value;
  gchar *key;

  g_variant_get(parameters, "(s@a{sv}*)", NULL, &props, NULL);

  g_variant_iter_init(&iter, props);
  while (g_variant_iter_loop(&iter, "{sv}", &key, &value)) {
    if (g_strcmp0(key, "Flags") == 0) {
      g_signal_emit(container, signals[CHILD_PROPERTY_CHANGED],
                    g_quark_from_string("flags"), "flags", value);
    } else if (g_strcmp0(key, "SizeHints") == 0) {
      g_signal_emit(container, signals[CHILD_PROPERTY_CHANGED],
                    g_quark_from_string("size-hints"), "size-hints", value);
    }
  }

  g_variant_unref(props);
}

static void on_proxy_appeared(GObject *source_object, GAsyncResult *res,
                              gpointer user_data) {
  GTask *task = G_TASK(user_data);
  MatePanelAppletContainer *container;
  GDBusProxy *proxy;
  GError *error = NULL;

  proxy = g_dbus_proxy_new_finish(res, &error);
  if (!proxy) {
    g_task_return_error(task, error);
    g_object_unref(task);

    return;
  }

  container = MATE_PANEL_APPLET_CONTAINER(
      g_async_result_get_source_object(G_ASYNC_RESULT(task)));
  container->priv->applet_proxy = proxy;
  g_signal_connect(container->priv->applet_proxy, "g-signal",
                   G_CALLBACK(mate_panel_applet_container_child_signal),
                   container);
  g_dbus_connection_signal_subscribe(
      g_dbus_proxy_get_connection(proxy), g_dbus_proxy_get_name(proxy),
      "org.freedesktop.DBus.Properties", "PropertiesChanged",
      g_dbus_proxy_get_object_path(proxy), MATE_PANEL_APPLET_INTERFACE,
      G_DBUS_SIGNAL_FLAGS_NONE, (GDBusSignalCallback)on_property_changed,
      container, NULL);

  g_task_return_boolean(task, TRUE);
  g_object_unref(task);

  panel_applet_container_setup(container);

#ifdef HAVE_X11
  /* xid always <= 0 when not using X11 */
  if (container->priv->xid > 0) {
    gtk_socket_add_id(GTK_SOCKET(container->priv->socket),
                      container->priv->xid);
  }
#endif

  /* g_async_result_get_source_object returns new ref */
  g_object_unref(container);
}

static void get_applet_cb(GObject *source_object, GAsyncResult *res,
                          gpointer user_data) {
  GDBusConnection *connection = G_DBUS_CONNECTION(source_object);
  GTask *task = G_TASK(user_data);
  MatePanelAppletContainer *container;
  GVariant *retvals;
  const gchar *applet_path;
  GError *error = NULL;

  retvals = g_dbus_connection_call_finish(connection, res, &error);
  if (!retvals) {
    g_task_return_error(task, error);
    g_object_unref(task);

    return;
  }

  container = MATE_PANEL_APPLET_CONTAINER(
      g_async_result_get_source_object(G_ASYNC_RESULT(task)));
  g_variant_get(retvals, "(&obuu)", &applet_path,
                &container->priv->out_of_process, &container->priv->xid,
                &container->priv->uid);

  g_dbus_proxy_new(connection, G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES, NULL,
                   container->priv->bus_name, applet_path,
                   MATE_PANEL_APPLET_INTERFACE, NULL,
                   (GAsyncReadyCallback)on_proxy_appeared, task);

  g_variant_unref(retvals);

  /* g_async_result_get_source_object returns new ref */
  g_object_unref(container);
}

typedef struct {
  gchar *factory_id;
  GVariant *parameters;
  GCancellable *cancellable;
} AppletFactoryData;

static void applet_factory_data_free(AppletFactoryData *data) {
  g_free(data->factory_id);
  if (data->cancellable) g_object_unref(data->cancellable);

  g_free(data);
}

static void on_factory_appeared(GDBusConnection *connection, const gchar *name,
                                const gchar *name_owner, GTask *task) {
  MatePanelAppletContainer *container;
  gchar *object_path;
  AppletFactoryData *data;

  data = g_task_get_task_data(task);
  container = MATE_PANEL_APPLET_CONTAINER(
      g_async_result_get_source_object(G_ASYNC_RESULT(task)));
  container->priv->bus_name = g_strdup(name_owner);
  object_path =
      g_strdup_printf(MATE_PANEL_APPLET_FACTORY_OBJECT_PATH, data->factory_id);
  g_dbus_connection_call(
      connection, name_owner, object_path, MATE_PANEL_APPLET_FACTORY_INTERFACE,
      "GetApplet", data->parameters, G_VARIANT_TYPE("(obuu)"),
      G_DBUS_CALL_FLAGS_NONE, -1, data->cancellable, get_applet_cb, task);
  g_free(object_path);
}

static void mate_panel_applet_container_get_applet(
    MatePanelAppletContainer *container, GdkScreen *screen, const gchar *iid,
    GVariant *props, GCancellable *cancellable, GAsyncReadyCallback callback,
    gpointer user_data) {
  GTask *task;
  AppletFactoryData *data;
  gint screen_number;
  gchar *bus_name;
  gchar *factory_id;
  gchar *applet_id;

  task = g_task_new(G_OBJECT(container), cancellable, callback, user_data);
  g_task_set_source_tag(task, mate_panel_applet_container_get_applet);
  applet_id = g_strrstr(iid, "::");
  if (!applet_id) {
    g_task_return_new_error(task, MATE_PANEL_APPLET_CONTAINER_ERROR,
                            MATE_PANEL_APPLET_CONTAINER_INVALID_APPLET,
                            "Invalid applet iid: %s", iid);
    g_object_unref(task);
    return;
  }

  factory_id = g_strndup(iid, strlen(iid) - strlen(applet_id));
  applet_id += 2;

  /* we can't use the screen of the container widget since it's not in a
   * widget hierarchy yet */
#ifdef HAVE_X11
  if (GDK_IS_X11_DISPLAY(gdk_screen_get_display(screen))) {
    screen_number = gdk_x11_screen_get_screen_number(screen);
  } else
#endif
  { /* Not using X11 */
    screen_number = 0;
  }

  data = g_new(AppletFactoryData, 1);
  data->factory_id = factory_id;
  data->parameters = g_variant_new("(si*)", applet_id, screen_number, props);
  data->cancellable = cancellable ? g_object_ref(cancellable) : NULL;
  g_task_set_task_data(task, data, (GDestroyNotify)applet_factory_data_free);
  bus_name = g_strdup_printf(MATE_PANEL_APPLET_BUS_NAME, factory_id);

  container->priv->iid = g_strdup(iid);
  container->priv->name_watcher_id = g_bus_watch_name(
      G_BUS_TYPE_SESSION, bus_name, G_BUS_NAME_WATCHER_FLAGS_AUTO_START,
      (GBusNameAppearedCallback)on_factory_appeared, NULL, task, NULL);

  g_free(bus_name);
}

void mate_panel_applet_container_add(MatePanelAppletContainer *container,
                                     GdkScreen *screen, const gchar *iid,
                                     GCancellable *cancellable,
                                     GAsyncReadyCallback callback,
                                     gpointer user_data, GVariant *properties) {
  g_return_if_fail(PANEL_IS_APPLET_CONTAINER(container));
  g_return_if_fail(iid != NULL);

  mate_panel_applet_container_cancel_pending_operations(container);

  mate_panel_applet_container_get_applet(container, screen, iid, properties,
                                         cancellable, callback, user_data);
}

gboolean mate_panel_applet_container_add_finish(
    MatePanelAppletContainer *container, GAsyncResult *result, GError **error) {
  g_return_val_if_fail(g_task_is_valid(result, container), FALSE);
  g_warn_if_fail(g_task_get_source_tag(G_TASK(result)) ==
                 mate_panel_applet_container_get_applet);
  return g_task_propagate_boolean(G_TASK(result), error);
}

/* Child Properties */
static void set_applet_property_cb(GObject *source_object, GAsyncResult *res,
                                   gpointer user_data) {
  GDBusConnection *connection = G_DBUS_CONNECTION(source_object);
  GTask *task = G_TASK(user_data);
  MatePanelAppletContainer *container;
  GVariant *retvals;
  GError *error = NULL;

  retvals = g_dbus_connection_call_finish(connection, res, &error);
  if (!retvals) {
    if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      g_warning("Error setting property: %s\n", error->message);
    g_task_return_error(task, error);
    return;
  } else {
    g_variant_unref(retvals);
  }

  container = MATE_PANEL_APPLET_CONTAINER(
      g_async_result_get_source_object(G_ASYNC_RESULT(task)));
  g_hash_table_remove(container->priv->pending_ops, task);
  g_task_return_boolean(task, TRUE);
  g_object_unref(task);

  /* g_async_result_get_source_object returns new ref */
  g_object_unref(container);
}

gconstpointer mate_panel_applet_container_child_set(
    MatePanelAppletContainer *container, const gchar *property_name,
    const GVariant *value, GCancellable *cancellable,
    GAsyncReadyCallback callback, gpointer user_data) {
  GDBusProxy *proxy = container->priv->applet_proxy;
  const AppletPropertyInfo *info;
  GTask *task;
  if (!proxy) return NULL;

  info = mate_panel_applet_container_child_property_get_info(property_name);
  if (!info) {
    g_task_report_new_error(G_OBJECT(container), callback, user_data, NULL,
                            MATE_PANEL_APPLET_CONTAINER_ERROR,
                            MATE_PANEL_APPLET_CONTAINER_INVALID_CHILD_PROPERTY,
                            "%s: Applet has no child property named `%s'",
                            G_STRLOC, property_name);
    return NULL;
  }

  task = g_task_new(G_OBJECT(container), cancellable, callback, user_data);
  g_task_set_source_tag(task, mate_panel_applet_container_child_set);

  if (cancellable)
    g_object_ref(cancellable);
  else
    cancellable = g_cancellable_new();
  g_hash_table_insert(container->priv->pending_ops, task, cancellable);

  g_dbus_connection_call(
      g_dbus_proxy_get_connection(proxy), g_dbus_proxy_get_name(proxy),
      g_dbus_proxy_get_object_path(proxy), "org.freedesktop.DBus.Properties",
      "Set",
      g_variant_new("(ssv)", g_dbus_proxy_get_interface_name(proxy),
                    info->dbus_name, value),
      NULL, G_DBUS_CALL_FLAGS_NO_AUTO_START, -1, cancellable,
      set_applet_property_cb, task);

  return task;
}

gboolean mate_panel_applet_container_child_set_finish(
    MatePanelAppletContainer *container, GAsyncResult *result, GError **error) {
  g_return_val_if_fail(g_task_is_valid(result, container), FALSE);
  g_warn_if_fail(g_task_get_source_tag(G_TASK(result)) ==
                 mate_panel_applet_container_child_set);
  return g_task_propagate_boolean(G_TASK(result), error);
}

static void get_applet_property_cb(GObject *source_object, GAsyncResult *res,
                                   gpointer user_data) {
  GDBusConnection *connection = G_DBUS_CONNECTION(source_object);
  GTask *task = G_TASK(user_data);
  MatePanelAppletContainer *container;
  GVariant *retvals;
  GVariant *value, *item;
  GError *error = NULL;

  retvals = g_dbus_connection_call_finish(connection, res, &error);
  if (!retvals) {
    if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
      g_warning("Error getting property: %s\n", error->message);
    g_task_return_error(task, error);
    return;
  }

  item = g_variant_get_child_value(retvals, 0);
  value = g_variant_get_variant(item);
  g_variant_unref(item);
  g_variant_unref(retvals);

  container = MATE_PANEL_APPLET_CONTAINER(
      g_async_result_get_source_object(G_ASYNC_RESULT(task)));
  g_hash_table_remove(container->priv->pending_ops, task);
  g_task_return_pointer(task, value, (GDestroyNotify)g_variant_unref);
  g_object_unref(task);

  g_object_unref(container);
}

gconstpointer mate_panel_applet_container_child_get(
    MatePanelAppletContainer *container, const gchar *property_name,
    GCancellable *cancellable, GAsyncReadyCallback callback,
    gpointer user_data) {
  GDBusProxy *proxy = container->priv->applet_proxy;
  const AppletPropertyInfo *info;
  GTask *task;

  if (!proxy) return NULL;

  info = mate_panel_applet_container_child_property_get_info(property_name);
  if (!info) {
    g_warning("mate_panel_applet_container_child_get error");
    return NULL;
  }

  task = g_task_new(G_OBJECT(container), cancellable, callback, user_data);
  g_task_set_source_tag(task, mate_panel_applet_container_child_get);
  if (cancellable)
    g_object_ref(cancellable);
  else
    cancellable = g_cancellable_new();
  g_hash_table_insert(container->priv->pending_ops, task, cancellable);

  g_dbus_connection_call(
      g_dbus_proxy_get_connection(proxy), g_dbus_proxy_get_name(proxy),
      g_dbus_proxy_get_object_path(proxy), "org.freedesktop.DBus.Properties",
      "Get",
      g_variant_new("(ss)", g_dbus_proxy_get_interface_name(proxy),
                    info->dbus_name),
      G_VARIANT_TYPE("(v)"), G_DBUS_CALL_FLAGS_NO_AUTO_START, -1, cancellable,
      get_applet_property_cb, task);

  return task;
}

GVariant *mate_panel_applet_container_child_get_finish(
    MatePanelAppletContainer *container, GAsyncResult *result, GError **error) {
  g_return_val_if_fail(g_task_is_valid(result, container), NULL);
  g_warn_if_fail(g_task_get_source_tag(G_TASK(result)) ==
                 mate_panel_applet_container_child_get);

  return g_variant_ref(g_task_propagate_pointer(G_TASK(result), error));
}

static void child_popup_menu_cb(GObject *source_object, GAsyncResult *res,
                                gpointer user_data) {
  GDBusConnection *connection = G_DBUS_CONNECTION(source_object);
  GTask *task = G_TASK(user_data);
  GVariant *retvals;
  GError *error = NULL;

  retvals = g_dbus_connection_call_finish(connection, res, &error);
  if (!retvals) {
    g_task_return_error(task, error);
    return;
  } else {
    g_variant_unref(retvals);
    g_task_return_boolean(task, TRUE);
  }

  g_object_unref(task);
}

void mate_panel_applet_container_child_popup_menu(
    MatePanelAppletContainer *container, guint button, guint32 timestamp,
    GCancellable *cancellable, GAsyncReadyCallback callback,
    gpointer user_data) {
  GTask *task;
  GDBusProxy *proxy = container->priv->applet_proxy;

  if (!proxy) return;
  task = g_task_new(G_OBJECT(container), cancellable, callback, user_data);
  g_task_set_source_tag(task, mate_panel_applet_container_child_popup_menu);

  g_dbus_connection_call(
      g_dbus_proxy_get_connection(proxy), g_dbus_proxy_get_name(proxy),
      g_dbus_proxy_get_object_path(proxy), MATE_PANEL_APPLET_INTERFACE,
      "PopupMenu", g_variant_new("(uu)", button, timestamp), NULL,
      G_DBUS_CALL_FLAGS_NO_AUTO_START, -1, cancellable, child_popup_menu_cb,
      task);
}

gboolean mate_panel_applet_container_child_popup_menu_finish(
    MatePanelAppletContainer *container, GAsyncResult *result, GError **error) {
  g_return_val_if_fail(g_task_is_valid(result, container), FALSE);
  g_warn_if_fail(g_task_get_source_tag(G_TASK(result)) ==
                 mate_panel_applet_container_child_popup_menu);
  return g_task_propagate_boolean(G_TASK(result), error);
}

void mate_panel_applet_container_cancel_operation(
    MatePanelAppletContainer *container, gconstpointer operation) {
  gpointer value = g_hash_table_lookup(container->priv->pending_ops, operation);
  if (value == NULL) return;

  g_cancellable_cancel(G_CANCELLABLE(value));
}
