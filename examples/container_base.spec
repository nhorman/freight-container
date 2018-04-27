%define __arch_install_post %nil 
%define _build_id_links none
%define container_packages systemd bash iproute initscripts dhclient vim passwd

%define replacepath /var/lib/freight/machines
%define ctreeroot %{name}_%{version}_%{release}

Name: container_base		
Version:	1
Release:	1%{?dist}
Summary:	Base container
Prefix:		/%{replacepath}
Group:		System/Containers
License:	GPLv2
Provides:	%{name}-%{version}-%{release}


%description
Base container on which all others are built

%install
# ===SETUP THE BASE DIRECTORIES TO HOLD THE CONTAINER FS===
%create_freight_container_dirs 


# ===INSTALL THE FILE SYSTEM TREE=== 

# Start by installing the minimum system: dnf, fedora-release, and fedora-repos
%install_base_container_fs

# This mounts the rootfs as an overlay mount, using an empty dir (none) as the
# lowerdir backing store
%activate_container_fs none

# This installs the desired packages to the container (as defined by
# container_packages), using chroot to the filesystem
%install_packages_to_container

# Fix up our selinux context as needed
%set_selinux_file_context shadow_t etc/shadow
%restorecon etc/shadow

# Set our default root password (optional)
%set_container_root_pw redhat

# This unmounts the backing store and container fs
%finalize_container_fs

# ===CREATION OF UNIT FILES===

# We need a mount unit, which is responsible for creating the 
# overlay fs mount.  For the base container the lowerdir is
# specified as an empty directory (named none).  For higher layer
# containers, the lowerdir will be the upperdir of the lower layer container
# and the upper layer container will claim the lower container mountpoint as 
# a dependency
%create_freight_mount_unit none

# This is the actual container service.  Starting this starts an instance of the
# container being installed.  Note that the service is a template, allowing
# multiple instances of the container to be created
%create_freight_service

# This is the options file for the container.  This file acts as the environment
# file for the container instance started by the service of the same name.
%create_freight_sysconf

%clean
%finalize_container_fs

%post
%systemd_post container_base.service
%systemd_post var-lib-machines-%{ctreeroot}.mount

%preun
%systemd_preun container_base.service
%systemd_preun var-lib-machines-%{ctreeroot}.mount

%postun
%systemd_postun_with_restart container_base.service
%systemd_postun_with_restart var-lib-machines-%{ctreeroot}.mount
 

%files
%dir /var/lib/machines/%{ctreeroot}
%dir /%{replacepath}/%{ctreeroot}
/%{replacepath}/%{ctreeroot}/
%{_unitdir}/*
%dir /%{_sysconfdir}/sysconfig/freight
%config /%{_sysconfdir}/sysconfig/freight/*


%changelog

