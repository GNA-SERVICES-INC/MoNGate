/* upstart
 *
 * Copyright © 2009 Canonical Ltd.
 * Author: Scott James Remnant <scott@netsplit.com>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef INIT_CONTROL_H
#define INIT_CONTROL_H

#include <dbus/dbus.h>

#include <nih/macros.h>

#include <nih-dbus/dbus_connection.h>
#include <nih-dbus/dbus_message.h>


NIH_BEGIN_EXTERN

extern DBusServer     *control_server;
extern DBusConnection *control_bus;

extern NihList        *control_conns;


void control_init                 (void);

int  control_server_open          (void)
	__attribute__ ((warn_unused_result));
void control_server_close         (void);

int  control_bus_open             (void)
	__attribute__ ((warn_unused_result));
void control_bus_close            (void);

int  control_reload_configuration (void *data, NihDBusMessage *message)
	__attribute__ ((warn_unused_result));

int  control_get_job_by_name      (void *data, NihDBusMessage *message,
				   const char *name, char **job)
	__attribute__ ((warn_unused_result));
int  control_get_all_jobs         (void *data, NihDBusMessage *message,
				   char ***jobs)
	__attribute__ ((warn_unused_result));

int  control_emit_event           (void *data, NihDBusMessage *message,
				   const char *name, char * const *env,
				   int wait)
	__attribute__ ((warn_unused_result));

int  control_get_version          (void *data, NihDBusMessage *message,
				   char **version)
	__attribute__ ((warn_unused_result));

int  control_get_log_priority     (void *data, NihDBusMessage *message,
				   char **log_priority)
	__attribute__ ((warn_unused_result));
int  control_set_log_priority     (void *data, NihDBusMessage *message,
				   const char *log_priority)
	__attribute__ ((warn_unused_result));

NIH_END_EXTERN

#endif /* INIT_CONTROL_H */
