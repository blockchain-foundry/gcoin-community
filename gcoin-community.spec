Name:       gcoin-community
Version:    1.2.1
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

* Sat June 12 2017 Bo-Yu Lin <skzlbyyy@gmail.com> - 1.2.1
- Reorder type of transaction.
- Prevent same pubkey in a alliance redeem script.
- Fix voting bug when -txindex turn on.
- Fix rpc signrawtransaction for license/miner related tx.
- Fix assignfixedaddress.
- Fix CheckTxFeeAndColor.
- Fix bug when transferring license.

* Sat Feb 18 2017 Bo-Yu Lin <skzlbyyy@gmail.com> - 1.2
- New role Miner.
- Use multi-sig policy to handle alliance relative issue.
- Allow higher fee.
- Update COPYING.
- Increase limitation of multi-sig address.

* Thu Nov 17 2016 Bo-Yu Lin <skzlbyyy@gmail.com> - 1.1.4
- Fix the bug of getlicenselist.
- Remove member only policy
- Update gcoin-compat-openssl.spec
- New RPC: assignfixedaddress, which used to set default address

* Sun Aug 28 2016 Ting-Wei Lan <lantw44@gmail.com> - 1.1.3.2-1
- Fix inconsistency between RPM spec and git tag

* Fri Aug 19 2016 Pang-Ting Huang <hihiben@gmail.com> - 1.1.3-1
- Put the data into "main" directory when running gcoind
- Naming refactor : bitcoin -> gcoin
- Remove order(exchange) mechanism (ORDER, MATCH, CANCEL)
- RPC refinement: fix rpc message
- Fix account related functions
- Apply multicurrency to some RPCs

* Tue Jul 05 2016 Ting-Wei Lan <lantw44@gmail.com> - 1.1.2-1
- Initial public packaging
