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
provide a more tradtional type of management for security issues and updates.
While Docker containers allow for for easy rebuilding and some level of
versioning, they fail to integreate metadata for administrators to determine
_when_ a container is in need of an update.  Freight seeks to enable
administrators to determine the need for contanier updates by building
containers out of existing package repositories, and including their
installation metadata.  By doing so, administrators can easily use existing
tools like yum to see if updated packages are needed inside a container.
Further, since the container itself is packaged as an rpm file, it can be
distributed for update using well tested and established tools and mechainsms.

# How it works
Freight uses robust existing technology to provide containerization of
applications.  Currently this is preformed using rpm (though other packaging
technologies can easily be added).  The transform path for building a freight
container looks like this:

    <freight manifest file>
    		|
    		|
         freight-builder
    		|
    		|
    		v
        <freight srpm>
    		|
    		|
      rpmbuild or freight-builder
    		|
    		|
    		v
      <binary container rpm>

The resultant binary rpm will contain a filesystem tree that can be installed in
any location using the command:

rpm --root /path/to/directory


Once installed, it can be used as a container via this command:

systemd-nspawn -D /path/to/directory <cmd>

or

systemd-nspawn -D /path/to/directory -b

if the container has had systemd installed to itself (see the systemd-nspawn man
page for details).

This method of container packaging is adventageous as it provides several
features:

* Versioning - the manifest file can be updated and the container rebuilt easily
  with a new version/release to update for security features
* Update detection - because the rpm database is preserved during the
  container build in the image, it can be introspected and compared to the
  repositories from which it was built to detect the need for updates
* Distribution - containers can be distributed using established technologies,
  the same that yum repositories use.
* Multi-arch support - The src rpm can be built on multiple arches so binaries
  for different architectures can be maintained and distributed just like
  standard packages are 



# Build and Installation

* Clone the repository
* % aclocal
* % autoheader
* % automake --add-missing
* % autoconf
* % ./configure [options]
* % make

You can skip the steps prior to the confgure command by running autogen.sh




