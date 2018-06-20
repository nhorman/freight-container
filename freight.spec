%global shortcommit	%(c=%{githash}; echo ${c:0:7})

Name: freight
Version: 0
Release: 0.2.20180613git%{shortcommit}%{?dist}
Summary: RPM macro set and commands for creating containers using rpm-build/mock
BuildArch: noarch

License: GPLv3
URL: https://github.com/nhorman/freight
Source0: %url/archive/%{githash}/freight-%{shortcommit}.tar.gz

Requires: rpm rpm-build mock bash dnf	

%description
Freight is a small container creation utility that allows you to build container
filesystems that use Systemd as a control mechanism and rpm as a container
package format.

%prep
%autosetup -n %{name}-%{githash}

%build
# No op - no building to be done

%install
mkdir -p %{buildroot}%{rpmmacrodir}
mkdir -p %{buildroot}/usr/bin
mkdir -p %{buildroot}/%{_datadir}/freight/examples/specs/
mkdir -p %{buildroot}/%{_datadir}/freight/examples/mock/

install -m 0644 rpm_macros/macros.freight %{buildroot}%{rpmmacrodir}
install -m 0755 scripts/freight-cmd %{buildroot}/usr/bin/
install -m 0644 examples/specs/* %{buildroot}/%{_datadir}/freight/examples/specs
install -m 0644 examples/mock/* %{buildroot}/%{_datadir}/freight/examples/mock

%files
%doc README.md
%license COPYING
%{_bindir}/freight-cmd
%dir %{_datadir}/freight
%dir %{_datadir}/freight/examples
%{_datadir}/freight/examples/*
%{rpmmacrodir}/macros.freight



%changelog
* Tue Jun 19 2018 Neil Horman <nhorman@tuxdriver.com - 0-0.1.20180613gitc72ed55
- Update spec file in response to review comments

* Wed Jun 13 2018 Neil Horman <nhorman@tuxdriver.com> - 0-0.2.20180613gitc72ed55 
- initial creation
