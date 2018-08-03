GITSHASH := $(shell git log --format=%h HEAD^..HEAD)
GITHASH := $(shell git log --format=%H HEAD^..HEAD)


all: tarball
	rpmbuild --define='githash ${GITHASH}' --define='_topdir ${PWD}' -ba freight-container.spec

tarball:
	mkdir -p ./SOURCES
	git archive --format=tar.gz --prefix=freight-container-${GITHASH}/ -o ./SOURCES/freight-container-${GITSHASH}.tar.gz ${GITHASH}

clean:
	rm -rf SOURCES BUILD BUILDROOT RPMS SRPMS SPECS
