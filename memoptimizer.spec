Name: memoptimizer
Version: 1.3.0
Release: 1%{?dist}
License: GPLv2
Summary: Free memory optimizer
URL: https://github.com/oracle/memoptimizer
Source0: %{name}-%{version}.tar.gz

BuildRequires: glib2-devel
BuildRequires: pkgconfig
BuildRequires: gcc
BuildRequires: make

Requires(post): systemd-units
Requires(preun): systemd-units
Requires(postun): systemd-units

%description
%{name} monitors the state of free pages by reading kernel provided
files under /proc and /sys. Based upon the current rate of free page
consumption and memory fragmentation, it predicts if the system is
likely to run out of memory, or if memory will become severely
fragmented in the near future. If so, it adjusts watermarks to force
memory reclamation. If memory is predicted to become severely
fragmented, it triggers compaction in the kernel. The goal is to avert
memory shortage and/or fragmentation by taking proactive measures.

%prep
%setup -q

%build
make %{?_smp_mflags}

%install

install -D -m 755 memoptimizer %{buildroot}%{_sbindir}/memoptimizer
install -D -m 644 memoptimizer.service %{buildroot}%{_unitdir}/memoptimizer.service
install -D -m 644 50-memoptimizer.preset %{buildroot}%{_presetdir}/50-memoptimizer.preset
install -D -m 644 memoptimizer.cfg %{buildroot}%{_sysconfdir}/sysconfig/memoptimizer
install -D -m 644 memoptimizer.8 %{buildroot}%{_mandir}/man8/memoptimizer.8

%post
%systemd_post memoptimizer.service

%postun
%systemd_postun_with_restart memoptimizer.service

%preun
%systemd_preun memoptimizer.service

%files
%license LICENSE.txt
%doc README.md CONTRIBUTING.md SECURITY.md
%attr(0640,root,root) %config(noreplace) /etc/sysconfig/memoptimizer
%{_sbindir}/memoptimizer
%{_mandir}/man8/memoptimizer.8*
%{_unitdir}/memoptimizer.service
%{_presetdir}/50-memoptimizer.preset

%changelog
* Thu Feb 11 2021 Khalid Aziz <khalid.aziz@oracle.com> - 1.3.0-1
- New upstream release. Various enhancements to reduce system
  resource usage by the daemon. Fix to correct the sense of
  aggressiveness level and documentation update

* Fri Jan 22 2021 Khalid Aziz <khalid.aziz@oracle.com> - 1.2.0-1
- New upstream release. Fixes bug with not reading configuration file

* Thu Jan 21 2021 Khalid Aziz <khalid.aziz@oracle.com> - 1.1.0-1
- New upstream release

* Tue Jan 12 2021 Khalid Aziz <khalid.aziz@oracle.com> - 1.0.5-1
- Updated spec file to conform to fedora packaging guidelines

* Wed Dec 23 2020 Khalid Aziz <khalid.aziz@oracle.com> - 1.0-4
- Fixed a formatting error in man page

* Mon Nov 2 2020 Khalid Aziz <khalid.aziz@oracle.com> - 1.0-3
- Added more documentation and updated spec file to enable memoptimizer
  daemon using a preset file but not start the daemon

* Mon Oct 26 2020 Khalid Aziz <khalid.aziz@oracle.com> - 1.0-2
- Enable and start memoptimizer immediately after installation

* Wed Sep 02 2020 Khalid Aziz <khalid.aziz@oracle.com> - 1.0-1
- Add support for configuration file and systemd

* Tue Aug 04 2020 Khalid Aziz <khalid.aziz@oracle.com> - 0.8
- Initial release
