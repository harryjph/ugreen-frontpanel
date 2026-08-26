# COPR "make_srpm" glue: generates the module source tarballs at build time
# and produces the SRPM. Copr invokes: make -f .copr/Makefile srpm OUTDIR=<results>
# Copr invokes us as:  make -f .copr/Makefile srpm outdir=<results> spec=<spec>
# (both passed as *lowercase* command-line variables)
outdir ?= $(CURDIR)
OUTDIR = $(outdir)

SPEC := rpm/SPECS/ugreen-frontpanel-dkms.spec
VER  := $(shell sed -n 's/^Version:[[:space:]]*//p' $(SPEC) | tr -d ' ')

.PHONY: srpm
srpm:
	command -v rpmbuild >/dev/null 2>&1 || dnf install -y rpm-build
	mkdir -p _topdir/SOURCES _topdir/SRPMS
	tar czf _topdir/SOURCES/ugreen-leds-mcu-$(VER).tar.gz -C packages ugreen-leds-mcu
	tar czf _topdir/SOURCES/ugreen-sio-$(VER).tar.gz      -C packages ugreen-sio
	tar czf _topdir/SOURCES/axs-touch-$(VER).tar.gz       -C packages axs-touch
	tar czf _topdir/SOURCES/ugreen-frontpanel-dkms-deps.tar.gz -C packages \
	    LICENSE-NOTICE README.rst 60-ugreen-axs_touch.rules ugreen-frontpanel.conf
	rpmbuild -bs --define "_topdir $(CURDIR)/_topdir"          \
	         --define "_sourcedir $(CURDIR)/_topdir/SOURCES"    \
	         --define '_udevrulesdir /usr/lib/udev/rules.d'     \
	         $(SPEC)
	cp _topdir/SRPMS/*.src.rpm "$(OUTDIR)/"
