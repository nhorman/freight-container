# freight-tools
A utility suite to construct disk images suitable for use with systemd-nspawn based on stackable manifest files 

# About
Docker is all the rage these days, but systemd offers its own set of container
management infrastructure, which is both robust and flexible.  Given that
systemd is part of several major distributionss, it would be nice to take more
advantage of these systemd tools.  Freight-tools seeks to provide some
distributed management of systemd containers in a way that the kubernetes
project does with docker.  It also seeks to provide some additional features
above and beyond what docker and kubernetes provide.  Most notably it seeks to
provide better management for security issues and updates.  While any container
can typically be rebuilt fairly easily, Docker containers contain very little
infrastructure to tell administrators when a given container instance is
effected by a security issues.  Freight seeks to provide that visibility to an
administrator.



# Build and Installation

* Clone the repository
* % aclocal
* % autoheader
* % automake --add-missing
* % autoconf
* % ./configure [options]
* % make

You can skip the steps prior to the confgure command by running autogen.sh




