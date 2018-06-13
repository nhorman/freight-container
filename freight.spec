Name: freight		
Version: 0.1
Release: 1%{?dist}
Summary: freight is an macro set for creating containers using rpmbuild/mock

Group: Development/Tools
License: GPLv3
URL: https://github.com/nhorman/freight
Source0: freight-0.1.tar.gz

Requires: rpm rpm-build mock bash dnf	

%description
freight is a small container creation utility that allows you to build container
filesystems that use systemd as a control mechanism and rpm as a container
package format.

%prep
%setup -q


%install
mkdir -p %{buildroot}/usr/lib/rpm/macros.d
mkdir -p %{buildroot}/usr/bin
mkdir -p %{buildroot}/%{_datadir}/freight/examples

install -m 0644 rpm_macros/macros.freight %{buildroot}/usr/lib/rpm/macros.d/
install -m 0755 scripts/freight-cmd %{buildroot}/usr/bin/
install -m 0644 examples/* %{buildroot}/%{_datadir}/freight/examples
%files
%doc README.md COPYING
%{_bindir}/freight-cmd
%dir %{_datadir}/freight
%dir %{_datadir}/freight/examples
%{_datadir}/freight/examples/*
/usr/lib/rpm/macros.d/macros.freight



%changelog
* Wed Jun 13 2018 Neil Horman <nhorman@tuxdriver.com> - 0.1-1
- initial creation
