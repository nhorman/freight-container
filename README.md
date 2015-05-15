# freight-tools
A utility suite to construct disk images suitable for use with systemd-nspawn based on stackable manifest files 

# About
Freight is a system for creating and managing containers using existing
packaging tools and distribution mechanisms.  Freight focuses on using a
distributions existing infrastructure tools to create, distribute and manage
containers on host systems across a site.  Doing so enables reuse of
administrative skills and distribution features to create a robust, flexible and
secure managed container environment.

# Key features

## Distribution based container creation
Freight uses well established packaging mechanisms (currently rpm/yum, but
others can easily be added).  To transform a manifest of standard packages into
a container file system.  This has the advantage of ensuring that the package
manifest is included in the container image so that it can be administratively
interrogated later using those same tools (most notably to determine the need
for package updates)

## Distribution based container packaging
Containerized file systems are then packaged into their own rpm, which provides
several advantages:
* Reuse of existing distribution mechanisms (yum repositories)
* Clear, well understood versioning that is human readable
* Ease of updates on remote container hosts

## Update detection
A key tenet of distribution packaging is the ability to identify the need to
update a package on a system (for security fixes/enhancements/etc).  By reusing
those distribution mechanisms, the same advantages can be applied to containers.
Freight allows a container rpm to be installed temporarily and inspected using
yum so that the need for updates can be reported to an administrator, and
appropriate action can be taken

## Manifest Level inheritance
Containers are generally considered to be immutable.  This gives rise to the
need to create layered containers in order to address the need for customization
at a given site.  While layering (via unionfs or overlayfs) provides an elegant
solution to the problem, it also creates an issue with aliasing, in which the
contents of a base layer image may be masked by a higher layer image, leading to
potential security holes.  Freight addresses this by preforming inheritance at
the manifest level.  By using distribution packaging dependency resolution and
versioning, even inherited/layered lists of packages can be resolved to a single
set of packages at their latest versions, ensuing that you know what will be
executed in a container.

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

Freight also has tools to support clustered container management:

Freight-agent acts as a front end to to container rpm management.  It can
operate in a local mode, were it can initalize a multiple container execution
environment and manage containers built with freight-builder.  It can also
operate in a 'node' mode where it listens for events and configuration from a
postgres database to direct the installation and execution of containers from a
central location.  In this mode yum repositories can be established holding
container rpms built with freight-builder, and those repositories can be
disseminated via the aforementioned database, as well as directives to launch
instances of those containers

freightctl is the administrative interface to a freight cluster.  This utiity
allows an administrator to preform the following operations:

* Create new tennants for a cluster
* Add and remove hosts from a cluster
* Subscribe hosts to a tennant so that a given tennant can exec containers there 

Tennants Can also use freightctl to direct container actvities:
* Addition/removal of repositories for containers
* Execution and shutdown of containers within their cluster


# Advantages
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



# Demo instructions:
1. Install and configure postgres on a system.  Ensure that it is reachable from
   the network.
2. Clone the freight-tools git tree on the system in the previous step 
3. run the command: ./scripts/createfreightdb.sh fr fn np fa ap
4. Build freight-tools
5. run ./src/freight-agent/freight-agent -c ./doc/freight-agent.config -m node
6. More here soon!
