%define prefix	/usr

Summary: ejects removable media and controls auto ejection
Name: eject
Version: 2.1.5
%define src %{name}-%{version}
Release: 2XL
Copyright: GPL
Group: Utilities/System
Source: www.ibiblio.org:/pub/Linux/utils/disk-management/%{src}.tar.gz
patch: %{src}.patch.gz
BuildRoot: /var/tmp/eject-root

%description
This program allows the user to eject removable like
CD-ROMs, Jaz and Zip drives, and floppy drives on SPARC machines.

%prep
set |grep RPM
if [ -z $RPM_BUILD_ROOT ]; then echo -e "\7ROOT is blank !"; sleep 1; break ;fi
%setup
%patch -p1
%build
./configure --prefix=%{prefix}
chown root.root . -R
make RPM_OPT_FLAGS="$RPM_OPT_FLAGS"

%install
if [ -d  $RPM_BUILD_ROOT ];then
   rm -rf $RPM_BUILD_ROOT
fi
#make prefix=$RPM_BUILD_ROOT/%{prefix} install
make ROOTDIR=$RPM_BUILD_ROOT install
cd $RPM_BUILD_ROOT/
find . -type f -links 1|grep \/man\/man  |xargs gzip


%files
%doc README COPYING ChangeLog INSTALL PORTING TODO

/usr/bin/*
/usr/man/man1/*
/usr/share/locale/*/*/eject.mo

%clean
rm -rf $RPM_BUILD_ROOT
rm -rf $RPM_BUILD_DIR/%{src}
rm -f  /var/tmp/rpm-tmp.*


%changelog
* Thu Jun 07 2001 Kun-Chung Hsieh <linuxer@coventive.com>
- adopt/update/i18nized  09:42:33

