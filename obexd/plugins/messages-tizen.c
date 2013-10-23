/*
 *
 *  OBEX Server
 *
 *  Copyright (C) 2009-2010  Intel Corporation
 *  Copyright (C) 2007-2010  Marcel Holtmann <marcel@holtmann.org>
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <glib.h>
#include <string.h>
#include "log.h"
#include "messages.h"

#include <dbus/dbus.h>

#define QUERY_GET_FOLDER_TREE "GetFolderTree"
#define QUERY_GET_MSG_LIST "GetMessageList"
#define QUERY_GET_MESSAGE "GetMessage"
#define QUERY_UPDATE_MESSAGE "UpdateMessage"
#define QUERY_MESSAGE_STATUS "MessageStatus"
#define QUERY_NOTI_REGISTRATION "NotiRegistration"

#define BT_MAP_SERVICE_OBJECT_PATH "/org/bluez/map_agent"
#define BT_MAP_SERVICE_NAME "org.bluez.map_agent"
#define BT_MAP_SERVICE_INTERFACE "org.bluez.MapAgent"

static DBusConnection *g_conn = NULL;

struct mns_reg_data {
	int notification_status;
	char *remote_addr;
};

struct message_folder {
	char *name;
	GSList *subfolders;
};

struct session {
	char *cwd;
	struct message_folder *folder;
	char *name;
	uint16_t max;
	uint16_t offset;
	void *user_data;
	void (*folder_list_cb)(void *session, int err, uint16_t size,
					const char *name, void *user_data);
	struct messages_message *msg;
	const struct messages_filter *filter;
	void (*msg_list_cb)(void *session, int err, int size, gboolean newmsg,
				const struct messages_message *entry,
				void *user_data);
	void (*get_msg_cb)(void *session, int err, gboolean fmore,
				const char *chunk, void *user_data);
	void (*msg_update_cb)(void *session, int err, void *user_data);
	void (*msg_status_cb)(void *session, int err, void *user_data);
};

static struct message_folder *folder_tree = NULL;

static struct message_folder *get_folder(const char *folder)
{
	GSList *folders = folder_tree->subfolders;
	struct message_folder *last = NULL;
	char **path;
	int i;

	if (g_strcmp0(folder, "/") == 0)
		return folder_tree;

	path = g_strsplit(folder, "/", 0);

	for (i = 1; path[i] != NULL; i++) {
		gboolean match_found = FALSE;
		GSList *l;

		for (l = folders; l != NULL; l = g_slist_next(l)) {
			struct message_folder *folder = l->data;

			if (g_ascii_strncasecmp(folder->name, path[i],
					strlen(folder->name)) == 0) {
				match_found = TRUE;
				last = l->data;
				folders = folder->subfolders;
				break;
			}
		}

		if (!match_found) {
			g_strfreev(path);
			return NULL;
		}
	}

	g_strfreev(path);

	return last;
}

static void destroy_folder_tree(void *root)
{
	struct message_folder *folder = root;
	GSList *tmp, *next;

	if (folder == NULL)
		return;

	g_free(folder->name);

	tmp = folder->subfolders;
	while (tmp != NULL) {
		next = g_slist_next(tmp);
		destroy_folder_tree(tmp->data);
		tmp = next;
	}
	g_slist_free(folder->subfolders);
	g_free(folder);
}

static struct message_folder *create_folder(const char *name)
{
	struct message_folder *folder = g_new0(struct message_folder, 1);

	folder->name = g_strdup(name);
	return folder;
}

static void create_folder_tree()
{

	struct message_folder *parent, *child;

	folder_tree = create_folder("/");

	parent = create_folder("telecom");
	folder_tree->subfolders = g_slist_append(folder_tree->subfolders,
								parent);

	child = create_folder("msg");
	parent->subfolders = g_slist_append(parent->subfolders, child);
}

int messages_init(void)
{
	g_conn = dbus_bus_get(DBUS_BUS_SESSION, NULL);
	if (!g_conn) {
		error("Can't get on session bus");
		return -1;
	}

	create_folder_tree();
	return 0;
}

void messages_exit(void)
{
	destroy_folder_tree(folder_tree);

	if (g_conn) {
		dbus_connection_unref(g_conn);
		g_conn = NULL;
	}
}

static void message_get_folder_list(DBusPendingCall *call, void *user_data)
{
	DBusMessage *reply = dbus_pending_call_steal_reply(call);
	DBusMessageIter iter;
	DBusMessageIter iter_struct;
	DBusMessageIter entry;
	DBusError derr;
	const char *name = NULL;
	struct message_folder *parent = {0,}, *child = {0,};
	GSList *l;

	DBG("+\n");

	for (l = folder_tree->subfolders; l != NULL; l = parent->subfolders)
		parent = l->data;

	DBG("Last child folder = %s\n", parent->name);
	dbus_error_init(&derr);

	if (dbus_set_error_from_message(&derr, reply)) {
		error("Replied with an error: %s, %s", derr.name, derr.message);
		dbus_error_free(&derr);
	} else {
		dbus_message_iter_init(reply, &iter);
		dbus_message_iter_recurse(&iter, &iter_struct);

		while (dbus_message_iter_get_arg_type(&iter_struct) ==
							DBUS_TYPE_STRUCT) {
			dbus_message_iter_recurse(&iter_struct, &entry);

			dbus_message_iter_get_basic(&entry, &name);
			DBG("Folder name = %s\n", name);
			child = create_folder(name);
			parent->subfolders = g_slist_append(parent->subfolders,
							child);
			dbus_message_iter_next(&iter_struct);
		}
	}
	dbus_message_unref(reply);
	DBG("-\n");
}

static void message_get_msg_list(DBusPendingCall *call, void *user_data)
{
	DBusMessage *reply = dbus_pending_call_steal_reply(call);
	DBusMessageIter iter;
	DBusMessageIter iter_struct;
	DBusMessageIter entry;
	DBusError derr;
	const char *msg_handle;
	const char *msg_type;
	const char *msg_time;
	struct session *session = user_data;
	struct messages_message *data = g_new0(struct messages_message, 1);

	DBG("+\n");

	dbus_error_init(&derr);

	if (dbus_set_error_from_message(&derr, reply)) {
		error("Replied with an error: %s, %s", derr.name, derr.message);
		dbus_error_free(&derr);
	} else {
		dbus_message_iter_init(reply, &iter);
		dbus_message_iter_recurse(&iter, &iter_struct);

		while (dbus_message_iter_get_arg_type(&iter_struct) ==
							DBUS_TYPE_STRUCT) {
			dbus_message_iter_recurse(&iter_struct, &entry);
			dbus_message_iter_get_basic(&entry, &msg_handle);
			DBG("Msg handle = %s\n", msg_handle);
			data->handle = g_strdup(msg_handle);
			dbus_message_iter_next(&entry);
			dbus_message_iter_get_basic(&entry, &msg_type);
			DBG("Msg Type = %s\n", msg_type);
			data->mask |= PMASK_TYPE;
			data->type = g_strdup(msg_type);
			dbus_message_iter_next(&entry);
			dbus_message_iter_get_basic(&entry, &msg_time);
			DBG("Msg date & time = %s\n", msg_time);
			data->mask |= PMASK_DATETIME;
			data->datetime = g_strdup(msg_time);

			session->msg_list_cb(session, -EAGAIN, 1,
					TRUE,
					data,
					session->user_data);
			dbus_message_iter_next(&iter_struct);
		}
		session->msg_list_cb(session, 0, 0,
					FALSE,
					NULL,
					session->user_data);
	}
	dbus_message_unref(reply);
	DBG("-\n");
}

static void message_get_msg(DBusPendingCall *call, void *user_data)
{
	DBusMessage *reply = dbus_pending_call_steal_reply(call);
	DBusMessageIter iter;
	DBusMessageIter iter_struct;
	DBusMessageIter entry;
	DBusError derr;
	struct session *session = user_data;
	char *msg_body;

	DBG("+\n");

	dbus_error_init(&derr);
	if (dbus_set_error_from_message(&derr, reply)) {
		error("Replied with an error: %s, %s", derr.name, derr.message);
		dbus_error_free(&derr);
	} else {
		dbus_message_iter_init(reply, &iter);
		dbus_message_iter_recurse(&iter, &iter_struct);

		if (dbus_message_iter_get_arg_type(&iter_struct) ==
							DBUS_TYPE_STRUCT) {
			dbus_message_iter_recurse(&iter_struct, &entry);
			dbus_message_iter_get_basic(&entry, &msg_body);
			DBG("Msg handle = %s\n", msg_body);
		}
		session->get_msg_cb(session, -EAGAIN, FALSE,
					msg_body, session->user_data);
		session->get_msg_cb(session, 0, FALSE,
					NULL, session->user_data);
	}
	dbus_message_unref(reply);
	DBG("-\n");
}

int messages_connect(void **s)
{
	DBusPendingCall *call;
	DBusMessage *message;
	DBG("+\n");

	struct session *session = g_new0(struct session, 1);

	session->cwd = g_strdup("/");
	session->folder = folder_tree;

	*s = session;

	message = dbus_message_new_method_call(BT_MAP_SERVICE_NAME,
						BT_MAP_SERVICE_OBJECT_PATH,
						BT_MAP_SERVICE_INTERFACE,
						QUERY_GET_FOLDER_TREE);
	if (!message) {
		error("Can't allocate new message");
		return -1;
	}

	if (dbus_connection_send_with_reply(g_conn, message, &call, -1) ==
			FALSE) {
		error("Could not send dbus message");
		dbus_message_unref(message);
		return -1;
	}

	dbus_pending_call_set_notify(call, message_get_folder_list, session,
						NULL);
	dbus_message_unref(message);
	DBG("-\n");
	return 0;
}

void messages_disconnect(void *s)
{
	DBG("+\n");
	struct session *session = s;

	g_free(session->cwd);
	g_free(session);

	DBG("-\n");
}

int messages_set_notification_registration(void *session,
		void (*send_event)(void *session,
			const struct messages_event *event, void *user_data),
		void *user_data)
{
	return -EINVAL;
}

int messages_set_folder(void *s, const char *name, gboolean cdup)
{
	struct session *session = s;
	char *newrel = NULL;
	char *newabs;
	char *tmp;

	if (name && (strchr(name, '/') || strcmp(name, "..") == 0))
		return -EBADR;

	if (cdup) {
		if (session->cwd[0] == 0)
			return -ENOENT;

		newrel = g_path_get_dirname(session->cwd);

		/* We use empty string for indication of the root directory */
		if (newrel[0] == '.' && newrel[1] == 0)
			newrel[0] = 0;
	}

	tmp = newrel;
	if (!cdup && (!name || name[0] == 0))
		newrel = g_strdup("");
	else
		newrel = g_build_filename(newrel ? newrel : session->cwd, name,
									NULL);
	g_free(tmp);

	if (newrel[0] != '/')
		newabs = g_build_filename("/", newrel, NULL);
	else
		newabs = g_strdup(newrel);

	session->folder = get_folder(newabs);
	if (session->folder == NULL) {
		g_free(newrel);
		g_free(newabs);

		return -ENOENT;
	}

	g_free(newrel);
	g_free(session->cwd);
	session->cwd = newabs;

	return 0;
}

static gboolean async_get_folder_listing(void *s)
{
	struct session *session = s;
	gboolean count = FALSE;
	int folder_count = 0;
	char *path = NULL;
	struct message_folder *folder;
	GSList *dir;

	if (session->name && strchr(session->name, '/') != NULL)
		goto done;

	path = g_build_filename(session->cwd, session->name, NULL);

	if (path == NULL || strlen(path) == 0)
		goto done;

	folder = get_folder(path);

	if (folder == NULL)
		goto done;

	if (session->max == 0) {
		session->max = 0xffff;
		session->offset = 0;
		count = TRUE;
	}

	for (dir = folder->subfolders; dir &&
				(folder_count - session->offset) < session->max;
				folder_count++, dir = g_slist_next(dir)) {
		struct message_folder *dir_data = dir->data;

		if (count == FALSE && session->offset <= folder_count)
			session->folder_list_cb(session, -EAGAIN, 0,
					dir_data->name, session->user_data);
	}

 done:
	session->folder_list_cb(session, 0, folder_count, NULL,
							session->user_data);

	g_free(path);
	g_free(session->name);

	return FALSE;
}

int messages_get_folder_listing(void *s, const char *name,
					uint16_t max, uint16_t offset,
					messages_folder_listing_cb callback,
					void *user_data)
{
	DBG("+\n");
	struct session *session = s;
	session->name = g_strdup(name);
	session->max = max;
	session->offset = offset;
	session->folder_list_cb = callback;
	session->user_data = user_data;

	g_idle_add_full(G_PRIORITY_DEFAULT_IDLE, async_get_folder_listing,
						session, NULL);

	DBG("-\n");
	return 0;
}

int messages_get_messages_listing(void *session, const char *name,
				uint16_t max, uint16_t offset,
				uint8_t subject_len,
				const struct messages_filter *filter,
				messages_get_messages_listing_cb callback,
				void *user_data)
{
	DBusPendingCall *call;
	DBusMessage *message;
	struct session *s = session;

	if (strlen(name) != 0)
		s->name = g_strdup(name);
	 else
		s->name = g_strdup(s->cwd);

	s->max = max;
	s->offset = offset;
	s->filter = filter;
	s->msg_list_cb = (void *)callback;
	s->user_data = user_data;

	message = dbus_message_new_method_call(BT_MAP_SERVICE_NAME,
						BT_MAP_SERVICE_OBJECT_PATH,
						BT_MAP_SERVICE_INTERFACE,
						QUERY_GET_MSG_LIST);
	if (!message) {
		error("Can't allocate new message");
		g_free(s->name);
		return -1;
	}
	dbus_message_append_args(message, DBUS_TYPE_STRING, &s->name,
							DBUS_TYPE_INVALID);

	if (dbus_connection_send_with_reply(g_conn, message, &call, -1) ==
					FALSE) {
		error("Could not send dbus message");
		dbus_message_unref(message);
		g_free(s->name);
		return -1;
	}
	dbus_pending_call_set_notify(call, message_get_msg_list, s, NULL);
	dbus_message_unref(message);
	g_free(s->name);
	DBG("-\n");
	return 1;
}

int messages_get_message(void *session,
					const char *handle,
					unsigned long flags,
					messages_get_message_cb callback,
					void *user_data)
{
	DBusPendingCall *call;
	DBusMessage *message;
	struct session *s = session;
	char *message_name;
	DBG("+\n");

	if (NULL != handle) {
		message_name =  g_strdup(handle);
		DBG("Message handle = %s\n", handle);
	} else {
		return -1;
	}
	s->get_msg_cb = callback;
	s->user_data = user_data;

	message = dbus_message_new_method_call(BT_MAP_SERVICE_NAME,
						BT_MAP_SERVICE_OBJECT_PATH,
						BT_MAP_SERVICE_INTERFACE,
						QUERY_GET_MESSAGE);
	if (!message) {
		error("Can't allocate new message");
		g_free(message_name);
		return -1;
	}
	dbus_message_append_args(message, DBUS_TYPE_STRING, &message_name,
							DBUS_TYPE_INVALID);

	if (dbus_connection_send_with_reply(g_conn, message, &call, -1) ==
					FALSE) {
		error("Could not send dbus message");
		dbus_message_unref(message);
		g_free(message_name);
		return -1;
	}
	dbus_pending_call_set_notify(call, message_get_msg, s, NULL);
	dbus_message_unref(message);
	g_free(message_name);
	DBG("-\n");
	return 1;
}

static void message_update_msg(DBusPendingCall *call, void *user_data)
{
	DBusMessage *reply = dbus_pending_call_steal_reply(call);
	DBusMessageIter iter;
	DBusError derr;
	struct session *session = user_data;
	int err;
	DBG("+\n");

	dbus_error_init(&derr);
	if (dbus_set_error_from_message(&derr, reply)) {
		error("Replied with an error: %s, %s", derr.name, derr.message);
		dbus_error_free(&derr);
	} else {
		dbus_message_iter_init(reply, &iter);
		if (dbus_message_iter_get_arg_type(&iter) ==
							DBUS_TYPE_INT32) {
			dbus_message_iter_get_basic(&iter, &err);
			DBG("Error : %d\n", err);
			session->msg_update_cb(session, err,
						session->user_data);
		}
	}
	dbus_message_unref(reply);
	DBG("-\n");
}

int messages_update_inbox(void *session,
					messages_status_cb callback,
					void *user_data)
{
	DBusPendingCall *call;
	DBusMessage *message;
	struct session *s = session;

	DBG("+\n");

	s->msg_update_cb = callback;
	s->user_data = user_data;

	message = dbus_message_new_method_call(BT_MAP_SERVICE_NAME,
						BT_MAP_SERVICE_OBJECT_PATH,
						BT_MAP_SERVICE_INTERFACE,
						QUERY_UPDATE_MESSAGE);
	if (!message) {
		error("Can't allocate new message");
		return -1;
	}

	if (dbus_connection_send_with_reply(g_conn, message, &call, -1) ==
					FALSE) {
		error("Could not send dbus message");
		dbus_message_unref(message);
		return -1;
	}
	dbus_pending_call_set_notify(call, message_update_msg, s, NULL);
	dbus_message_unref(message);
	DBG("-\n");
	return 1;
}

static void message_status_msg(DBusPendingCall *call, void *user_data)
{
	DBusMessage *reply = dbus_pending_call_steal_reply(call);
	DBusMessageIter iter;
	DBusError derr;
	struct session *session = user_data;
	int err;

	DBG("+\n");

	dbus_error_init(&derr);
	if (dbus_set_error_from_message(&derr, reply)) {
		error("Replied with an error: %s, %s", derr.name, derr.message);
		dbus_error_free(&derr);
	} else {
		dbus_message_iter_init(reply, &iter);
		if (dbus_message_iter_get_arg_type(&iter) ==
							DBUS_TYPE_INT32) {
			dbus_message_iter_get_basic(&iter, &err);
			DBG("Error : %d\n", err);
			session->msg_status_cb(session, err,
						session->user_data);
		}
	}
	dbus_message_unref(reply);
	DBG("-\n");
}

static int messages_set_message_status(void *session, const char *handle,
					int indicator, int value,
					messages_status_cb callback,
					void *user_data)
{
	DBusPendingCall *call;
	DBusMessage *message;
	struct session *s = session;
	char *message_name;

	DBG("+\n");

	if (NULL == handle)
		return -1;

	message_name =  g_strdup(handle);
	DBG("Message handle = %s\n", handle);

	s->msg_status_cb = callback;
	s->user_data = user_data;

	message = dbus_message_new_method_call(BT_MAP_SERVICE_NAME,
						BT_MAP_SERVICE_OBJECT_PATH,
						BT_MAP_SERVICE_INTERFACE,
						QUERY_MESSAGE_STATUS);
	if (!message) {
		error("Can't allocate new message");
		g_free(message_name);
		return -1;
	}

	dbus_message_append_args(message, DBUS_TYPE_STRING, &message_name,
						DBUS_TYPE_INT32, &indicator,
						DBUS_TYPE_INT32, &value,
						DBUS_TYPE_INVALID);

	if (dbus_connection_send_with_reply(g_conn, message, &call, -1) ==
					FALSE) {
		error("Could not send dbus message");
		g_free(message_name);
		dbus_message_unref(message);
		return -1;
	}
	dbus_pending_call_set_notify(call, message_status_msg, s, NULL);
	dbus_message_unref(message);
	g_free(message_name);
	DBG("-\n");
	return 1;
}

int messages_set_read(void *session, const char *handle, uint8_t value,
                                messages_status_cb callback, void *user_data)
{
	return messages_set_message_status(session, handle, 0,
						value, callback, user_data);
}

int messages_set_delete(void *session, const char *handle, uint8_t value,
                                        messages_status_cb callback,
                                        void *user_data)
{
	return messages_set_message_status(session, handle, 0,
						value, callback, user_data);
}

static gboolean notification_registration(gpointer user_data)
{
	DBG("+\n");
	DBusMessage *message = NULL;
	gboolean reg;
	struct mns_reg_data *data = (struct mns_reg_data *)user_data;

	message = dbus_message_new_method_call(BT_MAP_SERVICE_NAME,
					BT_MAP_SERVICE_OBJECT_PATH,
					BT_MAP_SERVICE_INTERFACE,
					QUERY_NOTI_REGISTRATION);
	if (!message) {
		error("Can't allocate new message");
		goto done;
	}

	DBG("data->notification_status = %d\n", data->notification_status);

	if (data->notification_status == 1)
		reg = TRUE;
	else
		reg = FALSE;

	dbus_message_append_args(message, DBUS_TYPE_STRING, &data->remote_addr,
						DBUS_TYPE_BOOLEAN, &reg,
						DBUS_TYPE_INVALID);

	if (dbus_connection_send(g_conn, message, NULL) == FALSE)
		error("Could not send dbus message");

done:
	if (message)
		dbus_message_unref(message);

	g_free(data->remote_addr);
	g_free(data);

	DBG("-\n");
	return FALSE;
}

int messages_notification_registration(void *session,
				char *address, int status,
				messages_notification_registration_cb callback,
				void *user_data)
{
	DBG("+\n");
	struct mns_reg_data *data = g_new0(struct mns_reg_data, 1);
	data->notification_status = status;
	data->remote_addr = g_strdup(address);

	DBG("status = %d\n", status);

	g_idle_add(notification_registration, data);
	DBG("-\n");
	return 1;
}

void messages_abort(void *session)
{
}
