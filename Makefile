MYRPC_DEB_DIR    := $(CURDIR)/build/deb
MYSYSLOG_DEB_DIR := $(CURDIR)/../mysyslog/build/deb
REPO_DIR ?= $(CURDIR)/../myRPC-repo

.PHONY: repository
.PHONY: all clean deb

all:
	$(MAKE) -C src/mysyslog
	$(MAKE) -C src/client
	$(MAKE) -C src/server

clean:
	$(MAKE) -C src/mysyslog clean
	$(MAKE) -C src/client clean
	$(MAKE) -C src/server clean
	rm -rf build-deb
	rm -rf build
	rm -rf bin
	rm -rf repo
	rm -rf deb
	rm -rf repository
	bash clean.sh

deb:
	$(MAKE) -C src/mysyslog deb
	$(MAKE) -C src/client deb
	$(MAKE) -C src/server deb

repo:
	mkdir -p repo
	cp deb/*.deb repo/
	dpkg-scanpackages repo /dev/null | gzip -9c > repo/Packages.g

