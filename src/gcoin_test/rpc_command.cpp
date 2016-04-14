// Copyright (c) 2014-2016 The Gcoin Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rpcserver.h"
#include "rpcclient.h"
#include "init.h"
#include "base58.h"
#include "wallet/wallet.h"
#include "key.h"
#include "test_util.h"
#include "policy/licenseinfo.h"

#include <set>
#include <stdint.h>
#include <utility>
#include <vector>

#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/test/unit_test.hpp>

using namespace std;
using namespace json_spirit;

struct RPCTestWalletFixture : public GlobalSetupFixture, public CacheSetupFixture, public WalletSetupFixture
{
    RPCTestWalletFixture()
    {
    }
    ~RPCTestWalletFixture()
    {
    }
};

struct RPCTestFixture : public GlobalSetupFixture, public CacheSetupFixture
{
    RPCTestFixture()
    {
    }
    ~RPCTestFixture()
    {
    }
};

BOOST_AUTO_TEST_SUITE(test_rpc_command)

BOOST_FIXTURE_TEST_CASE(rpc_getfixedaddress_test, RPCTestWalletFixture)
{
    BOOST_CHECK(pwalletMain != NULL);
    LOCK2(cs_main, pwalletMain->cs_wallet);

    string strRPC = "getfixedaddress";
    BOOST_CHECK_NO_THROW(CallRPC(strRPC));
}

BOOST_FIXTURE_TEST_CASE(rpc_importprivkey_test, RPCTestWalletFixture)
{
    BOOST_CHECK(pwalletMain != NULL);
    LOCK2(cs_main, pwalletMain->cs_wallet);

    CKey key;
    CPubKey pubkey;
    string strRPC;

    key.MakeNewKey(true);
    pubkey = key.GetPubKey();
    strRPC= "importprivkey " + CBitcoinSecret(key).ToString();
    BOOST_CHECK_NO_THROW(CallRPC(strRPC));

    key.MakeNewKey(true);
    pubkey = key.GetPubKey();
    strRPC = "importprivkey " + CBitcoinSecret(key).ToString() + " import";
    BOOST_CHECK_NO_THROW(CallRPC(strRPC));
}

BOOST_FIXTURE_TEST_CASE(rpc_getnewaddress_test, RPCTestWalletFixture)
{
    BOOST_CHECK(pwalletMain != NULL);
    LOCK2(cs_main, pwalletMain->cs_wallet);

    CKey key;
    CPubKey pubkey;
    string strRPC = "";
    Value r;

    // show import key tests
    key.MakeNewKey(true);
    pubkey = key.GetPubKey();

    // import private key
    strRPC = "importprivkey " + CBitcoinSecret(key).ToString();
    CallRPC(strRPC);

    // address not found before import
    strRPC = "getnewaddress";
    BOOST_CHECK_NO_THROW(r = CallRPC(strRPC));
    string addr = r.get_str();
    BOOST_CHECK_EQUAL(addr, CBitcoinAddress(pubkey.GetID()).ToString());

    // address found after import
    BOOST_CHECK_THROW(CallRPC(strRPC), runtime_error);
}

BOOST_FIXTURE_TEST_CASE(rpc_listwalletaddress_test, RPCTestWalletFixture)
{
    BOOST_CHECK(pwalletMain != NULL);
    LOCK2(cs_main, pwalletMain->cs_wallet);

    CKey key;
    CPubKey pubkey;
    string strRPC = "";
    string result;

    BOOST_CHECK_NO_THROW(CallRPC("listwalletaddress"));
    BOOST_CHECK_NO_THROW(CallRPC("listwalletaddress -a"));
    BOOST_CHECK_NO_THROW(CallRPC("listwalletaddress -i"));
    BOOST_CHECK_NO_THROW(CallRPC("listwalletaddress -p"));

    // show imported key tests
    key.MakeNewKey(true);
    pubkey = key.GetPubKey();

    // address not found before import
    result = write_string(CallRPC("listwalletaddress"), true);
    BOOST_CHECK(result.find(CBitcoinAddress(pubkey.GetID()).ToString()) == string::npos);

    // import private key
    strRPC = "importprivkey " + CBitcoinSecret(key).ToString();
    CallRPC(strRPC);

    // address found after import
    result = write_string(CallRPC("listwalletaddress -i"), true);
    BOOST_CHECK(result.find(CBitcoinAddress(pubkey.GetID()).ToString()) != string::npos);

    // show address of specific labels test
    key.MakeNewKey(true);
    pubkey = key.GetPubKey();
    strRPC = "importprivkey " + CBitcoinSecret(key).ToString() + " import";
    CallRPC(strRPC);

    result = write_string(CallRPC("listwalletaddress import"), true);
    BOOST_CHECK(result.find(CBitcoinAddress(pubkey.GetID()).ToString()) != string::npos);

    result = write_string(CallRPC("listwalletaddress keypool"), true);
    BOOST_CHECK(result.find(CBitcoinAddress(pubkey.GetID()).ToString()) == string::npos);
}

BOOST_AUTO_TEST_CASE(rpc_createrawtransaction_test)
{
    Value v;
    string strRPC;
    strRPC = "createrawtransaction [{\"txid\":\"eb63d5d53cd906b5cf75a014e1bcf1c0198ae58d378d45dbfa15045ac89a38ac\",\"vout\":1}] [{\"address\":\"1BmjmJttPB66otSHAzxaAANMJWTLL4Axf8\",\"value\":999999999,\"color\":3}]";
    BOOST_CHECK_NO_THROW(v = CallRPC(strRPC));
}

BOOST_AUTO_TEST_CASE(rpc_decoderawtransaction_test)
{
    Value v;
    string strRPC;
    strRPC = "createrawtransaction [{\"txid\":\"eb63d5d53cd906b5cf75a014e1bcf1c0198ae58d378d45dbfa15045ac89a38ac\",\"vout\":1}] [{\"address\":\"1BmjmJttPB66otSHAzxaAANMJWTLL4Axf8\",\"value\":999999999,\"color\":3}]";
    v = CallRPC(strRPC);
    string rawtx = v.get_str();
    CTransaction tx;
    strRPC = "decoderawtransaction " + rawtx;
    BOOST_CHECK_NO_THROW(v = CallRPC(strRPC));
    Array a = find_value(v.get_obj(), "vout").get_array();
    BOOST_CHECK_EQUAL(find_value(a[0].get_obj(), "value").get_int64(), 99999999900000000);
}

BOOST_FIXTURE_TEST_CASE(rpc_sendlicensetoaddress_test, RPCTestWalletFixture)
{
    BOOST_CHECK(pwalletTest != NULL);
    LOCK2(cs_main, pwalletTest->cs_wallet);
    CTxDestination address = CreateDestination();
    string err_msg = "some error";
    type_Color color = 5;
    pwalletTest->setAddress(address);
    pwalletTest->setColor(color);
    pwalletTest->setType(LICENSE);
    pwalletTest->setMisc("72110100206162636465666768696a6b6c6d6e6f707172737475767778797a414243444546286162636465666768696a6b6c6d6e6f707172737475767778797a4142434445464748494a4b4c4d4e206162636465666768696a6b6c6d6e6f707172737475767778797a41424344454601000000000000000000000000223150364b4351733474594363583971376b414b6b63655a456d61786a6a7271774e38640000000000000000000000011568747470733a2f2f676f6f2e676c2f4e725035694fd032fdcdebbfe5e267e933e364e49f7f012e6a01c6203f9a246d8c330cd4a477");
    pwalletTest->setColorAdmin(COIN);
    pwalletTest->setLicense(COIN);
    string strRPC;
    strRPC = "sendlicensetoaddress " + CreateAddress() + " 5 72110100206162636465666768696a6b6c6d6e6f707172737475767778797a414243444546286162636465666768696a6b6c6d6e6f707172737475767778797a4142434445464748494a4b4c4d4e206162636465666768696a6b6c6d6e6f707172737475767778797a41424344454601000000000000000000000000223150364b4351733474594363583971376b414b6b63655a456d61786a6a7271774e38640000000000000000000000011568747470733a2f2f676f6f2e676c2f4e725035694fd032fdcdebbfe5e267e933e364e49f7f012e6a01c6203f9a246d8c330cd4a477";
    BOOST_CHECK_NO_THROW(CallRPC(strRPC));
    CLicenseInfo *pinfo = new CLicenseInfo();
    BOOST_CHECK(plicense->SetOwner(color, "someone", pinfo));
    BOOST_CHECK_THROW(CallRPC(strRPC), runtime_error);
    strRPC = "sendlicensetoaddress " + CreateAddress() + " 5";
    pwalletTest->setMisc("");
    BOOST_CHECK_NO_THROW(CallRPC(strRPC));
    plicense->RemoveColor(color);
    BOOST_CHECK_THROW(CallRPC(strRPC), runtime_error);
    delete pinfo;
}

BOOST_AUTO_TEST_CASE(rpc_encodelicenseinfo_test)
{
    string strRPC = "encodelicenseinfo {\"version\":1,\"name\":\"abcdefghijklmnopqrstuvwxyzABCDEF\",\"description\":\"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMN\",\"issuer\":\"abcdefghijklmnopqrstuvwxyzABCDEF\",\"divisibility\":true,\"fee_type\":\"fixed\",\"fee_rate\":0.0,\"fee_collector\":\"1P6KCQs4tYCcX9q7kAKkceZEmaxjjrqwN8\",\"upper_limit\":100,\"mint_schedule\":\"free\",\"member_control\":true,\"metadata_link\":\"https://goo.gl/NrP5iO\",\"metadata_hash\":\"77a4d40c338c6d249a3f20c6016a2e017f9fe464e333e967e2e5bfebcdfd32d0\"}";
    string r;
    BOOST_CHECK_NO_THROW(r = CallRPC(strRPC).get_str());
    BOOST_CHECK_EQUAL(r, "72110100206162636465666768696a6b6c6d6e6f707172737475767778797a414243444546286162636465666768696a6b6c6d6e6f707172737475767778797a4142434445464748494a4b4c4d4e206162636465666768696a6b6c6d6e6f707172737475767778797a41424344454601000000000000000000000000223150364b4351733474594363583971376b414b6b63655a456d61786a6a7271774e38640000000000000000000000011568747470733a2f2f676f6f2e676c2f4e725035694fd032fdcdebbfe5e267e933e364e49f7f012e6a01c6203f9a246d8c330cd4a477");
}

BOOST_AUTO_TEST_CASE(rpc_decodelicenseinfo_test)
{
    string strRPC = "decodelicenseinfo 72110100206162636465666768696a6b6c6d6e6f707172737475767778797a414243444546286162636465666768696a6b6c6d6e6f707172737475767778797a4142434445464748494a4b4c4d4e206162636465666768696a6b6c6d6e6f707172737475767778797a41424344454601000000000000000000000000223150364b4351733474594363583971376b414b6b63655a456d61786a6a7271774e38640000000000000000000000011568747470733a2f2f676f6f2e676c2f4e725035694fd032fdcdebbfe5e267e933e364e49f7f012e6a01c6203f9a246d8c330cd4a477";
    Object o;
    BOOST_CHECK_NO_THROW(o = CallRPC(strRPC).get_obj());
    BOOST_CHECK_EQUAL(find_value(o, "version").get_int(), 1);
    BOOST_CHECK_EQUAL(find_value(o, "name").get_str(), "abcdefghijklmnopqrstuvwxyzABCDEF");
    BOOST_CHECK_EQUAL(find_value(o, "description").get_str(), "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMN");
    BOOST_CHECK_EQUAL(find_value(o, "issuer").get_str(), "abcdefghijklmnopqrstuvwxyzABCDEF");
    BOOST_CHECK_EQUAL(find_value(o, "divisibility").get_str(), "true");
    BOOST_CHECK_EQUAL(find_value(o, "fee_type").get_str(), "fixed");
    BOOST_CHECK_EQUAL(find_value(o, "fee_rate").get_real(), 0.0);
    BOOST_CHECK_EQUAL(find_value(o, "fee_collector").get_str(), "1P6KCQs4tYCcX9q7kAKkceZEmaxjjrqwN8");
    BOOST_CHECK_EQUAL(find_value(o, "upper_limit").get_int64(), 100);
    BOOST_CHECK_EQUAL(find_value(o, "mint_schedule").get_str(), "free");
    BOOST_CHECK_EQUAL(find_value(o, "member_control").get_str(), "true");
    BOOST_CHECK_EQUAL(find_value(o, "metadata_link").get_str(), "https://goo.gl/NrP5iO");
    BOOST_CHECK_EQUAL(find_value(o, "metadata_hash").get_str(), "77a4d40c338c6d249a3f20c6016a2e017f9fe464e333e967e2e5bfebcdfd32d0");
}

BOOST_FIXTURE_TEST_CASE(rpc_getassetinfo_test, RPCTestFixture)
{
    CLicenseInfo info;
    info.DecodeInfo("72110100206162636465666768696a6b6c6d6e6f707172737475767778797a414243444546286162636465666768696a6b6c6d6e6f707172737475767778797a4142434445464748494a4b4c4d4e206162636465666768696a6b6c6d6e6f707172737475767778797a41424344454601000000000000000000000000223150364b4351733474594363583971376b414b6b63655a456d61786a6a7271774e38640000000000000000000000011568747470733a2f2f676f6f2e676c2f4e725035694fd032fdcdebbfe5e267e933e364e49f7f012e6a01c6203f9a246d8c330cd4a477");
    BOOST_CHECK(plicense->SetOwner(10, "1P6KCQs4tYCcX9q7kAKkceZEmaxjjrqwN8", &info));
    string strRPC = "getassetinfo 10";
    Object o;
    BOOST_CHECK_NO_THROW(o = CallRPC(strRPC).get_obj());
    BOOST_CHECK_EQUAL(find_value(o, "version").get_int(), 1);
    BOOST_CHECK_EQUAL(find_value(o, "name").get_str(), "abcdefghijklmnopqrstuvwxyzABCDEF");
    BOOST_CHECK_EQUAL(find_value(o, "description").get_str(), "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMN");
    BOOST_CHECK_EQUAL(find_value(o, "issuer").get_str(), "abcdefghijklmnopqrstuvwxyzABCDEF");
    BOOST_CHECK_EQUAL(find_value(o, "divisibility").get_str(), "true");
    BOOST_CHECK_EQUAL(find_value(o, "fee_type").get_str(), "fixed");
    BOOST_CHECK_EQUAL(find_value(o, "fee_rate").get_real(), 0.0);
    BOOST_CHECK_EQUAL(find_value(o, "fee_collector").get_str(), "1P6KCQs4tYCcX9q7kAKkceZEmaxjjrqwN8");
    BOOST_CHECK_EQUAL(find_value(o, "upper_limit").get_int64(), 100);
    BOOST_CHECK_EQUAL(find_value(o, "mint_schedule").get_str(), "free");
    BOOST_CHECK_EQUAL(find_value(o, "member_control").get_str(), "true");
    BOOST_CHECK_EQUAL(find_value(o, "metadata_link").get_str(), "https://goo.gl/NrP5iO");
    BOOST_CHECK_EQUAL(find_value(o, "metadata_hash").get_str(), "77a4d40c338c6d249a3f20c6016a2e017f9fe464e333e967e2e5bfebcdfd32d0");

}

BOOST_FIXTURE_TEST_CASE(rpc_hdaddchain_test_1, RPCTestWalletFixture)
{
    string strRPC = "hdaddchain";
    BOOST_CHECK_NO_THROW(CallRPC(strRPC));
}

BOOST_FIXTURE_TEST_CASE(rpc_hdaddchain_test_2, RPCTestWalletFixture)
{
    string strRPC = "hdaddchain whatever";
    BOOST_CHECK_THROW(CallRPC(strRPC), runtime_error);
}

BOOST_FIXTURE_TEST_CASE(rpc_hdaddchain_test_3, RPCTestWalletFixture)
{
    string strRPC = "hdaddchain default e41b6acfbd6d5dbb036ce639177a5c64d6481fe8e2809cef1dd355a01ffe7cf7";
    Object o;
    BOOST_CHECK_NO_THROW(o = CallRPC(strRPC).get_obj());
    BOOST_CHECK_EQUAL(find_value(o, "extended_master_pubkey").get_str(), "xpub661MyMwAqRbcFFRED9976fjeftDpwWkwwKg61DAsSVpttruDjYtaEgKRF1JoDQ96797QNd6nQKDMx6CfZrLDjuMn9PsFqPGKdsnEqbK5ct5");
    BOOST_CHECK_EQUAL(find_value(o, "extended_master_privkey").get_str(), "xprv9s21ZrQH143K2mLm77c6jXnv7rPLY436a6kVCpmFtAHv24a5C1aKgszwPkKBAsxpiUkYAGjSga55Gcw2rMdExjNVg9Utg9mBLKPj8atgEhv");
    BOOST_CHECK_EQUAL(find_value(o, "chainid").get_str(), "76968e0e428f36dd51dec80474c244d8a04023a1da859b6e928129e5137553c9");
}

BOOST_FIXTURE_TEST_CASE(rpc_hdaddchain_test_4, RPCTestWalletFixture)
{
    string strRPC = "hdaddchain default xprv9s21ZrQH143K2mLm77c6jXnv7rPLY436a6kVCpmFtAHv24a5C1aKgszwPkKBAsxpiUkYAGjSga55Gcw2rMdExjNVg9Utg9mBLKPj8atgEhv";
    Object o;
    BOOST_CHECK_NO_THROW(o = CallRPC(strRPC).get_obj());
    BOOST_CHECK_EQUAL(find_value(o, "seed_hex").get_str(), "00000000000000000046f985fa53ee8ad60bb0c0056b3c7382d5ba8bf958e7f10794d34ada3dee9f3f00f54c7377045ec4e7815b6b3f1418f028211bf368ecabead60d943cc916e496b7");
    BOOST_CHECK_EQUAL(find_value(o, "extended_master_pubkey").get_str(), "xpub661MyMwAqRbcFFRED9976fjeftDpwWkwwKg61DAsSVpttruDjYtaEgKRF1JoDQ96797QNd6nQKDMx6CfZrLDjuMn9PsFqPGKdsnEqbK5ct5");
    BOOST_CHECK_EQUAL(find_value(o, "extended_master_privkey").get_str(), "xprv9s21ZrQH143K2mLm77c6jXnv7rPLY436a6kVCpmFtAHv24a5C1aKgszwPkKBAsxpiUkYAGjSga55Gcw2rMdExjNVg9Utg9mBLKPj8atgEhv");
    BOOST_CHECK_EQUAL(find_value(o, "chainid").get_str(), "76968e0e428f36dd51dec80474c244d8a04023a1da859b6e928129e5137553c9");
}

BOOST_FIXTURE_TEST_CASE(rpc_hdsetchain_test_1, RPCTestWalletFixture)
{
    string strRPC = "hdsetchain";
    BOOST_CHECK_THROW(CallRPC(strRPC), runtime_error);
    strRPC = "hdsetchain whatever";
    BOOST_CHECK_THROW(CallRPC(strRPC), runtime_error);
}

BOOST_FIXTURE_TEST_CASE(rpc_hdsetchain_test_2, RPCTestWalletFixture)
{
    string strRPC;
    strRPC = "hdaddchain default bb5dd1ccfe176a516b311f8d26fc2dbfb9344bbc83c34b35f532847a66b930ae";
    CallRPC(strRPC);
    strRPC = "hdaddchain default e41b6acfbd6d5dbb036ce639177a5c64d6481fe8e2809cef1dd355a01ffe7cf7";
    CallRPC(strRPC);
    strRPC = "hdsetchain ae6a950b8e76fd06abe4225897e67d16235f9d6c245ea514eea4b3eb8bfc7391";
    BOOST_CHECK_NO_THROW(CallRPC(strRPC));
    strRPC = "hdsetchain 76968e0e428f36dd51dec80474c244d8a04023a1da859b6e928129e5137553c9";
    BOOST_CHECK_NO_THROW(CallRPC(strRPC));
}

BOOST_FIXTURE_TEST_CASE(rpc_hdgetinfo_test, RPCTestWalletFixture)
{
    string strRPC;
    Array a;
    strRPC = "hdgetinfo";
    BOOST_CHECK_NO_THROW(CallRPC(strRPC));
    CallRPC("hdaddchain default bb5dd1ccfe176a516b311f8d26fc2dbfb9344bbc83c34b35f532847a66b930ae");
    BOOST_CHECK_NO_THROW(a = CallRPC(strRPC).get_array());
    BOOST_CHECK_EQUAL(find_value(a[0].get_obj(), "chainid").get_str(), "ae6a950b8e76fd06abe4225897e67d16235f9d6c245ea514eea4b3eb8bfc7391");
    BOOST_CHECK_EQUAL(find_value(a[0].get_obj(), "chainpath").get_str(), "m/44'/0'/0'/c");
}

BOOST_FIXTURE_TEST_CASE(rpc_hdkeypoolrefill_test, RPCTestWalletFixture)
{
    string strRPC;
    strRPC = "hdkeypoolrefill";
    BOOST_CHECK_THROW(CallRPC(strRPC), runtime_error);
    CallRPC("hdaddchain default bb5dd1ccfe176a516b311f8d26fc2dbfb9344bbc83c34b35f532847a66b930ae");
    BOOST_CHECK_NO_THROW(CallRPC(strRPC));
    BOOST_CHECK_EQUAL(CallRPC("getnewaddress").get_str(), "1PuJ5yq3kh6Ln3K71jfYwiuf8KZk7foHE8");
}

BOOST_AUTO_TEST_SUITE_END()
