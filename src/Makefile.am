DIST_SUBDIRS = secp256k1
AM_LDFLAGS = $(PTHREAD_CFLAGS) $(LIBTOOL_LDFLAGS)
EXTRA_LIBRARIES =

BITCOIN_CONFIG_INCLUDES=-I$(builddir)/config
GCOIN_INCLUDES=-I$(builddir) -I$(builddir)/obj $(BOOST_CPPFLAGS) $(LEVELDB_CPPFLAGS) $(CRYPTO_CFLAGS) $(SSL_CFLAGS) $(SYSTEMD_JOURNAL_CFLAGS)

GCOIN_INCLUDES += -I$(srcdir)/secp256k1/include

LIBGCOIN_SERVER=libgcoin_server.a
LIBGCOIN_WALLET=libgcoin_wallet.a
LIBGCOIN_COMMON=libgcoin_common.a
LIBGCOIN_CLI=libgcoin_cli.a
LIBGCOIN_UTIL=libgcoin_util.a
LIBGCOIN_CRYPTO=crypto/libgcoin_crypto.a
LIBGCOIN_UNIVALUE=univalue/libgcoin_univalue.a
LIBGCOINQT=qt/libgcoinqt.a
LIBSECP256K1=secp256k1/libsecp256k1.la

$(LIBSECP256K1): $(wildcard secp256k1/src/*) $(wildcard secp256k1/include/*)
	$(AM_V_at)$(MAKE) $(AM_MAKEFLAGS) -C $(@D) $(@F)

# Make is not made aware of per-object dependencies to avoid limiting building parallelization
# But to build the less dependent modules first, we manually select their order here:
EXTRA_LIBRARIES += \
  crypto/libgcoin_crypto.a \
  libgcoin_util.a \
  libgcoin_common.a \
  univalue/libgcoin_univalue.a \
  libgcoin_server.a \
  libgcoin_cli.a
if ENABLE_WALLET
GCOIN_INCLUDES += $(BDB_CPPFLAGS)
EXTRA_LIBRARIES += libgcoin_wallet.a
endif

if BUILD_GCOIN_LIBS
lib_LTLIBRARIES = libgcoinconsensus.la
LIBGCOIN_CONSENSUS=libgcoinconsensus.la
else
LIBGCOIN_CONSENSUS=
endif

bin_PROGRAMS =
TESTS =

if BUILD_GCOIND
  bin_PROGRAMS += gcoind
endif

if BUILD_GCOIN_UTILS
  bin_PROGRAMS += gcoin-cli
endif

.PHONY: FORCE
# gcoin core #
GCOIN_CORE_H = \
  addrman.h \
  alert.h \
  amount.h \
  arith_uint256.h \
  base58.h \
  bloom.h \
  cache.h \
  chain.h \
  chainparams.h \
  chainparamsbase.h \
  chainparamsseeds.h \
  checkpoints.h \
  checkqueue.h \
  clientversion.h \
  coincontrol.h \
  coins.h \
  compat.h \
  compat/byteswap.h \
  compat/endian.h \
  compat/sanity.h \
  compressor.h \
  consensus/consensus.h \
  consensus/params.h \
  consensus/validation.h \
  core_io.h \
  eccryptoverify.h \
  ecwrapper.h \
  hash.h \
  init.h \
  key.h \
  keystore.h \
  leveldbwrapper.h \
  limitedmap.h \
  main.h \
  memusage.h \
  merkleblock.h \
  miner.h \
  mruset.h \
  net.h \
  netbase.h \
  noui.h \
  policy/fees.h \
  policy/licenseinfo.h \
  pow.h \
  primitives/block.h \
  primitives/transaction.h \
  protocol.h \
  pubkey.h \
  random.h \
  rpcclient.h \
  rpcprotocol.h \
  rpcserver.h \
  scheduler.h \
  script/interpreter.h \
  script/script.h \
  script/script_error.h \
  script/sigcache.h \
  script/sign.h \
  script/standard.h \
  serialize.h \
  streams.h \
  support/allocators/secure.h \
  support/allocators/zeroafterfree.h \
  support/cleanse.h \
  support/pagelocker.h \
  sync.h \
  threadsafety.h \
  timedata.h \
  tinyformat.h \
  txdb.h \
  txmempool.h \
  ui_interface.h \
  uint256.h \
  undo.h \
  util.h \
  utilerror.h \
  utilstrencodings.h \
  utilmoneystr.h \
  utilstrencodings.h \
  utiltime.h \
  validationinterface.h \
  version.h \
  wallet/crypter.h \
  wallet/db.h \
  wallet/hdkeystore.h \
  wallet/wallet.h \
  wallet/wallet_ismine.h \
  wallet/walletdb.h

JSON_H = \
  json/json_spirit.h \
  json/json_spirit_error_position.h \
  json/json_spirit_reader.h \
  json/json_spirit_reader_template.h \
  json/json_spirit_stream_reader.h \
  json/json_spirit_utils.h \
  json/json_spirit_value.h \
  json/json_spirit_writer.h \
  json/json_spirit_writer_template.h

obj/build.h: FORCE
	@$(MKDIR_P) $(builddir)/obj
	@$(top_srcdir)/share/genbuild.sh $(abs_top_builddir)/src/obj/build.h \
	  $(abs_top_srcdir)
libgcoin_util_a-clientversion.$(OBJEXT): obj/build.h

# server: for gcoind
libgcoin_server_a_CPPFLAGS = $(GCOIN_INCLUDES) $(MINIUPNPC_CPPFLAGS)
libgcoin_server_a_SOURCES = \
  addrman.cpp \
  alert.cpp \
  bloom.cpp \
  cache.cpp \
  chain.cpp \
  checkpoints.cpp \
  init.cpp \
  leveldbwrapper.cpp \
  main.cpp \
  merkleblock.cpp \
  miner.cpp \
  net.cpp \
  noui.cpp \
  policy/fees.cpp \
  pow.cpp \
  rest.cpp \
  rpcblockchain.cpp \
  rpcmining.cpp \
  rpcmisc.cpp \
  rpcnet.cpp \
  rpcrawtransaction.cpp \
  rpcserver.cpp \
  script/sigcache.cpp \
  timedata.cpp \
  txdb.cpp \
  txmempool.cpp \
  validationinterface.cpp \
  $(JSON_H) \
  $(GCOIN_CORE_H)

# wallet: for gcoind, but only linked
# when wallet enabled
libgcoin_wallet_a_CPPFLAGS = $(GCOIN_INCLUDES)
libgcoin_wallet_a_SOURCES = \
  wallet/crypter.cpp \
  wallet/db.cpp \
  wallet/hdkeystore.cpp \
  wallet/rpcdump.cpp \
  wallet/rpcwallet.cpp \
  wallet/wallet.cpp \
  wallet/wallet_ismine.cpp \
  wallet/walletdb.cpp \
  $(GCOIN_CORE_H)

# crypto primitives library
crypto_libgcoin_crypto_a_CPPFLAGS = $(BITCOIN_CONFIG_INCLUDES)
crypto_libgcoin_crypto_a_SOURCES = \
  crypto/common.h \
  crypto/hmac_sha256.cpp \
  crypto/hmac_sha256.h \
  crypto/hmac_sha512.cpp \
  crypto/hmac_sha512.h \
  crypto/ripemd160.cpp \
  crypto/ripemd160.h \
  crypto/sha1.cpp \
  crypto/sha1.h \
  crypto/sha256.cpp \
  crypto/sha256.h \
  crypto/sha512.cpp \
  crypto/sha512.h

# univalue JSON library
univalue_libgcoin_univalue_a_SOURCES = \
  univalue/univalue.cpp \
  univalue/univalue.h \
  univalue/univalue_escapes.h \
  univalue/univalue_read.cpp \
  univalue/univalue_write.cpp

# common: shared between gcoind, and non-server tools
libgcoin_common_a_CPPFLAGS = $(GCOIN_INCLUDES)
libgcoin_common_a_SOURCES = \
  amount.cpp \
  arith_uint256.cpp \
  base58.cpp \
  chainparams.cpp \
  coins.cpp \
  compressor.cpp \
  core_read.cpp \
  core_write.cpp \
  eccryptoverify.cpp \
  ecwrapper.cpp \
  hash.cpp \
  key.cpp \
  keystore.cpp \
  netbase.cpp \
  primitives/block.cpp \
  primitives/transaction.cpp \
  policy/licenseinfo.cpp \
  protocol.cpp \
  pubkey.cpp \
  scheduler.cpp \
  script/interpreter.cpp \
  script/script.cpp \
  script/script_error.cpp \
  script/sign.cpp \
  script/standard.cpp \
  $(GCOIN_CORE_H)

# util: shared between all executables.
# This library *must* be included to make sure that the glibc
# backward-compatibility objects and their sanity checks are linked.
libgcoin_util_a_CPPFLAGS = $(GCOIN_INCLUDES)
libgcoin_util_a_SOURCES = \
  support/pagelocker.cpp \
  chainparamsbase.cpp \
  clientversion.cpp \
  compat/glibc_sanity.cpp \
  compat/glibcxx_sanity.cpp \
  compat/strnlen.cpp \
  random.cpp \
  rpcprotocol.cpp \
  support/cleanse.cpp \
  sync.cpp \
  uint256.cpp \
  util.cpp \
  utilmoneystr.cpp \
  utilstrencodings.cpp \
  utiltime.cpp \
  $(GCOIN_CORE_H)

if GLIBC_BACK_COMPAT
libgcoin_util_a_SOURCES += compat/glibc_compat.cpp
endif

# cli: for gcoin-cli
libgcoin_cli_a_CPPFLAGS = $(GCOIN_INCLUDES)
libgcoin_cli_a_SOURCES = \
  rpcclient.cpp \
  $(GCOIN_CORE_H)

nodist_libgcoin_util_a_SOURCES = $(srcdir)/obj/build.h
#

# gcoind binary #
gcoind_SOURCES = gcoind.cpp
gcoind_CPPFLAGS = $(GCOIN_INCLUDES)
gcoind_LDFLAGS = $(RELDFLAGS) $(AM_LDFLAGS) $(LIBTOOL_APP_LDFLAGS)

if TARGET_WINDOWS
gcoind_SOURCES += gcoind-res.rc
endif

gcoind_LDADD = \
  $(LIBGCOIN_SERVER) \
  $(LIBGCOIN_COMMON) \
  $(LIBGCOIN_UNIVALUE) \
  $(LIBGCOIN_UTIL) \
  $(LIBGCOIN_CRYPTO) \
  $(LIBLEVELDB) \
  $(LIBMEMENV) \
  $(LIBSECP256K1)

if ENABLE_WALLET
gcoind_LDADD += libgcoin_wallet.a
endif

gcoind_LDADD += $(BOOST_LIBS) $(BDB_LIBS) $(SSL_LIBS) $(CRYPTO_LIBS) $(MINIUPNPC_LIBS) $(SYSTEMD_JOURNAL_LIBS)
#

# gcoin-cli binary #
gcoin_cli_SOURCES = gcoin-cli.cpp
gcoin_cli_CPPFLAGS = $(GCOIN_INCLUDES)
gcoin_cli_LDFLAGS = $(RELDFLAGS) $(AM_LDFLAGS) $(LIBTOOL_APP_LDFLAGS)

if TARGET_WINDOWS
gcoin_cli_SOURCES += gcoin-cli-res.rc
endif

# gcoin-cli binary #
gcoin_cli_LDADD = \
  $(LIBGCOIN_CLI) \
  $(LIBGCOIN_UTIL) \
  $(LIBSECP256K1) \
  $(LIBGCOIN_CRYPTO) \
  $(BOOST_LIBS) \
  $(SSL_LIBS) \
  $(CRYPTO_LIBS) \
  $(SYSTEMD_JOURNAL_LIBS)


# gcoinconsensus library #
if BUILD_GCOIN_LIBS
include_HEADERS = script/gcoinconsensus.h
libgcoinconsensus_la_SOURCES = \
  crypto/hmac_sha512.cpp \
  crypto/ripemd160.cpp \
  crypto/sha1.cpp \
  crypto/sha256.cpp \
  crypto/sha512.cpp \
  eccryptoverify.cpp \
  ecwrapper.cpp \
  hash.cpp \
  primitives/transaction.cpp \
  pubkey.cpp \
  script/gcoinconsensus.cpp \
  script/interpreter.cpp \
  script/script.cpp \
  uint256.cpp \
  utilstrencodings.cpp

if GLIBC_BACK_COMPAT
  libgcoinconsensus_la_SOURCES += compat/glibc_compat.cpp
endif

libgcoinconsensus_la_LDFLAGS = -no-undefined $(RELDFLAGS)
libgcoinconsensus_la_LIBADD = $(CRYPTO_LIBS)
libgcoinconsensus_la_CPPFLAGS = $(CRYPTO_CFLAGS) -I$(builddir)/obj -DBUILD_BITCOIN_INTERNAL

#

endif
#

CLEANFILES = $(EXTRA_LIBRARIES) *.gcda *.gcno

DISTCLEANFILES = obj/build.h

EXTRA_DIST =

clean-local:
	-$(MAKE) -C secp256k1 clean
	rm -f leveldb/*/*.gcno leveldb/helpers/memenv/*.gcno
	-rm -f config.h

.rc.o:
	@test -f $(WINDRES)
	$(AM_V_GEN) $(WINDRES) -DWINDRES_PREPROC -i $< -o $@

.mm.o:
	$(AM_V_CXX) $(OBJCXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) \
	  $(CPPFLAGS) $(AM_CXXFLAGS) $(QT_INCLUDES) $(CXXFLAGS)  -c -o $@ $<

%.pb.cc %.pb.h: %.proto
	@test -f $(PROTOC)
	$(AM_V_GEN) $(PROTOC) --cpp_out=$(@D) --proto_path=$(abspath $(<D) $<)
 
if EMBEDDED_LEVELDB
include Makefile.leveldb.include
endif

if ENABLE_TESTS
include Makefile.test.include

endif
