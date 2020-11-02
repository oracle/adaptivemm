Name: memoptimizer
Version: 1.0
Release: 3%{?dist}
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

install -m 755 -D memoptimizer $RPM_BUILD_ROOT/%{_sbindir}/memoptimizer
install -d -m755 $RPM_BUILD_ROOT/%{_unitdir}
install -d -m755 $RPM_BUILD_ROOT/%{_presetdir}
install -m644 memoptimizer.service $RPM_BUILD_ROOT/%{_unitdir}/memoptimizer.service
install -m644 50-memoptimizer.preset $RPM_BUILD_ROOT/%{_presetdir}/50-memoptimizer.preset
install -d $RPM_BUILD_ROOT/etc/sysconfig/
install -m644 memoptimizer.cfg $RPM_BUILD_ROOT/etc/sysconfig/memoptimizer
install -d -m755 $RPM_BUILD_ROOT/%{_mandir}/man8
install memoptimizer.8 $RPM_BUILD_ROOT%{_mandir}/man8/


%post
# Initial installation
%systemd_post memoptimizer.service

%postun
%systemd_postun_with_restart memoptimizer.service

%preun
# Package removal, not upgrade
%systemd_preun memoptimizer.service


%files
%attr(0755,root,root) %{_sbindir}/memoptimizer
%license LICENSE.txt
%doc README.md CONTRIBUTING.md SECURITY.md
%attr(0644,root,root) %{_mandir}/man8/memoptimizer.8*
%attr(0640,root,root) %config(noreplace) /etc/sysconfig/memoptimizer
%attr(0644,root,root) %{_unitdir}/memoptimizer.service
%attr(0644,root,root) %{_presetdir}/50-memoptimizer.preset

%changelog
* Mon Nov 01 2020 Khalid Aziz <khalid.aziz@oracle.com> - 1.0-3
- Added more documentation and updated spec file to enable memoptimizer
  daemon using a preset file but not start the daemon

* Mon Oct 26 2020 Khalid Aziz <khalid.aziz@oracle.com> - 1.0-2
- Enable and start memoptimizer immediately after installation

* Wed Sep 02 2020 Khalid Aziz <khalid.aziz@oracle.com> - 1.0-1
- Add support for configuration file and systemd

* Tue Aug 04 2020 Khalid Aziz <khalid.aziz@oracle.com> - 0.8
- Initial release

