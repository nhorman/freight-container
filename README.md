# Freight

## What is it?
Frieght is an alternative container system built to make use of RPM and systemd.  


## Why another container system?
In part because its an interesting exercise.  Also because there is value in
reuse and maximization of existing technologies on a system.  Whereas Docker,
Kubernetes, Rkt, etc have mostly in one way or anotother developed their own way
for how containers should work, most systemd based systems already have a set of
utilites for creating and managing containers. RPM already exists as a packaging
tool, and Yum/DNF already exist as package distribution tools.

## What are the advantages of using existing tools
The usual:
* Muscle memory for administration
* Better vetting of more mature code
* Natural interoperability with other existing tools (no one argues about the
  rpm format anymore :) )
* Many design issues are already solved in old tools (Freight is by design
  multi-arch capable, as an example).

## How do I use it?
Just write an rpm spec file, build it and distribute it with dnf.  The examples
directory contains example container specs that you can clone and modify to
suit your needs.  Once a Freight rpm is installed on your system, it provides a
systemd service that you can start, to bring up the container.  Interaction is
handled via machinectl and systemctl.

## Why is it better than Docker/Kube/etc?
Right now, it's not. Its focused on single user desktop systems, but it
presents an interesting (and much simpler) approach to containers that is good
for learning, and potentially usefull for smaller installations.


## Example
Using the current examples, you can build the following 2 containers:
* container_base
* container_httpd

To do so follow the following steps
1) become root
2) build the base container `rpmbuild -ba /path/to/container_base.spec`
3) install the rpm produced from (2); `rpm -ivh /path/to/container.rpm`
4) build the httpd container `rpmbuid -ba /path/to_container_httpd.sped`
5) install the rpm produced from (4): `rpm -ivh /path/to/container.rpm`
6) start the container:
`systemctl start container_httpd_<version>_<release>@default.service`
7) access the machine:
`machinectl login container_http-<version>-<release>-default`
Note the password is redhat

Feel free to explore the container, note that it is running a httpd daemon,
which can be accessed via your local web browser pointed at http://localhost


## Plans for the future?
Hopefully I would love to make use of Ansible to allow freight to be a
multi-host, multi-tennant utility.  We shall see.  I'd also like to be able to
use mock to do clean rebuilds of these containers (another example of utility
reuse), but to do so, mock needs to be updated to allow uid 0 to be the build
user id (this is pending inclusion in most distributions)

