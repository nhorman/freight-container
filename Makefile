GITSHASH := $(shell git log --format=%h HEAD^..HEAD)
GITHASH := $(shell git log --format=%H HEAD^..HEAD)

all:
	mkdir -p ./SOURCES
	git archive --format=tar.gz --prefix=freight-${GITHASH}/ -o ./SOURCES/freight-${GITSHASH}.tar.gz ${GITHASH} 
	rpmbuild --define='githash ${GITHASH}' --define='_topdir ${PWD}' -ba freight.spec

clean:
	rm -rf SOURCES
