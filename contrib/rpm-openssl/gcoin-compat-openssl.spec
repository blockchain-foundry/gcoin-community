Name:       gcoin-compat-openssl
Version:    1.0.2l
Release:    2%{?dist}
Summary:    OpenSSL shared libraries compiled with secp256k1 support

Group:      System Environment/Libraries
License:    OpenSSL
URL:        https://www.openssl.org
Source0:    https://www.openssl.org/source/openssl-%{version}.tar.gz

%global __provides_exclude_from ^%{_libdir}/%{name}/engines/.*\\.so$

%description
OpenSSL shared libraries package specifically made to support Gcoin
blockchain software on RHEL 7 and CentOS 7.

%package devel
Summary:    OpenSSL development files used to compile Gcoin
Group:      Development/Libraries
Requires:   %{name}%{?_isa} = %{version}-%{release}

%description devel
OpenSSL development files needed to compile Gcoin blockchain software
on RHEL 7 and CentOS 7.

%package static
Summary:    OpenSSL static libraries compiled with secp256k1 support
Group:      Development/Libraries
Requires:   %{name}%{?_isa} = %{version}-%{release}

%description static
OpenSSL static libraries package with secp256k1 support on RHEL 7 and CentOS 7.


%prep
%setup -q -n openssl-%{version}


%build
export CC="gcc %{optflags} %{__global_ldflags}"
./config \
    --prefix=%{_prefix} --libdir=%{_lib}/%{name} \
    no-ssl2 no-ssl3 no-dtls no-idea no-mdc2 no-rc5 no-ec2m no-gost no-srp \
    no-weak-ssl-ciphers shared
make depend MAKEDEPPROG="gcc"
make all


%install
make install_sw INSTALL_PREFIX=%{buildroot}
rm -r %{buildroot}%{_prefix}/bin
rm -r %{buildroot}%{_prefix}/ssl
rm -r %{buildroot}%{_libdir}/%{name}/pkgconfig
mkdir %{buildroot}%{_includedir}/%{name}
mv %{buildroot}%{_includedir}/openssl %{buildroot}%{_includedir}/%{name}
find %{buildroot}%{_libdir}/%{name} -name '*.so' -exec chmod u+w '{}' ';'


%files
%{_libdir}/%{name}/libcrypto.so.1.0.0
%{_libdir}/%{name}/libssl.so.1.0.0
%{_libdir}/%{name}/engines
%license LICENSE
%doc FAQ NEWS README

%files devel
%{_includedir}/%{name}/openssl
%{_libdir}/%{name}/libcrypto.so
%{_libdir}/%{name}/libssl.so

%files static
%{_libdir}/%{name}/libcrypto.a
%{_libdir}/%{name}/libssl.a


%changelog
* Mon Jul 03 2017 Ting-Wei Lan <lantw44@gmail.com> - 1.0.2l-2
- Disable automatic provides finding in engines directory

* Sun Jul 02 2017 Ting-Wei Lan <lantw44@gmail.com> - 1.0.2l-1
- Update to 1.0.2l

* Tue Sep 27 2016 Ting-Wei Lan <lantw44@gmail.com> - 1.0.2j-1
- Update to 1.0.2j

* Mon Sep 26 2016 Ting-Wei Lan <lantw44@gmail.com> - 1.0.2i-1
- Update to 1.0.2i

* Tue Jul 05 2016 Ting-Wei Lan <lantw44@gmail.com> - 1.0.2h-1
- Initial public packaging
