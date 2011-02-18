/*
 * Reads and displays CD-ROM volume name
 *
 * Several people have asked how to read CD volume names so I wrote this
 * small program to do it.
 *
 * usage: volname [<device-file>]
 *
 ********************************************************************
 *
 * Copyright (C) 2000-2005 Jeff Tranter (tranter@pobox.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 ********************************************************************
 *
 */
#include "i18n.h"

#ifndef DEFAULTDEVICE
#error DEFAULTDEVICE not set, check Makefile
#endif

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
  int fd;
  int status;
  char *device;
  char buffer[33];

  I18NCODE

  if (argc == 2) {
    device = argv[1];
  } else if (argc == 1) {
    device = "/dev/" DEFAULTDEVICE;
  } else {
    fprintf(stderr, _("usage: volname [<device-file>]\n"));
    exit(1);
  }

  fd = open(device, O_RDONLY);
  if (fd == -1) {
    perror(_("volname"));
    exit(1);
  }

  status = lseek(fd, 32808, SEEK_SET);
  if (status == -1) {
    perror(_("volname"));
    exit(1);
  }

  status = read(fd, buffer, 32);
  if (status == -1) {
    perror(_("volname"));
    exit(1);
  }

  printf("%32.32s\n", buffer);

  return 0;
}
