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
%define container_packages systemd bash iproute initscripts dhclient vim selinux-policy passwd
%freight_base_package 

%description
Base container on which all others are built

%install
# SETUP THE BASE DIRECTORIES TO HOLD THE CONTAINER FS
%create_freight_container_dirs 


# Install the file system
%install_base_container_fs
%activate_container_fs
%install_packages_to_container

# Set our default root password
%set_container_root_pw redhat

%finalize_container_fs

# CREATION OF UNIT FILES

# This is the actual container service.  Starting this starts an instance of the
# container being installed.  Note that the service is a template, allowing
# multiple instances of the container to be created
%create_freight_service

# This is the options file for the container.  This file acts as the environment
# file for the container instance started by the service of the same name.
%create_freight_option_file


%post
%systemd_post container_base.service

%preun
%systemd_preun container_base.service

%postun
%systemd_postun_with_restart container_base.service
 

%files
%dir /%{freightimagepath}/%{ctreeroot}
/%{freightimagepath}/%{ctreeroot}/
%dir /%{freightimagepath}/none/rootfs
%{_unitdir}/*
%dir /etc/systemd/nspawn
%config /etc/systemd/nspawn/*


%changelog

