// Copyright (c) 2011-2013 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#define BOOST_TEST_MODULE Bitcoin Test Suite

#include "test_bitcoin.h"

#include "key.h"
#include "main.h"
#include "random.h"
#include "txdb.h"
#include "ui_interface.h"
#include "util.h"
#ifdef ENABLE_WALLET
#include "wallet/db.h"
#include "wallet/wallet.h"
#endif

#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/thread.hpp>

using namespace json_spirit;
using namespace std;

CClientUIInterface uiInterface; // Declared but not defined in ui_interface.h
CWallet* pwalletMain;

extern bool fPrintToConsole;
extern void noui_connect();

BasicTestingSetup::BasicTestingSetup()
{
        ECC_Start();
        SetupEnvironment();
        fPrintToDebugLog = false; // don't want to write to debug.log file
        fCheckBlockIndex = true;
        SelectParams(CBaseChainParams::MAIN);
}
BasicTestingSetup::~BasicTestingSetup()
{
        ECC_Stop();
}

TestingSetup::TestingSetup()
{
#ifdef ENABLE_WALLET
        bitdb.MakeMock();
#endif
        ClearDatadirCache();
        pathTemp = GetTempPath() / strprintf("test_gcoin_%lu_%i", (unsigned long)GetTime(), (int)(GetRand(100000)));
        boost::filesystem::create_directories(pathTemp);
        mapArgs["-datadir"] = pathTemp.string();
        pblocktree = new CBlockTreeDB(1 << 20, true);
        pcoinsdbview = new CCoinsViewDB(1 << 23, true);
        pcoinsTip = new CCoinsViewCache(pcoinsdbview);
        InitBlockIndex();
#ifdef ENABLE_WALLET
        bool fFirstRun;
        pwalletMain = new CWallet("wallet.dat");
        pwalletMain->LoadWallet(fFirstRun);
        RegisterValidationInterface(pwalletMain);
#endif
        nScriptCheckThreads = 3;
        for (int i=0; i < nScriptCheckThreads-1; i++)
            threadGroup.create_thread(&ThreadScriptCheck);
        RegisterNodeSignals(GetNodeSignals());
}

TestingSetup::~TestingSetup()
{
        UnregisterNodeSignals(GetNodeSignals());
        threadGroup.interrupt_all();
        threadGroup.join_all();
#ifdef ENABLE_WALLET
        UnregisterValidationInterface(pwalletMain);
        delete pwalletMain;
        pwalletMain = NULL;
#endif
        UnloadBlockIndex();
        delete pcoinsTip;
        delete pcoinsdbview;
        delete pblocktree;
#ifdef ENABLE_WALLET
        bitdb.Flush(true);
        bitdb.Reset();
#endif
        boost::filesystem::remove_all(pathTemp);
}

void Shutdown(void* parg)
{
  exit(0);
}

void StartShutdown()
{
  exit(0);
}

bool ShutdownRequested()
{
  return false;
}

map<uint256, CMutableTransaction> transactions;

type_transaction_handler::HandlerInterface *handler;

/*!
 * @brief An alternate function for `GetTransaction()` in the file main.cpp
 */
bool GetTransaction_UnitTest(
        const uint256 &tx_hash, CTransaction &result,
        uint256 &block_hash, const CBlock *block, bool allow_slow)
{
    map<uint256, CMutableTransaction>::iterator it
        = transactions.find(tx_hash);
    if (it == transactions.end()) {
        return false;
    }
    result = CTransaction(it->second);
    return true;
}

bool GetCoinsFromCache_UnitTest(const COutPoint &outpoint,
        CCoins &coins,
        bool fUseMempool)
{
    map<uint256, CMutableTransaction>::iterator it
        = transactions.find(outpoint.hash);
    if (it == transactions.end()) {
        return false;
    }

    coins = CCoins(CTransaction(it->second), 1);
    return true;
}

bool CheckTxFeeAndColor_UnitTest(const CTransaction tx)
{
    return true;
}

/*!
 * @brief Creates an transaction for unit test.
 * @param [in] tx_hash The hash value of the transaction to be created.
 * @param [in] type The type of the transaction.
 * @param [in] color The color of the transaction.
 */
void CreateTransaction(
        const uint256 &tx_hash, const tx_type &type)
{
    CMutableTransaction tx;
    tx.type = type;
    transactions[tx_hash] = tx;
}


/*!
 * @brief Connects two transactions for unit test.
 * @param [in] src_hash The hash value of the source transaction.
 * @param [in] dst_hash The hash value of the destination transaction.
 * @param [in] value Amount of coins to transform.
 * @param [in] address The destination address.
 */
void ConnectTransactions(const uint256 &src_hash,
        const uint256 &dst_hash,
        int64_t value,
        const string &address,
        const type_Color &color,
        const string &misc)
{
    CScript address_script;
    address_script = GetScriptForDestination(CBitcoinAddress(address).Get());

    size_t index = transactions[src_hash].vout.size();

    transactions[src_hash].vout.push_back(CTxOut(value, address_script, color));
    if (transactions[src_hash].type == LICENSE && !misc.empty()) {
        CScript scriptMessage;
        vector<unsigned char> msg(misc.begin(), misc.end());
        scriptMessage = CScript() << OP_RETURN << msg;
        CTxOut op_return(0, scriptMessage, color);
        transactions[src_hash].vout.push_back(op_return);
    }
    transactions[dst_hash].vin.push_back(CTxIn(COutPoint(src_hash, index)));
}

/*!
 * @brief Creates a random key
 */

CPubKey GenerateNewKey()
{
    CKey secret;
    secret.MakeNewKey(true);

    CPubKey pubkey = secret.GetPubKey();

    return pubkey;
}


/*!
 * @brief Creates a valid bitcoin address.
 */
string CreateAddress()
{
    return CBitcoinAddress(GenerateNewKey().GetID()).ToString();
}


/*!
 * @brief Creates a valid bitcoin transaction destination.
 */
CTxDestination CreateDestination()
{
    return CBitcoinAddress(GenerateNewKey().GetID()).Get();
}

Array createArgs(int nRequired, const char* address1, const char* address2)
{
    Array result;
    result.push_back(nRequired);
    Array addresses;
    if (address1) addresses.push_back(address1);
    if (address2) addresses.push_back(address2);
    result.push_back(addresses);
    return result;
}

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
    } catch (Object& objError) {
        throw runtime_error(find_value(objError, "message").get_str());
    }
}

DBErrors CWallet_UnitTest::LoadWallet(bool& fFirstRunRet)
{
    if (!fFileBacked)
        return DB_LOAD_OK;
    fFirstRunRet = false;
    DBErrors nLoadWalletRet = CWalletDB(strWalletFile,"cr").LoadWallet(this);
    if (nLoadWalletRet == DB_NEED_REWRITE) {
        if (CDB::Rewrite(strWalletFile, "\x04pool")) {
            LOCK(cs_wallet);
            setKeyPool.clear();
            // Note: can't top-up keypool here, because wallet is locked.
            // User will be prompted to unlock wallet the next operation
            // the requires a new key.
        }
    }

    if (nLoadWalletRet != DB_LOAD_OK)
        return nLoadWalletRet;
    fFirstRunRet = !vchDefaultKey.IsValid();

    uiInterface.LoadWallet(this);

    return DB_LOAD_OK;
}

bool CWallet_UnitTest::CheckMapValueExpected_(const mapValue_t &tx_map_value)
{
    if (tx_map_value.size() != expected_map_values_.size()) {
        return false;
    }
    for (mapValue_t::const_iterator it2, it = tx_map_value.begin();
            it != tx_map_value.end(); ++it) {
        it2 = expected_map_values_.find(it->first);
        if (it2 == expected_map_values_.end() ||
                it2->second != it->second) {
            return false;
        }
    }
    return true;
}
