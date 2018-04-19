%define releasenum 1
%define ctreeroot %{name}_%{version}_%{releasenum}
%define replacepath /var/lib/freight/machines 
%define __arch_install_post %nil 
%define container_packages systemd fedora-release iproute initscripts dhclient vim passwd

Name: container_base		
Version:	1
Release:	%{releasenum}%{?dist}
Summary:	Base container
Prefix:		/%{replacepath}
Group:		System/Containers
License:	GPLv2
BuildArch:	noarch



%description
Base container on which all others are built

%prep

%build

%install
# SETUP THE BASE DIRECTORIES TO HOLD THE CONTAINER FS
%create_freight_container_dirs 

# CREATE THE SETUP SCRIPT
# Every freight container has a setup script which is responsible
# for installing the rpms needed to implement this container
# This script is run from the %{ctreename}_setup service
cat <<\EOF >> $RPM_BUILD_ROOT/%{replacepath}/%{ctreeroot}/setup.sh
#!/bin/sh
echo 'hello' | systemd-cat -p warning
if [ -d /var/lib/machines/%{ctreeroot}/etc ]
then
	exit 0
fi

/usr/bin/dnf --noplugins -v -y --installroot=/var/lib/machines/%{ctreeroot} --releasever %{fedora} install %{container_packages}
/usr/bin/dnf --noplugins -v -y --installroot=/var/lib/machines/%{ctreeroot} clean all
/usr/sbin/chroot /var/lib/machines/%{ctreeroot} /bin/sh -c "echo redhat | passwd --stdin root"

exit 0
EOF

#make sure to mark the setup script as executable
chmod 755 $RPM_BUILD_ROOT/%{replacepath}/%{ctreeroot}/setup.sh

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

# We need a setup service, this is a one shot service that 'creates' the
# container, by running the setup script above.
cat <<\EOF >> $RPM_BUILD_ROOT/%{_unitdir}/container_base_setup.service
[Unit]
Description=Base Container Setup for Freight
Requires=var-lib-machines-%{ctreeroot}.mount
After=var-lib-machines-%{ctreeroot}.mount

[Service]
Type=oneshot
TimeoutStartSec=240
RemainAfterExit=true
ExecStart=%{replacepath}/%{ctreeroot}/setup.sh

EOF


# This is the actual container service.  Starting this starts an instance of the
# container being installed.  Note that the service is a template, allowing
# multiple instances of the container to be created
cat <<\EOF >> $RPM_BUILD_ROOT/%{_unitdir}/container_base@.service

[Unit]
Description=Base Container for Freight
Requires=var-lib-machines-%{ctreeroot}.mount
Requires=container_base_setup.service
After=container_base_setup.service


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
%dir /var/lib/machines/%{ctreeroot}
/%{replacepath}/%{ctreeroot}/
%{_unitdir}/*
%dir /%{_sysconfdir}/sysconfig/freight
%config /%{_sysconfdir}/sysconfig/freight/*


%changelog

