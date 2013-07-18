%define with_libcapng --enable-capng

Name:           bluez
BuildRequires:  pkgconfig(dbus-1)
BuildRequires:  flex
BuildRequires:  libcap-ng-devel
BuildRequires:  systemd
%{?systemd_requires}
BuildRequires:  pkgconfig(alsa)
BuildRequires:  automake
BuildRequires:  check-devel
BuildRequires:  glib2-devel >= 2.16
BuildRequires:  libsndfile-devel
BuildRequires:  libtool
BuildRequires:  libudev-devel
BuildRequires:  pkg-config
BuildRequires:  readline-devel
BuildRequires:  udev
BuildRequires:  pkgconfig(libnl-1)
BuildRequires:  libical-devel
Url:            http://www.bluez.org
Version:        5.7
Release:        0
Summary:        Bluetooth Stack for Linux
License:        GPL-2.0+
Group:          Connectivity/Bluetooth
Source:         bluez-%{version}.tar.xz
Source2:        bluez-coldplug.init
Source3:        bluetooth.sysconfig
Source4:        bluetooth.sh
Source5:        baselibs.conf
Source7:        bluetooth.modprobe
Source1001: 	bluez.manifest

%define cups_lib_dir %{_prefix}/lib/cups

%description
The Bluetooth stack for Linux.

%package devel
Summary:        Files needed for BlueZ development
License:        GPL-2.0+
Group:          Development/Libraries

%description devel
Files needed to develop applications for the BlueZ Bluetooth protocol
stack.

%package cups
Summary:        CUPS Driver for Bluetooth Printers
License:        GPL-2.0+
Group:          Connectivity/Bluetooth

%description cups
Contains the files required by CUPS for printing to Bluetooth-connected
printers.

%package -n obexd
Summary: OBEX Server A basic OBEX server implementation
Group: Applications/System

%description -n obexd
OBEX Server A basic OBEX server implementation.


%package test
Summary:        Tools for testing of various Bluetooth-functions
License:        GPL-2.0+ ; MIT
Group:          Development/Tools
Requires:       dbus-python
Requires:       python-gobject

%description test
Contains a few tools for testing various bluetooth functions. The
BLUETOOTH trademarks are owned by Bluetooth SIG, Inc., U.S.A.


%prep
%setup -q
cp %{SOURCE1001} .

%build
autoreconf -fiv
%configure  --with-pic \
			--libexecdir=/lib \
			--disable-usb		\
			--enable-tools		\
			--enable-cups		\
			--enable-test		\
			--enable-datafiles
make %{?_smp_mflags} all V=1

%check
make check

%install
%make_install

# bluez-test
rm -rvf $RPM_BUILD_ROOT/%{_libdir}/gstreamer-*
install --mode=0755 -D %{S:4} $RPM_BUILD_ROOT/usr/lib/udev/bluetooth.sh
install --mode=0644 -D %{S:7} $RPM_BUILD_ROOT/%{_sysconfdir}/modprobe.d/50-bluetooth.conf
if ! test -e %{buildroot}%{cups_lib_dir}/backend/bluetooth
then if test -e %{buildroot}%{_libdir}/cups/backend/bluetooth
     then mkdir -p %{buildroot}%{cups_lib_dir}/backend
          mv %{buildroot}%{_libdir}/cups/backend/bluetooth %{buildroot}%{cups_lib_dir}/backend/bluetooth
     fi
fi
# no idea why this is suddenly necessary...
install --mode 0755 -d $RPM_BUILD_ROOT/var/lib/bluetooth



%files
%manifest %{name}.manifest
%defattr(-, root, root)
%license COPYING
%{_bindir}/hcitool
%{_bindir}/l2ping
%{_bindir}/rfcomm
%{_bindir}/sdptool
%{_bindir}/ciptool
%{_bindir}/hciattach
%{_bindir}/hciconfig
/lib/bluetooth/bluetoothd
%{_bindir}/bccmd
%dir /usr/lib/udev
/usr/lib/udev/*
/usr/share/dbus-1/system-services/org.bluez.service
%config %{_sysconfdir}/dbus-1/system.d/bluetooth.conf
%dir /var/lib/bluetooth
%dir %{_sysconfdir}/modprobe.d
%config(noreplace) %{_sysconfdir}/modprobe.d/50-bluetooth.conf
%{_unitdir}/bluetooth.service

%files cups
%manifest %{name}.manifest
%defattr(-,root,root)
%dir %{cups_lib_dir}
%dir %{cups_lib_dir}/backend
%{cups_lib_dir}/backend/bluetooth

%files -n obexd
%defattr(-,root,root,-)
/lib/bluetooth/obexd
%{_libdir}/systemd/user/obex.service
%{_datadir}/dbus-1/services/org.bluez.obex.service

%files test
%manifest %{name}.manifest
%defattr(-,root,root)
%{_libdir}/bluez/test/*
%{_bindir}/l2test
%{_bindir}/rctest
%{_bindir}/bluetoothctl
%{_bindir}/btmon
%{_bindir}/hcidump


%docs_package

%changelog
