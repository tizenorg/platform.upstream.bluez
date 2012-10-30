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
BuildRequires:  libusb-devel
BuildRequires:  pkg-config
BuildRequires:  readline-devel
BuildRequires:  udev
BuildRequires:  pkgconfig(libnl-1)
Url:            http://www.bluez.org
Version:        4.101
Release:        0
Summary:        Bluetooth Stack for Linux
License:        GPL-2.0+
Group:          Hardware/Mobile
Source:         bluez-%{version}.tar.gz
Source2:        bluez-coldplug.init
Source3:        bluetooth.sysconfig
Source4:        bluetooth.sh
Source5:        baselibs.conf
Source7:        bluetooth.modprobe
BuildRoot:      %{_tmppath}/%{name}-%{version}-build

%define cups_lib_dir %{_prefix}/lib/cups

%description
The Bluetooth stack for Linux.

%package devel
Summary:        Files needed for BlueZ development
License:        GPL-2.0+
Group:          Development/Sources
Requires:       libbluetooth = %{version}

%description devel
Files needed to develop applications for the BlueZ Bluetooth protocol
stack.

%package -n libbluetooth
Summary:        Bluetooth Libraries
License:        GPL-2.0+
Group:          Hardware/Mobile

%description -n libbluetooth
Bluetooth protocol stack libraries.

%package cups
Summary:        CUPS Driver for Bluetooth Printers
License:        GPL-2.0+
Group:          Hardware/Printing
Requires:       libbluetooth = %{version}

%description cups
Contains the files required by CUPS for printing to Bluetooth-connected
printers.


%package test
Summary:        Tools for testing of various Bluetooth-functions
License:        GPL-2.0+ ; MIT
Group:          Development/Tools/Debuggers
Requires:       dbus-python
Requires:       libbluetooth = %{version}
Requires:       python-gobject2

%description test
Contains a few tools for testing various bluetooth functions. The
BLUETOOTH trademarks are owned by Bluetooth SIG, Inc., U.S.A.


%package sbc
Summary:        Bluetooth Low-Complexity, Sub-Band Codec Utilities
License:        GPL-2.0+
Group:          Hardware/Mobile
Requires:       libbluetooth = %{version}
Requires:       libsbc = %{version}

%description sbc
The package contains utilities for using the SBC codec.

The BLUETOOTH trademarks are owned by Bluetooth SIG, Inc., USA.



%package -n libsbc
Summary:        Bluetooth Low-Complexity, Sub-Band Codec Library
License:        GPL-2.0+
Group:          Hardware/Mobile
Requires:       libbluetooth = %{version}

%description -n libsbc
The package contains libraries for using the SBC codec.

The BLUETOOTH trademarks are owned by Bluetooth SIG, Inc., USA.

%package alsa
Summary:        Bluetooth Sound Support
License:        GPL-2.0+
Group:          Productivity/Multimedia/Sound/Utilities
Requires:       libbluetooth = %{version}
Provides:       bluez-audio:%_libdir/alsa-lib/libasound_module_pcm_bluetooth.so

%description alsa
The package contains libraries for using bluetooth audio services.

The BLUETOOTH trademarks are owned by Bluetooth SIG, Inc., USA.


%package compat
Summary:        Bluetooth Stack for Linux
License:        GPL-2.0+
Group:          Hardware/Mobile
Requires:       libbluetooth = %{version}

%description compat
The Bluetooth stack for Linux. This package contains older and partly
deprecated binaries that might still be needed for compatibility.


%prep
%setup -q

%build
autoreconf -fiv
%configure  --with-pic \
			--libexecdir=/lib \
			--enable-gstreamer	\
			--enable-alsa		\
			--enable-usb		\
			--enable-tools		\
			--enable-bccmd		\
			--enable-hid2hci	\
			--enable-dfutool	\
			--enable-cups		\
			--enable-test		\
			--enable-pand		\
			--enable-dund		\
			--enable-proximity	\
			--enable-wiimote	\
			--enable-thermometer	\
			--enable-datafiles	\
			--enable-pcmcia \
            --with-systemdunitdir=%{_unitdir} \
			%{?with_libcapng}
make %{?_smp_mflags} all V=1

%check
make check

%install
%make_install

# bluez-test
cd test
install --mode=0755	\
	simple-agent	\
	simple-service	\
	list-devices	\
	test-audio	\
	test-adapter	\
	test-device	\
	test-discovery	\
	test-input	\
	test-manager	\
	test-network	\
	test-serial	\
	test-service	\
	test-telephony	\
	$RPM_BUILD_ROOT/%{_bindir}/
cd ..
rm -rvf $RPM_BUILD_ROOT/%{_libdir}/gstreamer-*
install --mode=0755 -D %{S:4} $RPM_BUILD_ROOT/usr/lib/udev/bluetooth.sh
install --mode=0644 -D %{S:7} $RPM_BUILD_ROOT/%{_sysconfdir}/modprobe.d/50-bluetooth.conf
# In openSUSE 11.3 there is upstream compliant CUPS 1.4
# which means to have a fixed "/usr/lib/cups/" directory
# on all platforms (see Novell/Suse Bugzilla bnc#575544):
if ! test -e %{buildroot}%{cups_lib_dir}/backend/bluetooth
then if test -e %{buildroot}%{_libdir}/cups/backend/bluetooth
     then mkdir -p %{buildroot}%{cups_lib_dir}/backend
          mv %{buildroot}%{_libdir}/cups/backend/bluetooth %{buildroot}%{cups_lib_dir}/backend/bluetooth
     fi
fi
# no idea why this is suddenly necessary...
install --mode 0755 -d $RPM_BUILD_ROOT/var/lib/bluetooth





%post -n libsbc -p /sbin/ldconfig

%postun -n libsbc -p /sbin/ldconfig

%post -n libbluetooth -p /sbin/ldconfig

%postun -n libbluetooth -p /sbin/ldconfig

%files
%defattr(-, root, root)
%doc COPYING 
%{_bindir}/hcitool
%{_bindir}/l2ping
%{_bindir}/rfcomm
%{_bindir}/sdptool
%{_bindir}/ciptool
#%{_bindir}/dfutool
%{_bindir}/gatttool
%{_sbindir}/hciattach
%{_sbindir}/hciconfig
%{_sbindir}/bluetoothd
#%{_sbindir}/hid2hci
%{_sbindir}/bccmd
%dir /usr/lib/udev
/usr/lib/udev/*
/usr/lib/udev/bluetooth_serial
/usr/lib/udev/rules.d/97-bluetooth-serial.rules
/usr/share/dbus-1/system-services/org.bluez.service
%dir %{_sysconfdir}/bluetooth
%config(noreplace) %{_sysconfdir}/bluetooth/main.conf
%config(noreplace) %{_sysconfdir}/bluetooth/rfcomm.conf
%config %{_sysconfdir}/dbus-1/system.d/bluetooth.conf
%dir /var/lib/bluetooth
%dir %{_sysconfdir}/modprobe.d
%config(noreplace) %{_sysconfdir}/modprobe.d/50-bluetooth.conf
%{_unitdir}/bluetooth.service

%files devel
%defattr(-, root, root)
/usr/include/bluetooth
%{_libdir}/libbluetooth.so
%{_libdir}/pkgconfig/bluez.pc

%files -n libbluetooth
%defattr(-, root, root)
%{_libdir}/libbluetooth.so.*
%doc AUTHORS COPYING ChangeLog README

%files cups
%defattr(-,root,root)
%dir %{cups_lib_dir}
%dir %{cups_lib_dir}/backend
%{cups_lib_dir}/backend/bluetooth

%files test
%defattr(-,root,root)
%{_sbindir}/hciemu
%{_bindir}/l2test
%{_bindir}/rctest
%{_bindir}/list-devices
%{_bindir}/simple-agent
%{_bindir}/simple-service
%{_bindir}/test-adapter
%{_bindir}/test-audio
%{_bindir}/test-device
%{_bindir}/test-discovery
%{_bindir}/test-input
%{_bindir}/test-manager
%{_bindir}/test-network
%{_bindir}/test-serial
%{_bindir}/test-service
%{_bindir}/test-telephony

%files alsa
%defattr(-,root,root)
%dir /usr/share/alsa
%config /usr/share/alsa/bluetooth.conf
%{_libdir}/alsa-lib/*.so


%files compat
%defattr(-,root,root)
%{_bindir}/dund
%{_bindir}/pand

%docs_package

%changelog
