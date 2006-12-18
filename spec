Name: ucspi-proxy
Summary: Connection proxy for UCSPI tools
Version: @VERSION@
Release: 1
License: GPL
Group: Utilities/System
Source: http://untroubled.org/ucspi-proxy/ucspi-proxy-%{version}.tar.gz
BuildRoot: %{_tmppath}/ucspi-proxy-root
URL: http://untroubled.org/ucspi-proxy/
Packager: Bruce Guenter <bruce@untroubled.org>
BuildRequires: bglibs >= 1.025

%description
This package contains a proxy program that passes data back and forth
between two connections set up by a UCSPI server and a UCSPI client.

%prep
%setup

%build
echo "gcc $RPM_OPT_FLAGS" >conf-cc
echo "gcc $RPM_OPT_FLAGS -s" >conf-ld
echo %{_bindir} >conf-bin
echo %{_mandir} >conf-man
make

%install
rm -fr $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/%{_bindir}
mkdir -p $RPM_BUILD_ROOT/%{_mandir}
make install install_prefix=$RPM_BUILD_ROOT

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%doc ANNOUNCEMENT COPYING NEWS README TODO
%{_bindir}/*
%{_mandir}/*/*
