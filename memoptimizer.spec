Name: memoptimizer
Version: 1.0
Release: 1%{?dist}
License: GPLv2
Group: Applications/System
Summary: Free memory optimizer
Source0: %{name}-%{version}.tar.xz

Requires(post): systemd-units
Requires(preun): systemd-units
Requires(postun): systemd-units
Requires(pre): coreutils
BuildRequires: glib2-devel
BuildRequires: pkgconfig
BuildRequires: systemd-units


#START INSERT

%description
memoptimizer monitors the state of free pages by reading kernel
provided files under /proc/and /sys. Based upon current rate of
free pages consumption and memory fragmentation, it predicts if
system is likely to run out of memory or if memory will become
severely fragmented in near future. If so, it adjusts watermarks
to force memory reclamation if system is about to run out of
memory. If memory is precited to become severely fragmented, it
triggers comapction in the kernel. The goal is to avert memory
shortage and/or fragmentation by taking proactive measures.

%prep
%setup -q 


%build
export CFLAGS="-O2 -g -pipe -Wall -Wp,-D_FORTIFY_SOURCE=2"
make

%install

install -m 700 -D memoptimizer $RPM_BUILD_ROOT/sbin/memoptimizer
install -d -m755 $RPM_BUILD_ROOT/%{_unitdir}
install -m644 memoptimizer.service $RPM_BUILD_ROOT/%{_unitdir}/memoptimizer.service
install -d $RPM_BUILD_ROOT/etc/sysconfig/
install -m644 memoptimizer.cfg $RPM_BUILD_ROOT/etc/sysconfig/memoptimizer
install -d -m755 $RPM_BUILD_ROOT/%{_mandir}/man8
install memoptimizer.8 $RPM_BUILD_ROOT%{_mandir}/man8/


%post server
# Initial installation
%systemd_post memoptimizer.service

%postun server
%systemd_postun_with_restart memoptimizer.service

%preun server
# Package removal, not upgrade
%systemd_preun memoptimizer.service


%files
/sbin/memoptimizer
%license LICENSE
%doc README
%attr(0644,root,root) %{_mandir}/man8/memoptimizer.8*
%attr(0640,root,root) %config(noreplace) /etc/sysconfig/memoptimizer
%attr(0644,root,root) %{_unitdir}/memoptimizer.service

%changelog
* Wed Sep 02 2020 Khalid Aziz <khalid.aziz@oracle.com> - 1.0
- Add support for configuration file and systemd

* Tue Aug 04 2020 Khalid Aziz <khalid.aziz@oracle.com> - 0.8
- Initial release

