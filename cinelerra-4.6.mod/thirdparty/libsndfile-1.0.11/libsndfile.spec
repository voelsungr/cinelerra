
%define name    libsndfile
%define version 1.0.11
%define release 1

Summary: A library to handle various audio file formats.
Name: %{name}
Version: %{version}
Release: %{release}
Copyright: LGPL
Group: Libraries/Sound
Source: http://www.mega-nerd.com/libsndfile/libsndfile-%{version}.tar.gz
URL: http://www.mega-nerd.com/libsndfile/
BuildRoot: /var/tmp/%{name}-%{version}

%description
libsndfile is a C library for reading and writing sound files such as
AIFF, AU and WAV files through one standard interface. It can currently
read/write 8, 16, 24 and 32-bit PCM files as well as 32-bit floating
point WAV files and a number of compressed formats.

%package devel
Summary: Libraries, includes, etc to develop libsndfile applications
Group: Libraries

%description devel
Libraries, include files, etc you can use to develop libsndfile applications.

%prep
%setup

%build
%configure
make

%install
if [ -d $RPM_BUILD_ROOT ]; then rm -rf $RPM_BUILD_ROOT; fi
mkdir -p $RPM_BUILD_ROOT
make DESTDIR=$RPM_BUILD_ROOT install
%clean
if [ -d $RPM_BUILD_ROOT ]; then rm -rf $RPM_BUILD_ROOT; fi

%files
%defattr(-,root,root)
%doc AUTHORS COPYING ChangeLog INSTALL NEWS README TODO doc
%{_libdir}/libsndfile.so.*
%{_bindir}/*
%{_mandir}/man1/*
%{_datadir}/octave/site/m/*

%files devel
%defattr(-,root,root)
%{_libdir}/libsndfile.a
%{_libdir}/libsndfile.la
%{_libdir}/libsndfile.so
%{_includedir}/sndfile.h
%{_libdir}/pkgconfig/sndfile.pc

%changelog
* Tue Sep 16 2003 Erik de Castro Lopo <erikd@mega-nerd.com>
- Apply corrections from Andrew Schultz.
* Mon Oct 21 2002 Erik de Castro Lopo <erikd@mega-nerd.com>
- Force installation of sndfile.pc file.
* Thu Jul 6 2000 Josh Green <jgreen@users.sourceforge.net>
- Created libsndfile.spec.in

