Name: ucspi-proxy
Summary: Connection proxy for UCSPI tools
Version: @VERSION@
Release: 1
Copyright: GPL
Group: Utilities/System
Source: http://em.ca/~bruceg/ucspi-proxy/%{version}/ucspi-proxy-%{version}.tar.gz
BuildRoot: /tmp/ucspi-proxy-root
URL: http://em.ca/~bruceg/ucspi-proxy/
Packager: Bruce Guenter <bruceg@em.ca>

%description
This package contains a proxy program that passes data back and forth
between two connections set up by a UCSPI server and a UCSPI client.

%prep
%setup

%build
make CFLAGS="$RPM_OPT_FLAGS" LDFLAGS="$RPM_OPT_FLAGS -s"

%install
rm -fr $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT
make install_prefix=$RPM_BUILD_ROOT install

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%doc COPYING README TODO
/usr/bin/*
/usr/man/man1/*
