# Spec for UGREEN iDX6011 Pro front-panel drivers, packaged as three DKMS modules
# Drivers sourced from: https://github.com/ugreen-opensource/kernel-6.12 (commit c75c5abd6)
# License: GPL-2.0-only

%global _vpath_srcdir %nil

Name:           ugreen-frontpanel-dkms
Version:        1.0.git20260403
Release:        1%{?dist}
Summary:        DKMS kernel modules for the UGREEN iDX6011 Pro front panel (LEDs, SIO/EC, touch)

License:        GPL-2.0-only
URL:            https://github.com/ugreen-opensource/kernel-6.12
Source0:        ugreen-leds-mcu-%{version}.tar.gz
Source1:        ugreen-sio-%{version}.tar.gz
Source2:        axs-touch-%{version}.tar.gz
Source3:        ugreen-frontpanel-dkms-deps.tar.gz

BuildArch:      noarch
BuildRequires:  systemd-rpm-macros

Requires:       dkms >= 2.5
Requires:       gcc, make, binutils
Requires:       (kernel-devel if kernel)
Requires:       elfutils-libelf-devel

%description
DKMS source packages and automatic build/install glue for the UGREEN iDX6011 Pro
front-panel hardware on stock distribution kernels:

  * ugreen-leds-mcu : front-panel LED MCU driver (i2c) + normal/breath/timer2 triggers
  * ugreen-sio      : IT55xx EC Super-IO -> mipi_backlight backlight device,
                      fans, watchdog, power/wake procfs; GPIO power button; SATA beeper
  * axs-touch       : AiXieSheng AXS15205 touch controller of the front LCD
                      (autoloads via ACPI modalias acpi:CUST0000)

Upstream: https://github.com/ugreen-opensource/kernel-6.12 (GPL-2.0-only)

%prep
%setup -q -c -T -a 3 -a 0 -a 1 -a 2

%build
# nothing to compile here; compilation happens through dkms at install time

%install
mkdir -p %{buildroot}%{_usrsrc} \
         %{buildroot}%{_udevrulesdir} \
         %{buildroot}%{_modulesloaddir}
cp -a ugreen-leds-mcu %{buildroot}%{_usrsrc}/ugreen-leds-mcu-%{version}
cp -a ugreen-sio      %{buildroot}%{_usrsrc}/ugreen-sio-%{version}
cp -a axs-touch       %{buildroot}%{_usrsrc}/axs-touch-%{version}
install -Dpm 0644 60-ugreen-axs_touch.rules %{buildroot}%{_udevrulesdir}/60-ugreen-axs_touch.rules
install -Dpm 0644 ugreen-frontpanel.conf    %{buildroot}%{_modulesloaddir}/ugreen-frontpanel.conf

%files
%license LICENSE-NOTICE
%doc README.rst
%dir %{_usrsrc}/ugreen-leds-mcu-%{version}
%{_usrsrc}/ugreen-leds-mcu-%{version}/*
%dir %{_usrsrc}/ugreen-sio-%{version}
%{_usrsrc}/ugreen-sio-%{version}/*
%dir %{_usrsrc}/axs-touch-%{version}
%{_usrsrc}/axs-touch-%{version}/*
%{_udevrulesdir}/60-ugreen-axs_touch.rules
%{_modulesloaddir}/ugreen-frontpanel.conf

%pre
for m in ugreen-leds-mcu:%{version} ugreen-sio:%{version} axs-touch:%{version}; do
    dkms remove -m "${m%%:*}" -v "${m##*:}" --all >/dev/null 2>&1 || :
done

%post
ERR=0
for m in ugreen-leds-mcu ugreen-sio axs-touch; do
    dkms add    -m "$m" -v %{version}                     || ERR=1
    dkms build  -m "$m" -v %{version} -k "$(uname -r)" || ERR=1
    dkms install -m "$m" -v %{version} -k "$(uname -r)" || ERR=1
done
exit 0

%preun
for m in ugreen-leds-mcu ugreen-sio axs-touch; do
    dkms remove -m "$m" -v %{version} --all >/dev/null 2>&1 || :
done

%postun
rm -rf %{_usrsrc}/ugreen-leds-mcu-%{version} \
       %{_usrsrc}/ugreen-sio-%{version} \
       %{_usrsrc}/axs-touch-%{version}

%changelog
* Wed Aug 26 2026 harry <harry@localhost> - 1.0.git20260403-1
- Initial packaging from ugreen-opensource/kernel-6.12 commit c75c5abd6.
