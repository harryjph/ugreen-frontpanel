#!/bin/sh -ex
# COPR "custom" build-method script.
# Project settings that go with it:
#   Build method:            custom
#   Clone URL / source repo: <your git repo containing packages/ and rpm/>
#   Script:                  rpm/copr/build_srpm.sh
#   Additional packages:     rpm-build
VER=$(sed -n 's/^Version:[[:space:]]*//p' rpm/SPECS/ugreen-frontpanel-dkms.spec | tr -d ' ')
mkdir -p "$HOME/rpmbuild/SOURCES" "$HOME/rpmbuild/SPECS"
tar czf "$HOME/rpmbuild/SOURCES/ugreen-leds-mcu-$VER.tar.gz" -C packages ugreen-leds-mcu
tar czf "$HOME/rpmbuild/SOURCES/ugreen-sio-$VER.tar.gz"      -C packages ugreen-sio
tar czf "$HOME/rpmbuild/SOURCES/axs-touch-$VER.tar.gz"       -C packages axs-touch
cp rpm/SOURCES/ugreen-frontpanel-dkms-deps.tar.gz "$HOME/rpmbuild/SOURCES/"
cp rpm/SPECS/ugreen-frontpanel-dkms.spec          "$HOME/rpmbuild/SPECS/"
rpmbuild -bs --define "_topdir $HOME/rpmbuild" "$HOME/rpmbuild/SPECS/ugreen-frontpanel-dkms.spec"
cp "$HOME"/rpmbuild/SRPMS/*.src.rpm "$COPR_SRPM_OUTDIR/"
