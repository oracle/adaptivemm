Name: adaptivemm
Version: 2.0.1
Release: 1%{?dist}
License: GPLv2
Obsoletes: memoptimizer
Summary: Adaptive free memory management
URL: https://github.com/oracle/adaptivemm
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

install -D -m 755 adaptivemmd %{buildroot}%{_sbindir}/adaptivemmd
install -D -m 644 adaptivemmd.service %{buildroot}%{_unitdir}/adaptivemmd.service
install -D -m 644 50-adaptivemm.preset %{buildroot}%{_presetdir}/50-adaptivemm.preset
install -D -m 644 adaptivemmd.cfg %{buildroot}%{_sysconfdir}/sysconfig/adaptivemmd
install -D -m 644 adaptivemmd.8 %{buildroot}%{_mandir}/man8/adaptivemmd.8

%post
%systemd_post adaptivemmd.service

%postun
%systemd_postun_with_restart adaptivemmd.service

%preun
%systemd_preun adaptivemmd.service

%files
%license LICENSE.txt
%doc README.md CONTRIBUTING.md SECURITY.md
%attr(0640,root,root) %config(noreplace) /etc/sysconfig/adaptivemmd
%{_sbindir}/adaptivemmd
%{_mandir}/man8/adaptivemmd.8*
%{_unitdir}/adaptivemmd.service
%{_presetdir}/50-adaptivemm.preset

%changelog
* Tue Feb 9 2022 Khalid Aziz <khalid.aziz@oracle.com> - 2.0.1-1
- Fix total memory calculation for ARM since it is slightly
  different from x86

* Tue Jan 25 2022 Khalid Aziz <khalid.aziz@oracle.com> - 2.0.0-1
- Changing name to adaptivemmd from memoptimizer
- Change a config option name to stop systemd from complaining
  about the name format

* Thu Jul 29 2021 Khalid Aziz <khalid.aziz@oracle.com> - 1.5.0-1
- Add support for one time initializations at start up
- Add support for tunables value updates upon change
  in number of hugepages on the system

* Mon May 17 2021 Khalid Aziz <khalid.aziz@oracle.com> - 1.4.2-1
- Fix crash on systems with less than 4G memory

* Thu Mar 25 2021 Khalid Aziz <khalid.aziz@oracle.com> - 1.4.0-1
- Various algorithm updates to reduce the possibility of OOM
  killer being invoked because of memoptimizer's actions

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
