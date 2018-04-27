%define releasenum 1
%define ctreeroot %{name}_%{version}_%{release}
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
BuildRequires:	container_base-%{version}-%{release}
Requires:	container_base-%{version}-%{release}


%description
Httpd container image

%install
# ===SETUP THE BASE DIRECTORIES TO HOLD THE CONTAINER FS===
%create_freight_container_dirs 

# ===INSTALL THE ADDITIONAL PACKAGES NEEDED FOR THIS CONTAINER===

# This mounts the parent container fs and our own container, using the parent
# container fs as the lowerdir
%activate_container_fs container_base_%{version}_%{release}

# Install packages (defined by container_packages) to the container fs
%install_packages_to_container

# Generic chroot operation to default enable the httpd service
%run_in_container systemctl enable httpd.service

# Unmount our container fs and stop the parent mount unit
%finalize_container_fs container_base_%{version}_%{release}

# ===CREATION OF UNIT FILES===

# We need a mount unit, which is responsible for creating the 
# overlay fs mount.  For the base container the lowerdir is
# specified as an empty directory (named none).  For higher layer
# containers, the lowerdir will be the upperdir of the lower layer container
# and the upper layer container will claim the lower container mountpoint as 
# a dependency
%create_freight_mount_unit container_base_%{version}_%{release}

# This is the actual container service.  Starting this starts an instance of the
# container being installed.  Note that the service is a template, allowing
# multiple instances of the container to be created
%create_freight_service

# This is the options file for the container.  This file acts as the environment
# file for the container instance started by the service of the same name.
%create_freight_sysconf

%clean
%finalize_container_fs container_base_%{version}_%{release}

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

