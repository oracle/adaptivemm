Name: memoptimizer
Version: 0.8
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


%post
# Initial installation
%systemd_post memoptimizer.service

touch /etc/default/memoptimizer.conf

%postun
%systemd_postun_with_restart memoptimizer.service

%preun
# Package removal, not upgrade
%systemd_preun memoptimizer.service


%files
/sbin/memoptimizer
%doc LICENSE
%doc README

%changelog
* Tue Aug 04 2020 Khalid Aziz <khalid.aziz@oracle.com> - 0.8
- Initial release

