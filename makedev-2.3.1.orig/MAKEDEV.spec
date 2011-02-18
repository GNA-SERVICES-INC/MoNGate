Summary: Script to make and update /dev entries
Name: MAKEDEV
Version: 2.3.1
Release: 1
Copyright: none
Group: Utilities/System
Source: ftp://tsx-11.mit.edu/pub/linux/sources/sbin/MAKEDEV-2.3.1.tar.gz
Requires: /bin/sh fileutils shadow-utils >= 970616-7
Prereq: shadow-utils
# Note: pre-requires that particular version of shadow-utils, but
# RPM can't handle that right now.
BuildArchitectures: noarch

%changelog

* Fri Oct 24 1997 Michael K. Johnson <johnsonm@redhat.com>

- 2.3.1: use /usr/sbin/groupadd from new shadow utils

* Mon Sep 29 1997 Michael K. Johnson <johnsonm@redhat.com>

- Updated to 2.3, as Nick agreed to me making an interim release while
  he figures out whether he wants to be the maintainer.

* Wed Sep 03 1997 Erik Troan <ewt@redhat.com>

- made a noarch package

* Tue Jul 08 1997 Erik Troan <ewt@redhat.com>

- added dependencies

%description
The /dev tree holds special files, each of which corresponds to a type
of hardware device that Linux supports. This package contains a script
which makes it easier to create and maintain the files which fill the
/dev tree.

%prep
%setup

%build

%install
make install

%pre
/usr/sbin/groupadd -g 19 -r -f floppy

%files
/dev/MAKEDEV
/usr/man/man8/MAKEDEV.8
