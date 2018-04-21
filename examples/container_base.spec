%define releasenum 1
%define ctreeroot %{name}_%{version}_%{releasenum}
%define replacepath /var/lib/freight/machines 
%define __arch_install_post %nil 
%define _build_id_links none
%define container_packages systemd dnf fedora-repos fedora-release strace bash fedora-release iproute initscripts dhclient vim passwd

Name: container_base		
Version:	1
Release:	%{releasenum}%{?dist}
Summary:	Base container
Prefix:		/%{replacepath}
Group:		System/Containers
License:	GPLv2



%description
Base container on which all others are built

%prep

%build

%install
# SETUP THE BASE DIRECTORIES TO HOLD THE CONTAINER FS
%create_freight_container_dirs 


# install the file system
/usr/bin/dnf --noplugins -v -y --installroot=$RPM_BUILD_ROOT/%{replacepath}/%{ctreeroot}/rootfs/ --releasever %{fedora} install %{container_packages}
/usr/bin/dnf --noplugins -v -y --installroot=$RPM_BUILD_ROOT/%{replacepath}/%{ctreeroot}/rootfs/ clean all 
/usr/sbin/semanage fcontext --modify -t shadow_t "$RPM_BUILD_ROOT/%{replacepath}/%{ctreeroot}/rootfs/etc/shadow"
/usr/sbin/restorecon -v "$RPM_BUILD_ROOT/%{replacepath}/%{ctreeroot}/rootfs/etc/shadow"
/usr/sbin/chroot $RPM_BUILD_ROOT/%{replacepath}/%{ctreeroot}/rootfs/ /bin/sh -c "echo redhat | passwd --stdin root"

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

[Mount]
Type=overlay
What=overlay
Where=/var/lib/machines/%{ctreeroot}
Options=lowerdir=%{replacepath}/%{ctreeroot}/none,upperdir=%{replacepath}/%{ctreeroot}/rootfs,workdir=%{replacepath}/%{ctreeroot}/work

EOF

# This is the actual container service.  Starting this starts an instance of the
# container being installed.  Note that the service is a template, allowing
# multiple instances of the container to be created
cat <<\EOF >> $RPM_BUILD_ROOT/%{_unitdir}/container_base@.service

[Unit]
Description=Base Container for Freight
Requires=var-lib-machines-%{ctreeroot}.mount


[Service]
Type=simple
EnvironmentFile=/etc/sysconfig/freight/%{ctreeroot}-%I
ExecStart=/usr/bin/systemd-nspawn -b --machine=%I --directory=/var/lib/machines/%{ctreeroot} $MACHINE_OPTS
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
%systemd_post container_base.service
%systemd_post var-lib-machines-%{ctreeroot}.mount

%preun
%systemd_preun container_base.service
%systemd_preun var-lib-machines-%{ctreeroot}.mount

%postun
%systemd_postun_with_restart container_base.service
%systemd_postun_with_restart var-lib-machines-%{ctreeroot}.mount
rm -rf %{replacepath}/%{ctreeroot}
 

%files
%dir /%{replacepath}/%{ctreeroot}
/%{replacepath}/%{ctreeroot}/
%{_unitdir}/*
%dir /%{_sysconfdir}/sysconfig/freight
%config /%{_sysconfdir}/sysconfig/freight/*


%changelog

