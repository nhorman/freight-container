Name: container_base		
Version:	1
Release:	1%{?dist}
Summary:	Base container
Prefix:		/%{freightimagepath}
Group:		System/Containers
License:	GPLv2

# We have to define what packages this container is going to install (used with
# the install_packages_to_container_macro below), and we have to run the
# freight_package macro to mark this rpm as a container
%define container_packages systemd bash iproute initscripts dhclient vim passwd
%freight_package none 

%description
Base container on which all others are built

%install
# SETUP THE BASE DIRECTORIES TO HOLD THE CONTAINER FS
%create_freight_container_dirs 


# Install the file system
%install_base_container_fs
%activate_container_fs none
%install_packages_to_container

# Fix up our selinux context as needed
%set_selinux_file_context shadow_t etc/shadow
%restorecon etc/shadow

# Set our default root password
%set_container_root_pw redhat

%finalize_container_fs

# CREATION OF UNIT FILES

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
%create_freight_option_file

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
%dir /%{freightimagepath}/%{ctreeroot}
/%{freightimagepath}/%{ctreeroot}/
%{_unitdir}/*
%dir /etc/systemd/nspawn
%config /etc/systemd/nspawn/*


%changelog

