/*
 * OBEX Server
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact:  Hocheol Seo <hocheol.seo@samsung.com>
 *		 Girishashok Joshi <girish.joshi@samsung.com>
 *		 Chanyeol Park <chanyeol.park@samsung.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *		http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <glib.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "log.h"
#include "phonebook.h"

#include <dbus/dbus.h>
#include <gdbus.h>

#define PHONEBOOK_DESTINATION		"org.bluez.pb_agent"
#define PHONEBOOK_PATH			"/org/bluez/pb_agent"
#define PHONEBOOK_INTERFACE		"org.bluez.PbAgent"

#define QUERY_GET_PHONEBOOK_SIZE	"GetPhonebookSize"
#define QUERY_GET_PHONEBOOK		"GetPhonebook"
#define QUERY_GET_PHONEBOOK_LIST	"GetPhonebookList"
#define QUERY_GET_PHONEBOOK_ENTRY	"GetPhonebookEntry"

struct phonebook_data {
	phonebook_cb cb;
	DBusPendingCallNotifyFunction reply_cb;
	void *user_data;
	const struct apparam_field *params;

	phonebook_entry_cb entry_cb;
	phonebook_cache_ready_cb ready_cb;

	char *req_name;
};

struct phonebook_session {
	DBusConnection *connection;
	phonebook_cache_clear_cb clear_cb;
	unsigned int clear_id;

	void *user_data;
};

static gboolean folder_is_valid(const char *folder)
{
	if (folder == NULL)
		return FALSE;

	if (g_str_equal(folder, "/"))
		return TRUE;
	else if (g_str_equal(folder, "/telecom"))
		return TRUE;
	else if (g_str_equal(folder, "/telecom/pb"))
		return TRUE;
	else if (g_str_equal(folder, "/telecom/ich"))
		return TRUE;
	else if (g_str_equal(folder, "/telecom/och"))
		return TRUE;
	else if (g_str_equal(folder, "/telecom/mch"))
		return TRUE;
	else if (g_str_equal(folder, "/telecom/cch"))
		return TRUE;

	return FALSE;
}

static gboolean phonebook_request(struct phonebook_data *data,
				const gchar *method,
				DBusPendingCallNotifyFunction function,
				int first_arg_type,
				...)
{
	DBusConnection *connection;
	DBusPendingCall *call;
	DBusMessage *message;

	va_list args;

	if (!method) {
		error("Can't get method name");
		return FALSE;
	}

	DBG("%s\n", method);

	connection = g_dbus_setup_bus(DBUS_BUS_SYSTEM, NULL, NULL);
	if (!connection) {
		error("Can't get on session bus");
		return FALSE;
	}

	message = dbus_message_new_method_call(PHONEBOOK_DESTINATION,
			PHONEBOOK_PATH,
			PHONEBOOK_INTERFACE,
			method);
	if (!message) {
		dbus_connection_unref(connection);
		error("Can't Allocate new message");
		return FALSE;
	}

	va_start(args, first_arg_type);
	dbus_message_append_args_valist(message, first_arg_type, args);
	va_end(args);

	if (dbus_connection_send_with_reply(connection, message, &call, -1) ==
					 FALSE) {
		dbus_message_unref(message);
		dbus_connection_unref(connection);
		return FALSE;
	}
	dbus_pending_call_set_notify(call, function, data, NULL);

	dbus_pending_call_unref(call);
	dbus_message_unref(message);
	dbus_connection_unref(connection);

	return TRUE;
}

static void get_phonebook_size_reply(DBusPendingCall *call, void *user_data)
{
	DBusMessage *reply = dbus_pending_call_steal_reply(call);
	struct phonebook_data *s_data = user_data;
	DBusError derr;
	uint32_t phonebook_size;

	DBG("");

	if (!reply) {
		DBG("Reply Error\n");
		return;
	}

	dbus_error_init(&derr);
	if (dbus_set_error_from_message(&derr, reply)) {
		error("Replied with an error: %s, %s", derr.name, derr.message);
		dbus_error_free(&derr);
		phonebook_size = 0;
	} else {
		dbus_message_get_args(reply, NULL, DBUS_TYPE_UINT32,
					&phonebook_size, DBUS_TYPE_INVALID);
		DBG("phonebooksize:%d\n", phonebook_size);
	}

	s_data->cb(NULL, 0, phonebook_size, 0, TRUE, s_data->user_data);

	dbus_message_unref(reply);
}

static void get_phonebook_reply(DBusPendingCall *call, void *user_data)
{
	DBusMessage *reply = dbus_pending_call_steal_reply(call);
	struct phonebook_data *s_data = user_data;
	DBusError derr;
	GString *buffer;

	guint new_missed_call = 0;

	uint32_t count = 0;

	DBG("");

	if (!reply) {
		DBG("Reply Error\n");
		return;
	}

	buffer = g_string_new("");

	dbus_error_init(&derr);
	if (dbus_set_error_from_message(&derr, reply)) {
		error("Replied with an error: %s, %s", derr.name, derr.message);
		dbus_error_free(&derr);
	} else {
		DBusMessageIter iter;

		dbus_message_iter_init(reply, &iter);

		if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_ARRAY) {
			DBusMessageIter recurse_iter;

			dbus_message_iter_recurse(&iter, &recurse_iter);
			while (dbus_message_iter_get_arg_type(&recurse_iter) ==
							DBUS_TYPE_STRING) {
				gchar *str;

				dbus_message_iter_get_basic(&recurse_iter,
							&str);
				dbus_message_iter_next(&recurse_iter);

				g_string_append(buffer, str);

				DBG("str:\n%s\n", str);

				count++;
			}
			dbus_message_iter_next(&iter);
		}

		if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_UINT16)
			dbus_message_iter_get_basic(&iter, &new_missed_call);

		DBG("new_missed_call %d\n", new_missed_call);
	}

	s_data->cb(buffer->str, buffer->len, count, new_missed_call, 1,
							s_data->user_data);

	g_string_free(buffer, TRUE);
	dbus_message_unref(reply);
}


static void get_phonebook_list_reply(DBusPendingCall *call, void *user_data)
{
	DBusMessage *reply = dbus_pending_call_steal_reply(call);
	DBusMessageIter iter, iter_struct, entry;
	struct phonebook_data *data = user_data;
	DBusError derr;
	uint32_t handle = 0;
	const char *name = NULL;
	const char *tel = NULL;
	char id[10];

	if (!reply) {
		DBG("Reply Error\n");
		return;
	}

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
			dbus_message_iter_next(&entry);
			dbus_message_iter_get_basic(&entry, &tel);
			dbus_message_iter_next(&entry);
			dbus_message_iter_get_basic(&entry, &handle);

			/*
			DBG("[handle:%d name:%s tel:%s]\n", handle, name, tel);
			*/

			snprintf(id, sizeof(id), "%d.vcf", handle);

			data->entry_cb(id, handle, name, NULL, tel,
					data->user_data);

			dbus_message_iter_next(&iter_struct);
		}
	}

	data->ready_cb(data->user_data);

	dbus_message_unref(reply);
}

static void get_phonebook_entry_reply(DBusPendingCall *call, void *user_data)
{
	DBusMessage *reply = dbus_pending_call_steal_reply(call);
	struct phonebook_data *s_data = user_data;
	DBusError derr;
	const char *phonebook_entry = NULL;
	uint32_t phonebook_entry_size = 0;
	int vcards = 1;

	DBG("");

	if (!reply) {
		DBG("Reply Error\n");
		return;
	}

	dbus_error_init(&derr);
	if (dbus_set_error_from_message(&derr, reply)) {
		error("Replied with an error: %s, %s",
				derr.name, derr.message);
		dbus_error_free(&derr);
		vcards = -1;
	} else {
		dbus_message_get_args(reply, NULL, DBUS_TYPE_STRING,
					&phonebook_entry, DBUS_TYPE_INVALID);
		if (phonebook_entry)
			phonebook_entry_size = strlen(phonebook_entry);
		DBG("phonebook_entry:[%s]\n", phonebook_entry);
	}

	s_data->cb(phonebook_entry, phonebook_entry_size, vcards, 0, TRUE,
							s_data->user_data);

	dbus_message_unref(reply);
}

static gboolean get_sim_phonebook_reply(void *user_data)
{
	struct phonebook_data *s_data = user_data;
	uint32_t phonebook_size = 0;
	int lastpart = 1;

	DBG("");

	/* We don't support phonebook of SIM
	 * Send dummy data */
	s_data->cb(NULL, 0, phonebook_size, 0, lastpart, s_data->user_data);

	return FALSE;
}

static gboolean clear_signal(DBusConnection *conn, DBusMessage *msg,
			void *user_data)
{
	struct phonebook_session *session;

	if (user_data == NULL)
		return FALSE;

	DBG("");
	session = user_data;

	session->clear_cb(session->user_data);

	g_dbus_remove_watch(session->connection, session->clear_id);
	session->clear_id = 0;

	return TRUE;
}


int phonebook_init(void)
{
	return 0;
}

void phonebook_exit(void)
{
}

int phonebook_connect(void **user_data)
{
	struct phonebook_session *session;

	DBG("");

	session = g_new0(struct phonebook_session, 1);

	*user_data = session;

	return 0;
}

void phonebook_disconnect(void *user_data)
{
	struct phonebook_session *session;

	DBG("");
	session = user_data;

	g_dbus_remove_watch(session->connection, session->clear_id);
	dbus_connection_unref(session->connection);

	g_free(session);
}

char *phonebook_set_folder(const char *current_folder,
		const char *new_folder, uint8_t flags, int *err)
{
	char *tmp1, *tmp2, *base, *path = NULL;
	gboolean root, child;
	int ret = 0;
	int len;

	root = (g_strcmp0("/", current_folder) == 0);
	child = (new_folder && strlen(new_folder) != 0);

	switch (flags) {
	case 0x02:
		/* Go back to root */
		if (!child) {
			path = g_strdup("/");
			goto done;
		}

		path = g_build_filename(current_folder, new_folder, NULL);
		break;
	case 0x03:
		/* Go up 1 level */
		if (root) {
			/* Already root */
			path = g_strdup("/");
			goto done;
		}

		/*
		 * Removing one level of the current folder. Current folder
		 * contains AT LEAST one level since it is not at root folder.
		 * Use glib utility functions to handle invalid chars in the
		 * folder path properly.
		 */
		tmp1 = g_path_get_basename(current_folder);
		tmp2 = g_strrstr(current_folder, tmp1);
		len = tmp2 - (current_folder + 1);

		g_free(tmp1);

		if (len == 0)
			base = g_strdup("/");
		else
			base = g_strndup(current_folder, len);

		/* Return: one level only */
		if (!child) {
			path = base;
			goto done;
		}

		path = g_build_filename(base, new_folder, NULL);
		g_free(base);

		break;
	default:
		ret = -EBADR;
		break;
	}

done:
	if (path && !folder_is_valid(path))
		ret = -ENOENT;

	if (ret < 0) {
		g_free(path);
		path = NULL;
	}

	if (err)
		*err = ret;

	return path;
}

void phonebook_req_finalize(void *request)
{
	struct phonebook_data *data = request;

	DBG("");

	if (!data)
		return;

	g_free(data->req_name);
	g_free(data);
}

void *phonebook_pull(const char *name, const struct apparam_field *params,
				phonebook_cb cb, void *user_data, int *err)
{
	struct phonebook_data *data;

	DBG("name %s", name);

	if (!g_str_has_suffix(name, ".vcf")) {
		DBG("invaild request");
		if (err)
			*err = -EBADR;
		return NULL;
	}

	data = g_new0(struct phonebook_data, 1);
	data->params = params;
	data->user_data = user_data;
	data->cb = cb;
	data->req_name = g_strdup(name);

	if (err)
		*err = 0;

	return data;
}

int phonebook_pull_read(void *request)
{
	struct phonebook_data *data = request;

	DBG("name %s", data->req_name);

	if (data->params->maxlistcount == 0) {
		phonebook_request(data,
				QUERY_GET_PHONEBOOK_SIZE,
				get_phonebook_size_reply,
				DBUS_TYPE_STRING, &data->req_name,
				DBUS_TYPE_INVALID);
		return 0;
	}

	if (g_strcmp0(data->req_name, "/SIM1/telecom/pb.vcf") == 0) {
		g_idle_add(get_sim_phonebook_reply, data);
		return 0;
	}

	phonebook_request(data,
			QUERY_GET_PHONEBOOK,
			get_phonebook_reply,
			DBUS_TYPE_STRING, &data->req_name,
			DBUS_TYPE_UINT64, &data->params->filter,
			DBUS_TYPE_BYTE, &data->params->format,
			DBUS_TYPE_UINT16, &data->params->maxlistcount,
			DBUS_TYPE_UINT16, &data->params->liststartoffset,
			DBUS_TYPE_INVALID);

	return 0;
}

void *phonebook_get_entry(const char *folder, const char *id,
			const struct apparam_field *params, phonebook_cb cb,
			void *user_data, int *err)
{
	struct phonebook_data *data;

	if (!g_str_has_suffix(id, ".vcf")) {
		DBG("invaild request");
		if (err)
			*err = -EBADR;
		return NULL;
	}
	DBG("folder %s id %s", folder, id);

	data = g_new0(struct phonebook_data, 1);
	data->params = params;
	data->user_data = user_data;
	data->cb = cb;

	phonebook_request(data,
			QUERY_GET_PHONEBOOK_ENTRY,
			get_phonebook_entry_reply,
			DBUS_TYPE_STRING, &folder,
			DBUS_TYPE_STRING, &id,
			DBUS_TYPE_UINT64, &data->params->filter,
			DBUS_TYPE_BYTE, &data->params->format,
			DBUS_TYPE_INVALID);

	return data;
}

void *phonebook_create_cache(const char *name, phonebook_entry_cb entry_cb,
		phonebook_cache_ready_cb ready_cb, void *user_data, int *err)
{
	struct phonebook_data *data;
	gboolean req = FALSE;

	DBG("name %s", name);

	data = g_new0(struct phonebook_data, 1);
	data->user_data = user_data;
	data->entry_cb = entry_cb;
	data->ready_cb = ready_cb;

	req = phonebook_request(data,
				QUERY_GET_PHONEBOOK_LIST,
				get_phonebook_list_reply,
				DBUS_TYPE_STRING, &name,
				DBUS_TYPE_INVALID);

	if (*err) {
		if (req)
			*err = 0;
		else
			*err = -ENOENT;
	}

	return data;
}

void phonebook_set_cache_notification(void *session,
				phonebook_cache_clear_cb clear_cb,
				void *user_data)
{
	struct phonebook_session *s = session;

	DBG("");
	s->clear_cb = clear_cb;

	if (s->connection == NULL) {
		s->connection = g_dbus_setup_bus(DBUS_BUS_SYSTEM,
				NULL, NULL);

		if (s->connection == NULL) {
			error("Can't get on s bus");
			return;
		}
	}

	s->user_data = user_data;

	if (s->clear_id) {
		g_dbus_remove_watch(s->connection, s->clear_id);
		s->clear_id = 0;
	}

	s->clear_id = g_dbus_add_signal_watch(s->connection,
			NULL, PHONEBOOK_PATH, PHONEBOOK_INTERFACE,
			"clear", clear_signal,
			s, NULL);
}
