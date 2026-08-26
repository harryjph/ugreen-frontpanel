#!/bin/bash
# Regenerate source tarballs from packages/ and build the DKMS RPM.
set -euo pipefail
cd "$(dirname "$0")"
V=1.0.git20260403

mkdir -p SOURCES RPMS/noarch SRPMS BUILDROOT
( cd ../packages
  tar czf ../rpm/SOURCES/ugreen-leds-mcu-$V.tar.gz ugreen-leds-mcu
  tar czf ../rpm/SOURCES/ugreen-sio-$V.tar.gz      ugreen-sio
  tar czf ../rpm/SOURCES/axs-touch-$V.tar.gz       axs-touch
)
rpmbuild --define "_topdir $(pwd)" --define '_udevdir /usr/lib/udev' \
         -bb SPECS/ugreen-frontpanel-dkms.spec
echo "--> $(pwd)/RPMS/noarch/ugreen-frontpanel-dkms-${V}-1.*.noarch.rpm"
