// Copyright (c) 2012-2014 The Bitcoin Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include "rpcserver.h"
#include "rpcclient.h"
#include "init.h"
#include "base58.h"
#include "wallet.h"
#include "key.h"

#include <set>
#include <stdint.h>
#include <utility>
#include <vector>

#include <boost/foreach.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/test/unit_test.hpp>

using namespace std;
using namespace json_spirit;

Value CallRPC(string args)
{
    vector<string> vArgs;
    boost::split(vArgs, args, boost::is_any_of(" \t"));
    string strMethod = vArgs[0];
    vArgs.erase(vArgs.begin());
    Array params = RPCConvertValues(strMethod, vArgs);

    rpcfn_type method = tableRPC[strMethod]->actor;
    try {
        Value result = (*method)(params, false);
        return result;
    }
    catch (Object& objError)
    {
        throw runtime_error(find_value(objError, "message").get_str());
    }
}

extern CWallet* pwalletMain;

BOOST_AUTO_TEST_SUITE(listaddr_tests)

BOOST_AUTO_TEST_CASE(rpc_listaddr_tests)
{
// Test listwalletaddress
    BOOST_CHECK(pwalletMain != NULL);

    LOCK2(cs_main, pwalletMain->cs_wallet);
    BOOST_CHECK_NO_THROW(CallRPC("listwalletaddress"));
    BOOST_CHECK_NO_THROW(CallRPC("listwalletaddress -a"));
    BOOST_CHECK_NO_THROW(CallRPC("listwalletaddress -i"));
    BOOST_CHECK_NO_THROW(CallRPC("listwalletaddress -p"));

    CKey key;
    CPubKey pubkey;

    string strRPC = "";
    string result;

    // show imported key tests
    key.MakeNewKey(true);
    pubkey = key.GetPubKey();

    // address not found before import
    result = write_string(CallRPC("listwalletaddress"), true);
    BOOST_CHECK(result.find(CBitcoinAddress(pubkey.GetID()).ToString()) == string::npos);

    // import private key
    strRPC = "importprivkey " + CBitcoinSecret(key).ToString();
    BOOST_CHECK_NO_THROW(CallRPC(strRPC));

    // address found after import
    result = write_string(CallRPC("listwalletaddress -i"), true);
    BOOST_CHECK(result.find(CBitcoinAddress(pubkey.GetID()).ToString()) != string::npos);

    // show address of specific labels test
    key.MakeNewKey(true);
    pubkey = key.GetPubKey();

    strRPC = "importprivkey " + CBitcoinSecret(key).ToString() + " import";
    BOOST_CHECK_NO_THROW(CallRPC(strRPC));

    result = write_string(CallRPC("listwalletaddress import"), true);
    BOOST_CHECK(result.find(CBitcoinAddress(pubkey.GetID()).ToString()) != string::npos);

    result = write_string(CallRPC("listwalletaddress keypool"), true);
    BOOST_CHECK(result.find(CBitcoinAddress(pubkey.GetID()).ToString()) == string::npos);
}

BOOST_AUTO_TEST_SUITE_END()
