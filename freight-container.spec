%global shortcommit	%(c=%{githash}; echo ${c:0:7})

Name: freight-container
Version: 0
Release: 0.2.20180613git%{shortcommit}%{?dist}
Summary: RPM macro set and commands for creating containers using rpm-build/mock
BuildArch: noarch

License: GPLv3
URL: https://github.com/nhorman/freight
Source0: %url/archive/%{githash}/%{name}-%{shortcommit}.tar.gz

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
mkdir -p %{buildroot}/%{_datadir}/%{name}/examples/specs/
mkdir -p %{buildroot}/%{_datadir}/%{name}/examples/mock/
mkdir -p %{buildroot}/%{_mandir}/man1/

install -m 0644 rpm_macros/macros.freight %{buildroot}%{rpmmacrodir}
install -m 0755 scripts/freight-cmd %{buildroot}/usr/bin/
install -m 0644 examples/specs/* %{buildroot}/%{_datadir}/%{name}/examples/specs
install -m 0644 examples/mock/* %{buildroot}/%{_datadir}/%{name}/examples/mock
install -m 0644 doc/freight-cmd.1 %{buildroot}/%{_mandir}/man1/

%files
%doc README.md
%license COPYING
%{_bindir}/freight-cmd
%dir %{_datadir}/%{name}
%dir %{_datadir}/%{name}/examples
%{_datadir}/%{name}/examples/*
%{_mandir}/man1/*
%{rpmmacrodir}/macros.freight



%changelog
* Mon Jun 25 2018 Neil Horman <nhorman@tuxdriver.com - 0-0.3.20180625git
* Tue Jun 19 2018 Neil Horman <nhorman@tuxdriver.com - 0-0.2.20180619git
- Update spec file in response to review comments

* Wed Jun 13 2018 Neil Horman <nhorman@tuxdriver.com> - 0-0.1.20180613git 
- initial creation
