MYRPC_DEB_DIR    := $(CURDIR)/build/deb
MYSYSLOG_DEB_DIR := $(CURDIR)/../mysyslog/build/deb
REPO_DIR ?= $(CURDIR)/../myRPC-repo

.PHONY: repository
.PHONY: all clean deb

all:
	$(MAKE) -C mysyslog
	$(MAKE) -C client
	$(MAKE) -C server

clean:
	$(MAKE) -C mysyslog clean
	$(MAKE) -C client clean
	$(MAKE) -C server clean
	rm -rf build-deb
	rm -rf build
	rm -rf bin
	rm -rf repo
	rm -rf deb
	rm -rf repository
	bash clean.sh

deb:
	$(MAKE) -C mysyslog deb
	$(MAKE) -C client deb
	$(MAKE) -C server deb

repo:
	mkdir -p repo
	cp deb/*.deb repo/
	dpkg-scanpackages repo /dev/null | gzip -9c > repo/Packages.g

