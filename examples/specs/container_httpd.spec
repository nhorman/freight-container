Name: container_httpd	
Version:	1
Release:	1%{?dist}
Summary:	Httpd container
Prefix:		/%{freightimagepath}
Group:		System/Containers
License:	GPLv2


# Packages we will install
%define container_packages httpd 

# Mark this as a freight container, specifying the parent
%freight_package container_base_1_1%{?dist}

%description
Httpd container image

%install
# SETUP THE BASE DIRECTORIES TO HOLD THE CONTAINER FS
%create_freight_container_dirs 

# INSTALL THE ADDITIONAL PACKAGES NEEDED FOR THIS CONTAINER
%activate_container_fs

%install_packages_to_container

%run_in_container systemctl enable httpd.service

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
%systemd_post %{name}.service

%preun
%systemd_preun %{name}.service

%postun
%systemd_postun_with_restart %{name}.service
rm -rf %{freightimagepath}/%{ctreeroot}
 

%files
/%{freightimagepath}/%{ctreeroot}/
%{_unitdir}/*
%config /etc/systemd/nspawn/*


%changelog

