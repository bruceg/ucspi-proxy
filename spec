Name: ucspi-proxy
Summary: Connection proxy for UCSPI tools
Version: @VERSION@
Release: 1
Copyright: GPL
Group: Utilities/System
Source: http://em.ca/~bruceg/ucspi-proxy/%{version}/ucspi-proxy-%{version}.tar.gz
BuildRoot: ${_tmppath}/ucspi-proxy-root
URL: http://em.ca/~bruceg/ucspi-proxy/
Packager: Bruce Guenter <bruceg@em.ca>
BuildRequires: bglibs >= 1.015

%description
This package contains a proxy program that passes data back and forth
between two connections set up by a UCSPI server and a UCSPI client.

%prep
%setup

%build
echo "gcc $RPM_OPT_FLAGS" >conf-cc
echo "gcc $RPM_OPT_FLAGS -s" >conf-ld
make

%install
rm -fr $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/%{_bindir}
mkdir -p $RPM_BUILD_ROOT/%{_mandir}
echo $RPM_BUILD_ROOT/%{_bindir} >conf-bin
echo $RPM_BUILD_ROOT/%{_mandir} >conf-man
rm -f insthier.o conf_*.c installer instcheck
make
./installer
./instcheck

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%doc COPYING README TODO
%{_bindir}/*
%{_mandir}/*/*
