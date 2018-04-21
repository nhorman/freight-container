%define releasenum 1
%define ctreeroot %{name}_%{version}_%{releasenum}
%define replacepath /var/lib/freight/machines 
%define __arch_install_post %nil 
%define _build_id_links none
%define container_packages httpd 

Name: container_httpd	
Version:	1
Release:	%{releasenum}%{?dist}
Summary:	Httpd container
Prefix:		/%{replacepath}
Group:		System/Containers
License:	GPLv2
BuildRequires:	container_base
Requires:	container_base


%description
Httpd container image

%prep

%build

%install
# SETUP THE BASE DIRECTORIES TO HOLD THE CONTAINER FS
%create_freight_container_dirs 

# INSTALL THE ADDITIONAL PACKAGES NEEDED FOR THIS CONTAINER
systemctl start var-lib-machines-container_base_1_1.mount
mount -t overlay overlay -o lowerdir=%{replacepath}/container_base_1_1/rootfs,upperdir=$RPM_BUILD_ROOT/%{replacepath}/%{ctreeroot}/rootfs,workdir=$RPM_BUILD_ROOT/%{replacepath}/%{ctreeroot}/work $RPM_BUILD_ROOT/var/lib/machines/%{ctreeroot}

/usr/bin/dnf --noplugins -v -y --installroot=$RPM_BUILD_ROOT/var/lib/machines/%{ctreeroot}/ install %{container_packages}
/usr/bin/dnf --noplugins -v -y --installroot=$RPM_BUILD_ROOT/var/lib/machines/%{ctreeroot}/ clean all

chroot $RPM_BUILD_ROOT/var/lib/machines/%{ctreeroot}/ systemctl enable httpd.service

umount $RPM_BUILD_ROOT/var/lib/machines/%{ctreeroot}
systemctl stop var-lib-machines-container_base_1_1.mount

# CREATION OF UNIT FILES

# We need a mount unit, which is responsible for creating the 
# overlay fs mount.  For the base container the lowerdir is
# specified as an empty directory (named none).  For higher layer
# containers, the lowerdir will be the upperdir of the lower layer container
# and the upper layer container will claim the lower container mountpoint as 
# a dependency
cat <<\EOF >> $RPM_BUILD_ROOT/%{_unitdir}/var-lib-machines-%{ctreeroot}.mount
[Unit]
Description=Mount for container base
Requires=var-lib-machines-container_base_1_1.mount
After=var-lib-machines-container_base_1_1.mount

[Mount]
Type=overlay
What=overlay
Where=/var/lib/machines/%{ctreeroot}
Options=lowerdir=%{replacepath}/container_base_1_1/rootfs,upperdir=%{replacepath}/%{ctreeroot}/rootfs,workdir=%{replacepath}/%{ctreeroot}/work

EOF


# This is the actual container service.  Starting this starts an instance of the
# container being installed.  Note that the service is a template, allowing
# multiple instances of the container to be created
cat <<\EOF >> $RPM_BUILD_ROOT/%{_unitdir}/%{name}@.service

[Unit]
Description=Httpd Container for Freight
Requires=var-lib-machines-%{ctreeroot}.mount
After=var-lib-machines-%{ctreeroot}.mount


[Service]
Type=simple
EnvironmentFile=/etc/sysconfig/freight/%{ctreeroot}-%I
ExecStart=/usr/bin/systemd-nspawn -b --machine=%{name}-%I --directory=/var/lib/machines/%{ctreeroot} $MACHINE_OPTS
ExecStop=/usr/bin/systemd-nspawn poweroff %I

[Install]
Also=dbus.service
DefaultInstance=default

EOF

# This is the options file for the container.  This file acts as the environment
# file for the container instance started by the service of the same name.
cat <<\EOF >> $RPM_BUILD_ROOT/%{_sysconfdir}/sysconfig/freight/%{ctreeroot}-default
MACHINE_OPTS=""
EOF

%post
/usr/bin/dnf --noplugins -v -y --installroot=/%{replacepath}/%{ctreeroot} install %{container_packages}
/usr/bin/dnf --noplugins -v -y --installroot=/%{replacepath}/%{ctreeroot} clean all

%systemd_post %{name}.service
%systemd_post var-lib-machines-%{ctreeroot}.mount

%preun
%systemd_preun %{name}.service
%systemd_preun var-lib-machines-%{ctreeroot}.mount

%postun
%systemd_postun_with_restart %{name}.service
%systemd_postun_with_restart var-lib-machines-%{ctreeroot}.mount
rm -rf %{replacepath}/%{ctreeroot}
 

%files
%dir /var/lib/machines/%{ctreeroot}
/%{replacepath}/%{ctreeroot}/
%{_unitdir}/*
%config /%{_sysconfdir}/sysconfig/freight/*


%changelog

