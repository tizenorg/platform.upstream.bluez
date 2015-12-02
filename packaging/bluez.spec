#%define with_libcapng --enable-capng
%define _libpath /usr/lib
Name:       	bluez
Summary:    	Bluetooth Stack for Linux
Version:    	5.28
Release:    	0
Group:      	Network & Connectivity/Bluetooth
License:    	GPL-2.0+
URL:        	http://www.bluez.org/
Source:         bluez-%{version}.tar.gz
Source2:        bluez-coldplug.init
Source3:        bluetooth.sysconfig
Source4:        bluetooth.sh
Source5:        baselibs.conf
Source7:        bluetooth.modprobe
Source101:    	obex-root-setup
Source102:    	create-symlinks
Source103:      obex.sh
Source1001:     bluez.manifest
#Patch1 :    bluez-ncurses.patch
#Patch2 :    disable-eir-unittest.patch
#Requires:   dbus >= 0.60
#BuildRequires:  pkgconfig(libudev)
BuildRequires:  pkgconfig(dbus-1)
#BuildRequires:  pkgconfig(glib-2.0)
#BuildRequires:  pkgconfig(ncurses)
#BuildRequires:  flex
#BuildRequires:  bison
#BuildRequires:  readline-devel
#BuildRequires:  openssl-devel
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
BuildRequires:  libusb-devel
BuildRequires:  pkg-config
BuildRequires:  readline-devel
BuildRequires:  udev
BuildRequires:  pkgconfig(libnl-1)
BuildRequires:  libical-devel
BuildRequires:  pkgconfig(libtzplatform-config)

%description
The Bluetooth stack for Linux.

%package devel
Summary:        Files needed for BlueZ development
License:        GPL-2.0+
Group:          Development/Libraries
Requires:       libbluetooth = %{version}

%description devel
Files needed to develop applications for the BlueZ Bluetooth protocol
stack.

%package -n libbluetooth
Summary:        Bluetooth Libraries
License:        GPL-2.0+
Group:          Network & Connectivity/Bluetooth

%description -n libbluetooth
Bluetooth protocol stack libraries.

%package -n obexd
Summary:        OBEX Server A basic OBEX server implementation
Group:          Network & Connectivity/Bluetooth
Requires:       tizen-platform-config-tools

%description -n obexd
OBEX Server A basic OBEX server implementation.

%package test
Summary:        Tools for testing of various Bluetooth-functions
License:        GPL-2.0+ and MIT
Group:          Development/Tools
Requires:       dbus-python
Requires:       libbluetooth = %{version}
Requires:       python-gobject

%description test
Contains a few tools for testing various bluetooth functions. The
BLUETOOTH trademarks are owned by Bluetooth SIG, Inc., U.S.A.

%prep
%setup -q
cp %{SOURCE1001} .

%build
autoreconf -fiv

export CFLAGS="${CFLAGS} -D__TIZEN_PATCH__ -DBLUEZ5_27_GATT_CLIENT"

export LDFLAGS=" -lncurses -Wl,--as-needed "
export CFLAGS+=" -DPBAP_SIM_ENABLE"
%reconfigure --disable-static \
			--sysconfdir=%{_sysconfdir} \
			--localstatedir=%{_localstatedir} \
			--with-systemdsystemunitdir=%{_libpath}/systemd/system \
			--with-systemduserunitdir=%{_libpath}/systemd/user \
			--libexecdir=%{_libexecdir} \
			--enable-debug \
			--enable-pie \
			--enable-serial \
			--enable-input \
			--enable-usb=no \
			--enable-tools \
			--disable-bccmd \
			--enable-pcmcia=no \
			--enable-hid2hci=no \
			--enable-alsa=no \
			--enable-gstreamer=no \
			--disable-dfutool \
			--disable-cups \
			--enable-health=yes \
			--enable-dbusoob \
			--enable-test \
			--with-telephony=tizen \
			--enable-obex \
			--enable-library \
			--enable-gatt \
			--enable-experimental \
			--enable-autopair=no \
			--enable-network \
			--enable-hid=yes \
			--enable-tizenunusedplugin=no


make %{?_smp_mflags} all V=1

%check
make check

%install
%make_install

# bluez-test
rm -rvf $RPM_BUILD_ROOT/%{_libdir}/gstreamer-*
install --mode=0755 -D %{S:4} $RPM_BUILD_ROOT/usr/lib/udev/bluetooth.sh
install --mode=0644 -D %{S:7} $RPM_BUILD_ROOT/%{_sysconfdir}/modprobe.d/50-bluetooth.conf

# no idea why this is suddenly necessary...
install --mode 0755 -d $RPM_BUILD_ROOT/var/lib/bluetooth


#install -D -m 0644 src/bluetooth.conf %{buildroot}%{_sysconfdir}/dbus-1/system.d/bluetooth.conf
#install -D -m 0644 profiles/audio/audio.conf %{buildroot}%{_sysconfdir}/bluetooth/audio.conf
#install -D -m 0644 profiles/network/network.conf %{buildroot}%{_sysconfdir}/bluetooth/network.conf

#install -D -m 0644 COPYING %{buildroot}%{_datadir}/license/bluez
#install -D -m 0644 COPYING %{buildroot}%{_datadir}/license/libbluetooth3
#install -D -m 0644 COPYING %{buildroot}%{_datadir}/license/libbluetooth-devel

install -D -m 0755 %SOURCE101 %{buildroot}%{_bindir}/obex-root-setup
install -D -m 0755 %SOURCE102 %{buildroot}%{_sysconfdir}/obex/root-setup.d/000_create-symlinks
install -D -m 0755 %SOURCE103 %{buildroot}%{_bindir}/obex.sh
install -D -m 0755 tools/btiotest $RPM_BUILD_ROOT/%{_bindir}/
install -D -m 0755 tools/bluetooth-player $RPM_BUILD_ROOT/%{_bindir}/
#install -D -m 0755 tools/mpris-player $RPM_BUILD_ROOT/%{_bindir}/
install -D -m 0755 tools/btmgmt $RPM_BUILD_ROOT/%{_bindir}/
install -D -m 0755 tools/scotest $RPM_BUILD_ROOT/%{_bindir}/
install -D -m 0755 tools/bluemoon $RPM_BUILD_ROOT/%{_bindir}/
install -D -m 0755 attrib/gatttool $RPM_BUILD_ROOT/%{_bindir}/


install -D -m 0755 tools/obexctl %{buildroot}%{_bindir}/obexctl

#test
ln -sf bluetooth.service %{buildroot}%{_libpath}/systemd/system/dbus-org.bluez.service

%post -n libbluetooth -p /sbin/ldconfig

%postun -n libbluetooth -p /sbin/ldconfig

%files
%manifest %{name}.manifest
%defattr(-, root, root)
%license COPYING
#%{_sysconfdir}/bluetooth/audio.conf
#%{_sysconfdir}/bluetooth/main.conf
#%{_sysconfdir}/bluetooth/network.conf
#%{_sysconfdir}/bluetooth/rfcomm.conf
#%{_sysconfdir}/dbus-1/system.d/bluetooth.conf
#%{_datadir}/man/*/*
%{_bindir}/hcitool
%{_bindir}/l2ping
%{_bindir}/obexctl
%{_bindir}/rfcomm
%{_bindir}/mpris-proxy
%{_bindir}/sdptool
%{_bindir}/ciptool
#%{_bindir}/dfutool
%{_bindir}/hciattach
%{_bindir}/hciconfig
%{_libexecdir}/bluetooth/bluetoothd
%{_bindir}/bccmd
#%{_sbindir}/hid2hci
%dir /usr/lib/udev
/usr/lib/udev/*

#test -2
%{_libpath}/systemd/system/bluetooth.service
%{_libpath}/systemd/system/dbus-org.bluez.service

%{_datadir}/dbus-1/system-services/org.bluez.service
%config %{_sysconfdir}/dbus-1/system.d/bluetooth.conf
%dir /var/lib/bluetooth
%dir %{_sysconfdir}/modprobe.d
%config(noreplace) %{_sysconfdir}/modprobe.d/50-bluetooth.conf




%files devel
%manifest %{name}.manifest
%defattr(-, root, root)
/usr/include/bluetooth
%{_libdir}/libbluetooth.so
%{_libdir}/pkgconfig/bluez.pc

%files -n libbluetooth
%manifest %{name}.manifest
%defattr(-, root, root)
%{_libdir}/libbluetooth.so.*
%license COPYING

%files -n obexd
%defattr(-,root,root,-)
%{_libexecdir}/bluetooth/obexd
%{_libpath}/systemd/user/obex.service
%{_datadir}/dbus-1/services/org.bluez.obex.service
%{_sysconfdir}/obex/root-setup.d/000_create-symlinks
%{_bindir}/obex-root-setup
%{_bindir}/obex.sh


%files test
%manifest %{name}.manifest
%defattr(-,root,root)
%{_libdir}/bluez/test/*
%{_bindir}/l2test
%{_bindir}/rctest
%{_bindir}/bluetoothctl
%{_bindir}/btiotest
#%{_bindir}/mpris-player
%{_bindir}/bluetooth-player
%{_bindir}/btmon
%{_bindir}/hcidump
%{_bindir}/btmgmt
%{_bindir}/scotest
%{_bindir}/bluemoon
%{_bindir}/gatttool
%{_bindir}/hex2hcd
%exclude /usr/lib/debug/*

%docs_package

%changelog
