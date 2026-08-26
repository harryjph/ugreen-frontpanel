#!/bin/bash
# Regenerate source tarballs and produce the SRPM (uploadable to COPR).
set -euo pipefail
cd "$(dirname "$0")"
V=1.0.git20260403

mkdir -p SOURCES SPECS RPMS/noarch SRPMS BUILDROOT
( cd ../packages
  tar czf ../rpm/SOURCES/ugreen-leds-mcu-$V.tar.gz ugreen-leds-mcu
  tar czf ../rpm/SOURCES/ugreen-sio-$V.tar.gz      ugreen-sio
  tar czf ../rpm/SOURCES/axs-touch-$V.tar.gz       axs-touch
)
rpmbuild --define "_topdir $(pwd)" --define '_udevdir /usr/lib/udev' \
         -bs SPECS/ugreen-frontpanel-dkms.spec
echo "--> $(pwd)/SRPMS/ugreen-frontpanel-dkms-${V}-1.*.src.rpm"
