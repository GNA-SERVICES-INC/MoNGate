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

#ifndef INIT_CONF_H
#define INIT_CONF_H

#include <nih/macros.h>

#include <nih/hash.h>
#include <nih/list.h>
#include <nih/watch.h>

#include "job_class.h"


/**
 * ConfSourceType:
 *
 * We support various different types of configuration source, both solitary
 * files and directory trees.  Files within a directory tree may define any
 * single top-level type, or may be as top-level configuration files and name
 * what they define within themselves.
 **/
typedef enum conf_source_type {
	CONF_FILE,
	CONF_DIR,
	CONF_JOB_DIR,
} ConfSourceType;


/**
 * ConfSource:
 * @entry: list header,
 * @path: path to source,
 * @type: type of source,
 * @watch: NihWatch structure for automatic change notification,
 * @flag: reload flag,
 * @files: hash table of files.
 *
 * This structure represents a single source of configuration, which may be
 * a single file or a directory of files of various types, depending on
 * @type.
 *
 * Normally inotify is used to watch the source for changes and load them
 * automatically, however mandatory reloading is also supported; for this
 * the @flag member is toggled, and copied to all files reloaded;
 * any that are in the old state are deleted.
 **/
typedef struct conf_source {
	NihList             entry;
	char               *path;
	ConfSourceType      type;

	NihWatch           *watch;

	int                 flag;
	NihHash            *files;
} ConfSource;

/**
 * ConfFile:
 * @entry: list header,
 * @path: path to file,
 * @source: configuration source,
 * @flag: reload flag,
 * @data: pointer to actual item.
 *
 * This structure represents a file within @source and links to the item
 * parsed from it.
 *
 * The @flag member is used to support mandatory reloading; when the file is
 * created and parsed, it is set to the same value as the source's.  Then
 * the source can trivially see which files have been lost, since they have
 * the wrong flag value.
 **/
typedef struct conf_file {
	NihList     entry;
	char       *path;

	ConfSource *source;
	int         flag;

	union {
		void     *data;
		JobClass *job;
	};
} ConfFile;


NIH_BEGIN_EXTERN

extern NihList *conf_sources;


void        conf_init          (void);

ConfSource *conf_source_new    (const void *parent, const char *path,
				ConfSourceType type)
	__attribute__ ((warn_unused_result, malloc));
ConfFile *  conf_file_new      (ConfSource *source, const char *path)
	__attribute__ ((warn_unused_result, malloc));

void        conf_reload        (void);
int         conf_source_reload (ConfSource *source)
	__attribute__ ((warn_unused_result));

int         conf_file_destroy  (ConfFile *file);

JobClass *  conf_select_job    (const char *name);

NIH_END_EXTERN

#endif /* INIT_CONF_H */
