Name:       gcoin-community
Version:    1.1.2
Release:    1%{?dist}
Summary:    Gcoin core daemon - reference client and server

Group:      Applications/System
License:    ASL 2.0
URL:        http://g-coin.org
Source0:    https://github.com/OpenNetworking/%{name}/archive/v%{version}.tar.gz

BuildRequires: autoconf automake libtool
BuildRequires: boost-devel libdb-cxx-devel
BuildRequires: pkgconfig(openssl)
BuildRequires: pkgconfig(libsystemd)

%if 0%{?rhel} && 0%{?rhel} <= 7
BuildRequires: gcoin-compat-openssl-devel
%global openssl_includedir  %{_includedir}/gcoin-compat-openssl
%global openssl_libdir      %{_libdir}/gcoin-compat-openssl
%endif

%description
Gcoin blockchain is an open-source software built for next-generation
digital infrastructure, the distributed ledger.


%prep
%setup -q -n %{name}-%{version}


%build
autoreconf -if
%configure \
%if 0%{?rhel} && 0%{?rhel} <= 7
    CRYPTO_CFLAGS='-I%{openssl_includedir}' \
    CRYPTO_LIBS='%{openssl_libdir}/libcrypto.so' \
    SSL_CFLAGS='-I%{openssl_includedir}' \
    SSL_LIBS='%{openssl_libdir}/libssl.so' \
    LDFLAGS='%{__global_ldflags} -Wl,--enable-new-dtags -Wl,-rpath,%{openssl_libdir}' \
%endif
    --enable-systemd-journal --without-miniupnpc CXX="c++ -std=gnu++03"
%make_build


%install
install -m 755 -d %{buildroot}%{_bindir}
install -m 755 src/gcoind %{buildroot}%{_bindir}
install -m 755 src/gcoin-cli %{buildroot}%{_bindir}


%check
make check


%files
%{_bindir}/gcoind
%{_bindir}/gcoin-cli
%license COPYING
%doc README.md


%changelog
* Tue Jul 05 2016 Ting-Wei Lan <lantw44@gmail.com> - 1.1.2-1
- Initial public packaging
