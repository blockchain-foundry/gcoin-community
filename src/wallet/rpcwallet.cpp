// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Copyright (c) 2014-2016 The Gcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "amount.h"
#include "base58.h"
#include "core_io.h"
#include "init.h"
#include "main.h"
#include "net.h"
#include "netbase.h"
#include "rpcserver.h"
#include "timedata.h"
#include "util.h"
#include "utilmoneystr.h"
#include "wallet/wallet.h"
#include "wallet/walletdb.h"
#include "primitives/transaction.h"
#include "policy/licenseinfo.h"
#include "utilstrencodings.h"

#include <stdint.h>

#include <boost/algorithm/string/replace.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/thread.hpp>
#include <boost/lexical_cast.hpp>
#include "json/json_spirit_utils.h"
#include "json/json_spirit_value.h"

using namespace std;
using namespace json_spirit;

int64_t nWalletUnlockTime;
static CCriticalSection cs_nWalletUnlockTime;

string HelpRequiringPassphrase()
{
    return pwalletMain && pwalletMain->IsCrypted()
        ? "\nRequires wallet passphrase to be set with walletpassphrase call."
        : "";
}

bool EnsureWalletIsAvailable(bool avoidException)
{
    if (!pwalletMain)
    {
        if (!avoidException)
            throw JSONRPCError(RPC_METHOD_NOT_FOUND, "Method not found (disabled)");
        else
            return false;
    }
    return true;
}

void EnsureWalletIsUnlocked()
{
    if (pwalletMain->IsLocked())
        throw JSONRPCError(RPC_WALLET_UNLOCK_NEEDED, "Error: Please enter the wallet passphrase with walletpassphrase first.");
}

void WalletTxToJSON(const CWalletTx& wtx, Object& entry)
{
    int confirms = wtx.GetDepthInMainChain();
    entry.push_back(Pair("confirmations", confirms));
    if (wtx.IsCoinBase())
        entry.push_back(Pair("generated", true));
    if (confirms > 0) {
        entry.push_back(Pair("blockhash", wtx.hashBlock.GetHex()));
        entry.push_back(Pair("blockindex", wtx.nIndex));
        entry.push_back(Pair("blocktime", mapBlockIndex[wtx.hashBlock]->GetBlockTime()));
    }
    uint256 hash = wtx.GetHash();
    entry.push_back(Pair("txid", hash.GetHex()));
    Array conflicts;
    BOOST_FOREACH(const uint256& conflict, wtx.GetConflicts())
        conflicts.push_back(conflict.GetHex());
    entry.push_back(Pair("walletconflicts", conflicts));
    entry.push_back(Pair("time", wtx.GetTxTime()));
    entry.push_back(Pair("timereceived", (int64_t)wtx.nTimeReceived));
    BOOST_FOREACH(const PAIRTYPE(string,string)& item, wtx.mapValue)
        entry.push_back(Pair(item.first, item.second));
}

// Create JSON object from given license information.
void LicenseInfoToJSON(const CLicenseInfo& info, Object& entry)
{
    entry.push_back(Pair("version", info.nVersion));
    entry.push_back(Pair("name", info.name));
    entry.push_back(Pair("description", info.description));
    entry.push_back(Pair("issuer", info.issuer));
    entry.push_back(Pair("divisibility", info.fDivisibility));
    if (info.feeType == FIXED)
        entry.push_back(Pair("fee_type", "fixed"));
    else if (info.feeType == BYSIZE)
        entry.push_back(Pair("fee_type", "by_size"));
    else if (info.feeType == BYAMOUNT)
        entry.push_back(Pair("fee_type", "by_amount"));
    entry.push_back(Pair("fee_rate", info.nFeeRate));
    entry.push_back(Pair("fee_collector", info.feeCollectorAddr));
    entry.push_back(Pair("upper_limit", info.nLimit));
    if (info.mintSchedule == FREE)
        entry.push_back(Pair("mint_schedule", "free"));
    else if (info.mintSchedule == ONCE)
        entry.push_back(Pair("mint_schedule", "once"));
    else if (info.mintSchedule == LINEAR)
        entry.push_back(Pair("mint_schedule", "linear"));
    else if (info.mintSchedule == HALFLIFE)
        entry.push_back(Pair("mint_schedule", "half_life"));
    entry.push_back(Pair("member_control", info.fMemberControl));
    entry.push_back(Pair("metadata_link", info.metadataLink));
    entry.push_back(Pair("metadata_hash", info.metadataHash.ToString()));
}

string AccountFromValue(const Value& value)
{
    string strAccount = value.get_str();
    if (strAccount == "*")
        throw JSONRPCError(RPC_WALLET_INVALID_ACCOUNT_NAME, "Invalid account name");
    return strAccount;
}

// Return the default address for current wallet.
Value getfixedaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 1)
        throw runtime_error(
            _(__func__) + "( \"account\" )\n"
            "\nReturns the default Gcoin address for receiving payments.\n"
            "\nResult:\n"
            "\"address\"    (string) The default gcoin address\n"
            "\nExamples:\n"
            + HelpExampleCli("getfixedaddress", "")
            + HelpExampleRpc("getfixedaddress", "")
        );

    CKeyID keyID = pwalletMain->vchDefaultKey.GetID();

    return CBitcoinAddress(keyID).ToString();
}

// Return the default address for current wallet.
Value assignfixedaddress(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            _(__func__) + "( \"account\" )\n"
            "\nAssign the default Gcoin address.\n"
            "\nArguments:\n"
            "1. \"address\"     (string, required) The address to be assigned as the default address.\n"
            "\nResult:\n"
            "\"address\"    (string) The default gcoin address\n"
            "\nExamples:\n"
            + HelpExampleCli("assignfixedaddress", "")
            + HelpExampleCli("assignfixedaddress", "address")
            + HelpExampleRpc("assignfixedaddress", "address")
        );

    std::string str = params[0].get_str();
    CPubKey newDefaultKey;
    CKeyID keyID;
    CBitcoinAddress address;
    if (address.SetString(str)) {
        address.GetKeyID(keyID);
    } else {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Gcoin address or key");
    }

    if (!pwalletMain->GetKeyFromPool(newDefaultKey, address)) {
        if (!pwalletMain->GetPubKey(keyID, newDefaultKey)) {
            throw JSONRPCError(RPC_WALLET_ERROR, "Public key for address " + str + " is not known");
        }
    }

    if (newDefaultKey.IsValid()) {
        pwalletMain->SetDefaultKey(newDefaultKey);
        keyID = pwalletMain->vchDefaultKey.GetID();
        if (!pwalletMain->SetAddressBook(keyID, "", "receive"))
            throw JSONRPCError(RPC_WALLET_ERROR, "Cannot write default address");
    }

    return str;
}

// Get a specific amount of new address.
Value getnewaddressamount(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            _(__func__) + "( \"account\" )\n"
            "\nReturns given amount of new Gcoin addresses for receiving payments.\n"
            "If 'account' is specified (recommended), it is added to the address book \n"
            "so payments received with the address will be credited to 'account'.\n"
            "\nArguments:\n"
            "1. \"number\"      (int) The number of address to be fetched from the keypool.\n"
            "2. \"account\"     (string, optional) The account name for the address to be linked to. if not provided, the default account \"\" is used. It can also be set to the empty string \"\" to represent the default account. The account does not need to exist, it will be created if there is no account by the given name.\n"
            "\nResult:\n"
            "[                     (json array of string)\n"
            "  \"address\"      (string) The new gcoin address\n"
            "  ,...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("getnewaddressamount", "\"number\"")
            + HelpExampleCli("getnewaddressamount", "\"number\" \"\"")
            + HelpExampleCli("getnewaddressamount", "\"number\" \"myaccount\"")
            + HelpExampleRpc("getnewaddressamount", "\"number\" \"myaccount\"")
        );

    // Parse the account first so we don't generate a key if there's an error
    Array a;
    string strAccount;
    unsigned int number = params[0].get_int();
    unsigned int keypoolSize = pwalletMain->GetKeyPoolSize();
    if (params.size() > 1)
        strAccount = AccountFromValue(params[1]);
    if (number > keypoolSize)
        throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT, "Error: Keypool not enough, only " + boost::lexical_cast<string>(keypoolSize) + " left");

    // Generate a new key that is added to wallet
    for (unsigned int i = 0; i < number; i++) {
        CPubKey newKey;
        if (!pwalletMain->GetKeyFromPool(newKey))
            throw JSONRPCError(RPC_WALLET_ERROR, "Error: Error fetching key");
        CKeyID keyID = newKey.GetID();
        pwalletMain->SetAddressBook(keyID, strAccount, "receive");
        a.push_back(CBitcoinAddress(keyID).ToString());
        keypoolSize--;
    }

    // notify an external script when keypool size is low
    unsigned int notifySize = GetArg("keypoolnotifysize", 100);
    string strCmd = GetArg("keypoolnotify", "");

    if (!strCmd.empty() && keypoolSize < notifySize) {
        boost::replace_all(strCmd, "%d", boost::lexical_cast<string>(keypoolSize));
        boost::thread t(runCommand, strCmd); // thread runs free
    }

    return a;
}

Value getnewaddress(const Array& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return Value::null;

    if (fHelp || params.size() > 1)
        throw runtime_error(
            _(__func__) + "( \"account\" )\n"
            "\nReturns a new Gcoin address for receiving payments.\n"
            "If 'account' is specified (DEPRECATED), it is added to the address book \n"
            "so payments received with the address will be credited to 'account'.\n"
            "\nArguments:\n"
            "1. \"account\"        (string, optional) DEPRECATED. The account name for the address to be linked to. If not provided, the default account \"\" is used. It can also be set to the empty string \"\" to represent the default account. The account does not need to exist, it will be created if there is no account by the given name.\n"
            "\nResult:\n"
            "\"address\"    (string) The new gcoin address\n"
            "\nExamples:\n"
            + HelpExampleCli("getnewaddress", "")
            + HelpExampleRpc("getnewaddress", "")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    // Parse the account first so we don't generate a key if there's an error
    string strAccount;
    if (params.size() > 0)
        strAccount = AccountFromValue(params[0]);


    // Generate a new key that is added to wallet
    CPubKey newKey;

    if (!pwalletMain->GetKeyFromPool(newKey))
        throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT, "Error: Keypool ran out, please call keypoolrefill first");
    CKeyID keyID = newKey.GetID();

    pwalletMain->SetAddressBook(keyID, strAccount, "receive");

    return CBitcoinAddress(keyID).ToString();
}


CBitcoinAddress GetAccountAddress(string strAccount, bool bForceNew=false)
{
    CWalletDB walletdb(pwalletMain->strWalletFile);

    CAccount account;
    walletdb.ReadAccount(strAccount, account);

    bool bKeyUsed = false;

    // Check if the current key has been used
    if (account.vchPubKey.IsValid()) {
        CScript scriptPubKey = GetScriptForDestination(account.vchPubKey.GetID());
        for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin();
             it != pwalletMain->mapWallet.end() && account.vchPubKey.IsValid();
             ++it) {
            const CWalletTx& wtx = (*it).second;
            BOOST_FOREACH(const CTxOut& txout, wtx.vout)
                if (txout.scriptPubKey == scriptPubKey)
                    bKeyUsed = true;
        }
    }

    // Generate a new key
    if (!account.vchPubKey.IsValid() || bForceNew || bKeyUsed) {
        CReserveKey reservekey(pwalletMain);
        if (!reservekey.GetReservedKey(account.vchPubKey))
            throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT, "Error: Keypool ran out, please call keypoolrefill first");

        pwalletMain->SetAddressBook(account.vchPubKey.GetID(), strAccount, "receive");
        walletdb.WriteAccount(strAccount, account);
    }

    return CBitcoinAddress(account.vchPubKey.GetID());
}

Value getaccountaddress(const Array& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return Value::null;

    if (fHelp || params.size() != 1)
        throw runtime_error(
            "getaccountaddress \"account\"\n"
            "\nDEPRECATED. Returns the current Gcoin address for receiving payments to this account.\n"
            "\nArguments:\n"
            "1. \"account\"       (string, required) The account name for the address. It can also be set to the empty string \"\" to represent the default account. The account does not need to exist, it will be created and a new address created if there is no account by the given name.\n"
            "\nResult:\n"
            "\"address\"   (string) The account gcoin address\n"
            "\nExamples:\n"
            + HelpExampleCli("getaccountaddress", "")
            + HelpExampleCli("getaccountaddress", "\"\"")
            + HelpExampleCli("getaccountaddress", "\"myaccount\"")
            + HelpExampleRpc("getaccountaddress", "\"myaccount\"")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    // Parse the account first so we don't generate a key if there's an error
    string strAccount = AccountFromValue(params[0]);

    Value ret;

    ret = GetAccountAddress(strAccount).ToString();
    return ret;
}


Value getrawchangeaddress(const Array& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return Value::null;

    if (fHelp || params.size() > 1)
        throw runtime_error(
            _(__func__) + "\n"
            "\nReturns a new Gcoin address, for receiving change.\n"
            "This is for use with raw transactions, NOT normal use.\n"
            "\nResult:\n"
            "\"address\"    (string) The address\n"
            "\nExamples:\n"
            + HelpExampleCli("getrawchangeaddress", "")
            + HelpExampleRpc("getrawchangeaddress", "")
       );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    CReserveKey reservekey(pwalletMain);
    CPubKey vchPubKey;
    if (!reservekey.GetReservedKey(vchPubKey))
        throw JSONRPCError(RPC_WALLET_KEYPOOL_RAN_OUT, "Error: Keypool ran out, please call keypoolrefill first");

    reservekey.KeepKey();

    CKeyID keyID = vchPubKey.GetID();

    return CBitcoinAddress(keyID).ToString();
}


Value setaccount(const Array& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return Value::null;

    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "setaccount \"address\" \"account\"\n"
            "\nDEPRECATED. Sets the account associated with the given address.\n"
            "\nArguments:\n"
            "1. \"address\"         (string, required) The gcoin address to be associated with an account.\n"
            "2. \"account\"         (string, required) The account to assign the address to.\n"
            "\nExamples:\n"
            + HelpExampleCli("setaccount", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\" \"tabby\"")
            + HelpExampleRpc("setaccount", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\", \"tabby\"")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    CBitcoinAddress address(params[0].get_str());
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Gcoin address");

    string strAccount;
    if (params.size() > 1)
        strAccount = AccountFromValue(params[1]);

    // Only add the account if the address is yours.
    if (IsMine(*pwalletMain, address.Get())) {
        // Detect when changing the account of an address that is the 'unused current key' of another account:
        if (pwalletMain->mapAddressBook.count(address.Get())) {
            string strOldAccount = pwalletMain->mapAddressBook[address.Get()].name;
            if (address == GetAccountAddress(strOldAccount))
                GetAccountAddress(strOldAccount, true);
        }
        pwalletMain->SetAddressBook(address.Get(), strAccount, "receive");
    }
    else
        throw JSONRPCError(RPC_MISC_ERROR, "setaccount can only be used with own address");

    return Value::null;
}


Value getaccount(const Array& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return Value::null;

    if (fHelp || params.size() != 1)
        throw runtime_error(
            "getaccount \"address\"\n"
            "\nDEPRECATED. Returns the account associated with the given address.\n"
            "\nArguments:\n"
            "1. \"address\"  (string, required) The gcoin address for account lookup.\n"
            "\nResult:\n"
            "\"accountname\"        (string) the account address\n"
            "\nExamples:\n"
            + HelpExampleCli("getaccount", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\"")
            + HelpExampleRpc("getaccount", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\"")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    CBitcoinAddress address(params[0].get_str());
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Gcoin address");

    string strAccount;
    map<CTxDestination, CAddressBookData>::iterator mi = pwalletMain->mapAddressBook.find(address.Get());
    if (mi != pwalletMain->mapAddressBook.end() && !(*mi).second.name.empty())
        strAccount = (*mi).second.name;
    return strAccount;
}


Value getaddressesbyaccount(const Array& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return Value::null;

    if (fHelp || params.size() != 1)
        throw runtime_error(
            "getaddressesbyaccount \"account\"\n"
            "\nDEPRECATED. Returns the list of addresses for the given account.\n"
            "\nArguments:\n"
            "1. \"account\"  (string, required) The account name.\n"
            "\nResult:\n"
            "[                     (json array of string)\n"
            "  \"address\"  (string) a gcoin address associated with the given account\n"
            "  ,...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("getaddressesbyaccount", "\"tabby\"")
            + HelpExampleRpc("getaddressesbyaccount", "\"tabby\"")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    string strAccount = AccountFromValue(params[0]);

    // Find all addresses that have the given account
    Array ret;
    BOOST_FOREACH(const PAIRTYPE(CBitcoinAddress, CAddressBookData)& item, pwalletMain->mapAddressBook) {
        const CBitcoinAddress& address = item.first;
        const string& strName = item.second.name;
        if (strName == strAccount)
            ret.push_back(address.ToString());
    }
    return ret;
}

static void SendLicense(const CTxDestination& address, const type_Color& color, CWalletTx& wtxNew)
{
    if (!plicense->IsColorExist(color))
        throw JSONRPCError(RPC_WALLET_ERROR, "License is not created yet. Please give license info if you are creating a new license.");

    // Check amount
    CAmount curBalance = pwalletMain->GetSendLicenseBalance(color);
    if (SEND_TYPE_AMOUNT > curBalance)
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Insufficient license funds");

    // Parse Gcoin address
    CScript scriptPubKey = GetScriptForDestination(address);

    // Create and send the transaction
    CReserveKey reservekey(pwalletMain);
    std::string strError;
    vector<CRecipient> vecSend;
    CRecipient recipient = {scriptPubKey, COIN, false};
    vecSend.push_back(recipient);
    if (!pwalletMain->CreateTypeTransaction(vecSend, color, LICENSE, wtxNew, strError, "")) {
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }
    if (!pwalletMain->CommitTransaction(wtxNew, reservekey))
        throw JSONRPCError(RPC_WALLET_ERROR, "Error: The license transaction was rejected! Please read debug.info.");
}

static void CreateLicense(const CTxDestination &address, const type_Color color, const string &info, CWalletTx& wtxNew)
{
    if (!IsValidColor(color))
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid color");

    if (plicense->IsColorExist(color))
        throw JSONRPCError(RPC_WALLET_ERROR, "License is already created. Please remove the license info if you are about to transfer your license.");

    CAmount curBalance = pwalletMain->GetColor0Balance();
    if (SEND_TYPE_AMOUNT > curBalance)
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Insufficient license funds");

    // Parse Gcoin address
    CScript scriptPubKey = GetScriptForDestination(address);

    // Check the license info
    CLicenseInfo licenseInfo;
    if (!licenseInfo.DecodeInfo(info)) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Error: Decode license info failed");
    }

    // Create and send the transaction
    CReserveKey reservekey(pwalletMain);
    std::string strError;
    vector<CRecipient> vecSend;
    CRecipient recipient = {scriptPubKey, COIN, false};
    vecSend.push_back(recipient);
    if (!pwalletMain->CreateTypeTransaction(vecSend, color, LICENSE, wtxNew, strError, info)) {
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }
    if (!pwalletMain->CommitTransaction(wtxNew, reservekey))
        throw JSONRPCError(RPC_WALLET_ERROR, "Error: The license transaction was rejected! Please read debug.info.");
}

static void SendVote(const CTxDestination& address, CWalletTx& wtxNew)
{
    CAmount curBalance = pwalletMain->GetVoteBalance();

    // Check amount
    if (SEND_TYPE_AMOUNT > curBalance)
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Insufficient vote funds");

    // Parse Gcoin address
    CScript scriptPubKey = GetScriptForDestination(address);

    // Create and send the transaction
    CReserveKey reservekey(pwalletMain);
    std::string strError;
    vector<CRecipient> vecSend;
    CRecipient recipient = {scriptPubKey, COIN, false};
    vecSend.push_back(recipient);
    if (!pwalletMain->CreateTypeTransaction(vecSend, 0, VOTE, wtxNew, strError)) {
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }
    if (!pwalletMain->CommitTransaction(wtxNew, reservekey))
        throw JSONRPCError(RPC_WALLET_ERROR, "Error: The vote transaction was rejected! Please read debug.info.");
}

<<<<<<< HEAD
static void SendBanVote(const CTxDestination& address, CWalletTx& wtxNew)
{
    CAmount curBalance = pwalletMain->GetVoteBalance();

    // Check amount
    if (SEND_TYPE_AMOUNT > curBalance)
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Insufficient ban-vote funds");

    // Parse Gcoin address
    CScript scriptPubKey = GetScriptForDestination(address);

    // Create and send the transaction
    CReserveKey reservekey(pwalletMain);
    std::string strError;
    vector<CRecipient> vecSend;
    CRecipient recipient = {scriptPubKey, COIN, false};
    vecSend.push_back(recipient);
    if (!pwalletMain->CreateTypeTransaction(vecSend, 0, BANVOTE, wtxNew, strError)) {
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }
    if (!pwalletMain->CommitTransaction(wtxNew, reservekey))
        throw JSONRPCError(RPC_WALLET_ERROR, "Error: The ban-vote transaction was rejected! Please read debug.info.");
}

static void AddMiner(const CTxDestination& address, CWalletTx& wtxNew)
{
    CAmount curBalance = pwalletMain->GetVoteBalance();

    // Check amount
    if (SEND_TYPE_AMOUNT > curBalance)
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Insufficient vote funds");

    // Parse Gcoin address
    CScript scriptPubKey = GetScriptForDestination(address);

    // Create and send the transaction
    CReserveKey reservekey(pwalletMain);
    std::string strError;
    vector<CRecipient> vecSend;
    CRecipient recipient = {scriptPubKey, COIN, false};
    vecSend.push_back(recipient);
    if (!pwalletMain->CreateTypeTransaction(vecSend, 0, MINER, wtxNew, strError)) {
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }
    if (!pwalletMain->CommitTransaction(wtxNew, reservekey))
        throw JSONRPCError(RPC_WALLET_ERROR, "Error: The vote transaction was rejected! Please read debug.info.");
}

static void RevokeMiner(const CTxDestination& address, CWalletTx& wtxNew)
{
    CAmount curBalance = pwalletMain->GetVoteBalance();

    // Check amount
    if (SEND_TYPE_AMOUNT > curBalance)
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Insufficient vote funds");

    // Parse Gcoin address
    CScript scriptPubKey = GetScriptForDestination(address);

    // Create and send the transaction
    CReserveKey reservekey(pwalletMain);
    std::string strError;
    vector<CRecipient> vecSend;
    CRecipient recipient = {scriptPubKey, COIN, false};
    vecSend.push_back(recipient);
    if (!pwalletMain->CreateTypeTransaction(vecSend, 0, DEMINER, wtxNew, strError)) {
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }
    if (!pwalletMain->CommitTransaction(wtxNew, reservekey))
        throw JSONRPCError(RPC_WALLET_ERROR, "Error: The vote transaction was rejected! Please read debug.info.");
}

static void SendMoneyFromFixedAddress(const string& strFromAddress, const CTxDestination& address, CAmount nValue, const type_Color& color, bool fSubtractFeeFromAmount, CWalletTx& wtxNew, const string& feeFromAddress = "")
{
    CAmount curBalance = pwalletMain->GetColorBalanceFromFixedAddress(strFromAddress, color);

    // Check amount
    if (nValue <= 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid amount");
    if (!IsValidColor(color))
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid color");

    if (nValue > curBalance)
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Insufficient funds from this address");

    // Parse Gcoin address
    CScript scriptPubKey = GetScriptForDestination(address);

    // Create and send the transaction
    CReserveKey reservekey(pwalletMain);
    CAmount nFeeRequired;
    std::string strError;
    vector<CRecipient> vecSend;
    int nChangePosRet = -1;
    CRecipient recipient = {scriptPubKey, nValue, fSubtractFeeFromAmount};
    vecSend.push_back(recipient);
    if (!pwalletMain->CreateTransaction(vecSend, color, wtxNew, reservekey, nFeeRequired, nChangePosRet, strError, NULL, strFromAddress, feeFromAddress)) {
        if (!fSubtractFeeFromAmount && nValue + nFeeRequired > pwalletMain->GetColorBalanceFromFixedAddress(strFromAddress, color))
            strError = strprintf("Error: This transaction requires a transaction fee of at least %s because of its amount, complexity, or use of recently received funds!", FormatMoney(nFeeRequired));
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }
    if (!pwalletMain->CommitTransaction(wtxNew, reservekey))
        throw JSONRPCError(RPC_WALLET_ERROR, "Error: The transaction was rejected! Please read debug.info.");
}

static void SendMoney(const CTxDestination& address, CAmount nValue, const type_Color& color, bool fSubtractFeeFromAmount, CWalletTx& wtxNew)
{
    CAmount curBalance = pwalletMain->GetColorBalance(color);

    // Check amount
    if (nValue <= 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid amount");
    if (!IsValidColor(color))
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid color");

    if (nValue > curBalance)
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Insufficient funds");

    // Parse Gcoin address
    CScript scriptPubKey = GetScriptForDestination(address);

    // Create and send the transaction
    CReserveKey reservekey(pwalletMain);
    CAmount nFeeRequired;
    std::string strError;
    vector<CRecipient> vecSend;
    int nChangePosRet = -1;
    CRecipient recipient = {scriptPubKey, nValue, fSubtractFeeFromAmount};
    vecSend.push_back(recipient);
    if (!pwalletMain->CreateTransaction(vecSend, color, wtxNew, reservekey, nFeeRequired, nChangePosRet, strError)) {
        if (!fSubtractFeeFromAmount && nValue + nFeeRequired > pwalletMain->GetColorBalance(color))
            strError = strprintf("Error: This transaction requires a transaction fee of at least %s because of its amount, complexity, or use of recently received funds!", FormatMoney(nFeeRequired));
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }
    if (!pwalletMain->CommitTransaction(wtxNew, reservekey))
        throw JSONRPCError(RPC_WALLET_ERROR, "Error: The transaction was rejected! Please read debug.info.");
}

Value sendlicensetoaddress(const Array& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return Value::null;

    if (fHelp || params.size() < 2 || params.size() > 4)
        throw runtime_error(
            "sendlicensetoaddress \"address\" color ( \"comment\" \"comment-to\" )\n"
            "\nSent a license transaction to a given address.\n"
            + HelpRequiringPassphrase() +
            "\nArguments:\n"
            "1. \"address\"     (string, required) The gcoin address to send to.\n"
            "2. \"color\"       (numeric, required) The color of the license.\n"
            "3. \"licenseinfo\"       (string, optional) The license info string of the color\n"
            "\nResult:\n"
            "\"transactionid\"  (string) The transaction id.\n"
            "\nExamples:\n"
            + HelpExampleCli("sendlicensetoaddress", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 1")
            + HelpExampleCli("sendlicensetoaddress", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 1 \"72110100046e616d650b6465736372697074696f6e0669737375657204747970650001000000000000000000000000223147317453715634576a737a706e4e633873346a7731345336595461396f4671416b0004687474700100000000000000\"")
            + HelpExampleRpc("sendlicensetoaddress", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", 1, \"72110100046e616d650b6465736372697074696f6e0669737375657204747970650001000000000000000000000000223147317453715634576a737a706e4e633873346a7731345336595461396f4671416b0004687474700100000000000000\"")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    CBitcoinAddress address(params[0].get_str());
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Gcoin address");

    // Color
    const type_Color color = ColorFromValue(params[1]);
    string info;
    CWalletTx wtx;

    EnsureWalletIsUnlocked();

    if (params.size() > 2 && params[2].type() != null_type) {
        info = params[2].get_str();
        CreateLicense(address.Get(), color, info, wtx);
    } else
        SendLicense(address.Get(), color, wtx);

    return wtx.GetHash().GetHex();
}

// Encode the license information(in JSON) into hex string
Value encodelicenseinfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
                _(__func__) + " \"license_info\" ( \"comment\" \"comment-to\" )\n"
                "\nCreate a license info string from json format.\n"
                + HelpRequiringPassphrase() +
                "\nArguments:\n"
                "1. \"license_info\"    (json, required) The license info in json format to be encoded.\n"
                "{\n"
                "  \"version\" : n,             (numeric) The version\n"
                "  \"name\" : xxx,              (string) The name\n"
                "  \"description\" : xxx,       (string) The description\n"
                "  \"issuer\" : xxx,            (string) The issuer name\n"
                "  \"divisibility\" : true,     (bool) The divisibility\n"
                "  \"fee_type\" : n,            (string) The fee type (fixed/by_size/by_amount)\n"
                "  \"fee_rate\" : x.xx,         (double) The fee rate\n"
                "  \"upper_limit\" : n,         (numeric) The upper limit\n"
                "  \"fee_collector\" : xxx,     (string) The fee collector address\n"
                "  \"mint_schedule\" : free,    (string) Mint schedule type (free/once/linear/half_life)\n"
                "  \"member_control\" : false,  (bool) Have member control or not \n"
                "  \"metadata_link\" : xxx,     (string) Hyper link for the metadata \n"
                "  \"metadata_hash\" : xxx,     (string) Hash for the metadata \n"
                "}\n"
                "\nResult:\n"
                "\"licenseinfo\"  (string) The license information.\n"
                "\nExamples:\n"
                + HelpExampleCli("encodelicenseinfo", "{\"version\":1,\"name\":\"alice\",\"description\":\"some one\",\"issuer\":\"issueraddr\",\"divisibility\":true,\"fee_type\":\"fixed\",\"fee_rate\":0.0,\"fee_collector\":\"collectoraddr\",\"upper_limit\":0,\"mint_schedule\":\"free\",\"member_control\":true,\"metadata_link\":\"hyperlink\",\"metadata_hash\":\"hash\"}")
                + HelpExampleRpc("encodelicenseinfo", "{\"version\":1,\"name\":\"alice\",\"description\":\"some one\",\"issuer\":\"issueraddr\",\"divisibility\":true,\"fee_type\":\"fixed\",\"fee_rate\":0.0,\"fee_collector\":\"collectoraddr\",\"upper_limit\":0,\"mint_schedule\":\"free\",\"member_control\":true,\"metadata_link\":\"hyperlink\",\"metadata_hash\":\"hash\"}")
                );

    RPCTypeCheck(params, boost::assign::list_of(obj_type));
    const Object& o = params[0].get_obj();

    CLicenseInfo rawInfo;
    string temp;

    rawInfo.nVersion = find_value(o, "version").get_int();
    rawInfo.name = find_value(o, "name").get_str();
    rawInfo.description = find_value(o, "description").get_str();
    rawInfo.issuer = find_value(o, "issuer").get_str();
    rawInfo.fDivisibility = find_value(o, "divisibility").get_bool();
    temp = find_value(o, "fee_type").get_str();
    if (temp == "fixed")
        rawInfo.feeType = FIXED;
    else if (temp == "by_size")
        rawInfo.feeType = BYSIZE;
    else if (temp == "by_amount")
        rawInfo.feeType = BYAMOUNT;
    else
        throw JSONRPCError(RPC_INVALID_PARAMS, "Invalid fee type. (fixed / by_size / by_amount)");
    rawInfo.nFeeRate = find_value(o, "fee_rate").get_real();
    rawInfo.feeCollectorAddr = find_value(o, "fee_collector").get_str();
    rawInfo.nLimit = find_value(o, "upper_limit").get_int64();
    temp = find_value(o, "mint_schedule").get_str();
    if (temp == "free")
        rawInfo.mintSchedule = FREE;
    else if (temp == "once")
        rawInfo.mintSchedule = ONCE;
    else if (temp == "linear")
        rawInfo.mintSchedule = LINEAR;
    else if (temp == "half_life")
        rawInfo.mintSchedule = HALFLIFE;
    else
        throw JSONRPCError(RPC_INVALID_PARAMS, "Invalid mint schedule. (free / once / linear / half_life)");
    rawInfo.fMemberControl = find_value(o, "member_control").get_bool();
    rawInfo.metadataLink = find_value(o, "metadata_link").get_str();
    rawInfo.metadataHash = uint256S(find_value(o, "metadata_hash").get_str());
    if (!rawInfo.IsValid())
        throw JSONRPCError(RPC_INVALID_PARAMS, "Invalid license information.");
    else
        return rawInfo.EncodeInfo();
}

// Decode the hex string into license information
Value decodelicenseinfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw std::runtime_error(
                _(__func__) + " \"string\"\n"
                "\nReturn a JSON object representing the serialized licenseinfo.\n"

                "\nArguments:\n"
                "1. \"linceseinfo\"      (string, required) The encoded licenseinfo string\n"

                "\nResult:\n"
                "{\n"
                "  \"version\" : n,             (numeric) The version\n"
                "  \"name\" : xxx,              (string) The name\n"
                "  \"description\" : xxx,       (string) The description\n"
                "  \"issuer\" : xxx,            (string) The issuer name\n"
                "  \"divisibility\" : true,     (bool) The divisibility\n"
                "  \"fee_type\" : n,            (int) The fee type\n"
                "  \"fee_rate\" : x.xx,         (double) The fee rate\n"
                "  \"fee_collector\" : xxx,     (string) The fee collector address\n"
                "  \"upper_limit\" : xxx,       (numeric) The upper limit\n"
                "  \"mint_schedule\" : false,   (bool) Have mint schedule or not\n"
                "  \"member_control\" : false,  (bool) Have member control or not \n"
                "  \"metadata_link\" : xxx,     (string) Hyper link for the metadata \n"
                "  \"metadata_hash\" : xxx,     (string) Hash for the metadata \n"
                "}\n"
                "\nExamples:\n"
                + HelpExampleCli(__func__, "\"string\"")
                + HelpExampleRpc(__func__, "\"string\"")
                );

    RPCTypeCheck(params, boost::assign::list_of(str_type));

    CLicenseInfo info;

    if (!info.DecodeInfo(params[0].get_str()))
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "LicenseInfo decode failed");

    Object result;
    LicenseInfoToJSON(info, result);

    return result;
}

Value sendvotetoaddress(const Array& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return Value::null;

    if (fHelp || params.size() < 1 || params.size() > 3)
        throw runtime_error(
            "sendvotetoaddress \"address\" ( \"comment\" \"comment-to\" )\n"
            "\nSend a vote transaction to a given address.\n"
            + HelpRequiringPassphrase() +
            "\nArguments:\n"
            "1. \"address\"     (string, required) The gcoin address to send vote to.\n"
            "2. \"comment\"     (string, optional) A comment used to store what the transaction is for. \n"
            "                             This is not part of the transaction, just kept in your wallet.\n"
            "3. \"comment-to\"  (string, optional) A comment to store the name of the person or organization \n"
            "                             to which you're sending the transaction. This is not part of the \n"
            "                             transaction, just kept in your wallet.\n"
            "\nResult:\n"
            "\"transactionid\"  (string) The transaction id.\n"
            "\nExamples:\n"
            + HelpExampleCli("sendvotetoaddress", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\"")
            + HelpExampleCli("sendvotetoaddress", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" \"donation\" \"seans outpost\"")
            + HelpExampleRpc("sendvotetoaddress", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", \"donation\", \"seans outpost\"")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    CBitcoinAddress address(params[0].get_str());
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Gcoin address");

    // Wallet comments
    CWalletTx wtx;
    if (params.size() > 1 && params[1].type() != null_type && !params[1].get_str().empty())
        wtx.mapValue["comment"] = params[1].get_str();
    if (params.size() > 2 && params[2].type() != null_type && !params[2].get_str().empty())
        wtx.mapValue["to"]      = params[2].get_str();

    EnsureWalletIsUnlocked();

    SendVote(address.Get(), wtx);

    return wtx.GetHash().GetHex();
}

Value addminer(const Array& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return Value::null;

    if (fHelp || params.size() < 1 || params.size() > 3)
        throw runtime_error(
            "addminer \"address\" ( \"comment\" \"comment-to\" )\n"
            "\nAdd the given address as a miner.\n"
            + HelpRequiringPassphrase() +
            "\nArguments:\n"
            "1. \"address\"     (string, required) The gcoin address to be added as a miner.\n"
            "2. \"comment\"     (string, optional) A comment used to store what the transaction is for. \n"
            "                             This is not part of the transaction, just kept in your wallet.\n"
            "3. \"comment-to\"  (string, optional) A comment to store the name of the person or organization \n"
            "                             to which you're sending the transaction. This is not part of the \n"
            "                             transaction, just kept in your wallet.\n"
            "\nResult:\n"
            "\"transactionid\"  (string) The transaction id.\n"
            "\nExamples:\n"
            + HelpExampleCli("addminer", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\"")
            + HelpExampleCli("addminer", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" \"donation\" \"seans outpost\"")
            + HelpExampleRpc("addminer", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", \"donation\", \"seans outpost\"")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    CBitcoinAddress address(params[0].get_str());
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Gcoin address");

    // Wallet comments
    CWalletTx wtx;
    if (params.size() > 1 && params[1].type() != null_type && !params[1].get_str().empty())
        wtx.mapValue["comment"] = params[1].get_str();
    if (params.size() > 2 && params[2].type() != null_type && !params[2].get_str().empty())
        wtx.mapValue["to"]      = params[2].get_str();

    EnsureWalletIsUnlocked();

    AddMiner(address.Get(), wtx);

    return wtx.GetHash().GetHex();
}

Value revokeminer(const Array& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return Value::null;

    if (fHelp || params.size() < 1 || params.size() > 3)
        throw runtime_error(
            "revokeminer \"address\" ( \"comment\" \"comment-to\" )\n"
            "\nSend a transaction to revoke a miner.\n"
            + HelpRequiringPassphrase() +
            "\nArguments:\n"
            "1. \"address\"     (string, required) The gcoin address to be revoked.\n"
            "2. \"comment\"     (string, optional) A comment used to store what the transaction is for. \n"
            "                             This is not part of the transaction, just kept in your wallet.\n"
            "3. \"comment-to\"  (string, optional) A comment to store the name of the person or organization \n"
            "                             to which you're sending the transaction. This is not part of the \n"
            "                             transaction, just kept in your wallet.\n"
            "\nResult:\n"
            "\"transactionid\"  (string) The transaction id.\n"
            "\nExamples:\n"
            + HelpExampleCli("revokeminer", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\"")
            + HelpExampleCli("revokeminer", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" \"donation\" \"seans outpost\"")
            + HelpExampleRpc("revokeminer", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", \"donation\", \"seans outpost\"")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    CBitcoinAddress address(params[0].get_str());
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Gcoin address");

    // Wallet comments
    CWalletTx wtx;
    if (params.size() > 1 && params[1].type() != null_type && !params[1].get_str().empty())
        wtx.mapValue["comment"] = params[1].get_str();
    if (params.size() > 2 && params[2].type() != null_type && !params[2].get_str().empty())
        wtx.mapValue["to"]      = params[2].get_str();

    EnsureWalletIsUnlocked();

    RevokeMiner(address.Get(), wtx);

    return wtx.GetHash().GetHex();
}

Value sendtoaddress(const Array& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return Value::null;

    if (fHelp || params.size() < 3 || params.size() > 6)
        throw runtime_error(
            "sendtoaddress \"address\" amount color ( \"comment\" \"comment-to\" subtractfeefromamount )\n"
            "\nSend an amount to a given address. The amount is a real and is rounded to the nearest 0.00000001\n"
            + HelpRequiringPassphrase() +
            "\nArguments:\n"
            "1. \"address\"             (string, required) The gcoin address to send to.\n"
            "2. \"amount\"              (numeric, required) The amount in gcoin to send. eg 0.1\n"
            "3. \"color\"               (numeric, required) The currency type (color) of the coin.\n"
            "4. \"comment\"             (string, optional) A comment used to store what the transaction is for. \n"
            "                             This is not part of the transaction, just kept in your wallet.\n"
            "5. \"comment-to\"          (string, optional) A comment to store the name of the person or organization \n"
            "                             to which you're sending the transaction. This is not part of the \n"
            "                             transaction, just kept in your wallet.\n"
            "6. subtractfeefromamount   (boolean, optional, default=false) The fee will be deducted from the amount being sent.\n"
            "                             The recipient will receive less gcoins than you enter in the amount field.\n"
            "\nResult:\n"
            "\"transactionid\"          (string) The transaction id.\n"
            "\nExamples:\n"
            + HelpExampleCli("sendtoaddress", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 0.1 1")
            + HelpExampleCli("sendtoaddress", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 0.1 1 \"donation\" \"seans outpost\"")
            + HelpExampleCli("sendtoaddress", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 0.1 1 \"\" \"\" true")
            + HelpExampleRpc("sendtoaddress", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", 0.1, 1, \"donation\", \"seans outpost\"")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    CBitcoinAddress address(params[0].get_str());
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Gcoin address");

    // Amount
    CAmount nAmount = AmountFromValue(params[1]);

    // Color
    const type_Color color = ColorFromValue(params[2]);

    // Wallet comments
    CWalletTx wtx;
    if (params.size() > 3 && params[3].type() != null_type && !params[3].get_str().empty())
        wtx.mapValue["comment"] = params[3].get_str();
    if (params.size() > 4 && params[4].type() != null_type && !params[4].get_str().empty())
        wtx.mapValue["to"]      = params[4].get_str();

    bool fSubtractFeeFromAmount = false;
    if (params.size() > 5)
        fSubtractFeeFromAmount = params[5].get_bool();

    EnsureWalletIsUnlocked();

    SendMoney(address.Get(), nAmount, color, fSubtractFeeFromAmount, wtx);

    return wtx.GetHash().GetHex();
}

Value listaddressgroupings(const Array& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return Value::null;

    if (fHelp)
        throw runtime_error(
            _(__func__) + "\n"
            "\nLists groups of addresses which have had their common ownership\n"
            "made public by common use as inputs or as the resulting change\n"
            "in past transactions\n"
            "\nResult:\n"
            "[\n"
            "  [\n"
            "    [\n"
            "      \"address\",            (string) The gcoin address\n"
            "      \"color\" : amount,     (string : numeric) The amount in btc corresponding to color\n"
            "      \"account\"             (string, optional) The account (DEPRECATED)\n"
            "    ]\n"
            "    ,...\n"
            "  ]\n"
            "  ,...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("listaddressgroupings", "")
            + HelpExampleRpc("listaddressgroupings", "")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    Array jsonGroupings;
    map<CTxDestination, colorAmount_t > balances = pwalletMain->GetAddressBalances();
    BOOST_FOREACH(set<CTxDestination> grouping, pwalletMain->GetAddressGroupings()) {
        Array jsonGrouping;
        BOOST_FOREACH(CTxDestination address, grouping) {
            Array addressInfo;
            addressInfo.push_back(CBitcoinAddress(address).ToString());
            addressInfo.push_back(ValueFromAmount(balances[address]));
            {
                LOCK(pwalletMain->cs_wallet);
                if (pwalletMain->mapAddressBook.find(CBitcoinAddress(address).Get()) != pwalletMain->mapAddressBook.end())
                    addressInfo.push_back(pwalletMain->mapAddressBook.find(CBitcoinAddress(address).Get())->second.name);
            }
            jsonGrouping.push_back(addressInfo);
        }
        jsonGroupings.push_back(jsonGrouping);
    }
    return jsonGroupings;
}

Value signmessage(const Array& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return Value::null;

    if (fHelp || params.size() != 2)
        throw runtime_error(
            _(__func__) + " \"address\" \"message\"\n"
            "\nSign a message with the private key of an address"
            + HelpRequiringPassphrase() + "\n"
            "\nArguments:\n"
            "1. \"address\"     (string, required) The gcoin address to use for the private key.\n"
            "2. \"message\"     (string, required) The message to create a signature of.\n"
            "\nResult:\n"
            "\"signature\"      (string) The signature of the message encoded in base 64\n"
            "\nExamples:\n"
            "\nUnlock the wallet for 30 seconds\n"
            + HelpExampleCli("walletpassphrase", "\"mypassphrase\" 30") +
            "\nCreate the signature\n"
            + HelpExampleCli("signmessage", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\" \"my message\"") +
            "\nVerify the signature\n"
            + HelpExampleCli("verifymessage", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\" \"signature\" \"my message\"") +
            "\nAs json rpc\n"
            + HelpExampleRpc("signmessage", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\", \"my message\"")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    EnsureWalletIsUnlocked();

    string strAddress = params[0].get_str();
    string strMessage = params[1].get_str();

    CBitcoinAddress addr(strAddress);
    if (!addr.IsValid())
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid address");

    CKeyID keyID;
    if (!addr.GetKeyID(keyID))
        throw JSONRPCError(RPC_TYPE_ERROR, "Address does not refer to key");

    CKey key;
    if (!pwalletMain->GetKey(keyID, key))
        throw JSONRPCError(RPC_WALLET_ERROR, "Private key not available");

    CHashWriter ss(SER_GETHASH, 0);
    ss << strMessageMagic;
    ss << strMessage;

    vector<unsigned char> vchSig;
    if (!key.SignCompact(ss.GetHash(), vchSig))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Sign failed");

    return EncodeBase64(&vchSig[0], vchSig.size());
}

Value getreceivedbyaddress(const Array& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return Value::null;

    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            _(__func__) + " \"address\" ( minconf )\n"
            "\nReturns the total amount received by the given address in transactions with at least minconf confirmations.\n"
            "\nArguments:\n"
            "1. \"address\"     (string, required) The gcoin address for transactions.\n"
            "2. minconf         (numeric, optional, default=1) Only include transactions confirmed at least this many times.\n"
            "\nResult:\n"
            "[                     (json array of string : numeric)\n"
            "  \"color\" : amount  (string : numeric) The total amount in gcoin corresponding to color received at this address\n"
            "  ,...\n"
            "]\n"
            "\nExamples:\n"
            "\nThe amount from transactions with at least 1 confirmation\n"
            + HelpExampleCli("getreceivedbyaddress", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\"") +
            "\nThe amount including unconfirmed transactions, zero confirmations\n"
            + HelpExampleCli("getreceivedbyaddress", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\" 0") +
            "\nThe amount with at least 6 confirmation, very safe\n"
            + HelpExampleCli("getreceivedbyaddress", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\" 6") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("getreceivedbyaddress", "\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\", 6")
       );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    // Gcoin address
    CBitcoinAddress address = CBitcoinAddress(params[0].get_str());
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Gcoin address");
    CScript scriptPubKey = GetScriptForDestination(address.Get());
    if (!IsMine(*pwalletMain,scriptPubKey))
        return (double)0.0;

    // Minimum confirmations
    int nMinDepth = 1;
    if (params.size() > 1)
        nMinDepth = params[1].get_int();

    // Tally
    colorAmount_t colorAmount;
    for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); ++it) {
        const CWalletTx& wtx = (*it).second;
        if (wtx.IsCoinBase() || !CheckFinalTx(wtx))
            continue;

        BOOST_FOREACH(const CTxOut& txout, wtx.vout)
            if (txout.scriptPubKey == scriptPubKey)
                if (wtx.GetDepthInMainChain() >= nMinDepth) {
                    if (!colorAmount.count(txout.color))
                        colorAmount[txout.color] = 0;
                    colorAmount[txout.color] += txout.nValue;
                }
    }

    return ValueFromAmount(colorAmount);
}


Value getreceivedbyaccount(const Array& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return Value::null;

    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "getreceivedbyaccount \"account\" ( minconf )\n"
            "\nDEPRECATED. Returns the total amount received by addresses with <account> in transactions with at least [minconf] confirmations.\n"
            "\nArguments:\n"
            "1. \"account\"      (string, required) The selected account, may be the default account using \"\".\n"
            "2. minconf          (numeric, optional, default=1) Only include transactions confirmed at least this many times.\n"
            "\nResult:\n"
            "[                     (json array of string : numeric)\n"
            "  \"color\" : amount  (string : numeric) The total amount in gcoin corresponding to color received at this account\n"
            "  ,...\n"
            "]\n"
            "\nExamples:\n"
            "\nAmount received by the default account with at least 1 confirmation\n"
            + HelpExampleCli("getreceivedbyaccount", "\"\"") +
            "\nAmount received at the tabby account including unconfirmed amounts with zero confirmations\n"
            + HelpExampleCli("getreceivedbyaccount", "\"tabby\" 0") +
            "\nThe amount with at least 6 confirmation, very safe\n"
            + HelpExampleCli("getreceivedbyaccount", "\"tabby\" 6") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("getreceivedbyaccount", "\"tabby\", 6")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    // Minimum confirmations
    int nMinDepth = 1;
    if (params.size() > 1)
        nMinDepth = params[1].get_int();

    // Get the set of pub keys assigned to account
    string strAccount = AccountFromValue(params[0]);
    set<CTxDestination> setAddress = pwalletMain->GetAccountAddresses(strAccount);

    // Tally
    colorAmount_t colorAmount;
    for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); ++it) {
        const CWalletTx& wtx = (*it).second;
        if (wtx.IsCoinBase() || !CheckFinalTx(wtx))
            continue;

        BOOST_FOREACH(const CTxOut& txout, wtx.vout) {
            CTxDestination address;
            if (ExtractDestination(txout.scriptPubKey, address) && IsMine(*pwalletMain, address) && setAddress.count(address))
                if (wtx.GetDepthInMainChain() >= nMinDepth) {
                    if (!colorAmount.count(txout.color))
                        colorAmount[txout.color] = 0;
                    colorAmount[txout.color] += txout.nValue;
                }
        }
    }

    return ValueFromAmount(colorAmount);
}
colorAmount_t GetAccountBalance(CWalletDB& walletdb, const string& strAccount, int nMinDepth, const isminefilter& filter, colorAmount_t& color_amount)
{
    // Tally wallet transactions
    for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); ++it) {
        const CWalletTx& wtx = (*it).second;
        if (!CheckFinalTx(wtx) || wtx.GetBlocksToMaturity() > 0 || wtx.GetDepthInMainChain() < 0)
            continue;

        colorAmount_t nReceived, nSent;
        wtx.GetAccountAmounts(strAccount, nReceived, nSent, filter);

        if (nReceived.size() != 0 && wtx.GetDepthInMainChain() >= nMinDepth) {
            for (colorAmount_t::const_iterator it = nReceived.begin(); it != nReceived.end(); it++) {
                if (color_amount.count(it->first) == 0)
                    color_amount[it->first] = 0;
                color_amount[it->first] += it->second;
            }
        }
        for (colorAmount_t::const_iterator it = nSent.begin(); it != nSent.end(); it++) {
            if (color_amount.count(it->first) != 0)
                color_amount[it->first] -= it->second;
        }
    }

    // Tally internal accounting entries
    return walletdb.GetAccountCreditDebit(strAccount);
}

colorAmount_t GetAccountBalance(const string& strAccount, int nMinDepth, const isminefilter& filter, colorAmount_t& color_amount)
{
    CWalletDB walletdb(pwalletMain->strWalletFile);
    return GetAccountBalance(walletdb, strAccount, nMinDepth, filter, color_amount);
}

CAmount GetAccountColorBalance(CWalletDB& walletdb, const string& strAccount, const type_Color& color, int nMinDepth, const isminefilter& filter)
{
    CAmount nBalance = 0;

    // Tally wallet transactions
    for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); ++it) {
        const CWalletTx& wtx = (*it).second;
        if (!IsFinalTx(wtx, chainActive.Height(), GetAdjustedTime()) || wtx.GetBlocksToMaturity() > 0 || wtx.GetDepthInMainChain() < 0 )
            continue;

        colorAmount_t nReceived, nSent;
        wtx.GetAccountAmounts(strAccount, nReceived, nSent, filter);

        if (wtx.GetDepthInMainChain() >= nMinDepth && nReceived.count(color) != 0)
            nBalance += nReceived[color];
        if (nSent.count(color) != 0)
            nBalance -= nSent[color];
    }

    // Tally internal accounting entries
    if (walletdb.GetAccountCreditDebit(strAccount).count(color))
        nBalance += walletdb.GetAccountCreditDebit(strAccount)[color];

    return nBalance;
}

CAmount GetAccountColorBalance(const string& strAccount, const type_Color& color, int nMinDepth, const isminefilter& filter)
{
    CWalletDB walletdb(pwalletMain->strWalletFile);
    return GetAccountColorBalance(walletdb, strAccount, color, nMinDepth, filter);
}

Value getbalance(const Array& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return Value::null;

    if (fHelp || params.size() > 3)
        throw runtime_error(
            "getbalance ( \"account\" minconf includeWatchonly )\n"
            "\nIf account is not specified, returns the server's total available balance.\n"
            "If account is specified (DEPRECATED), returns the balance in the account.\n"
            "Note that the account \"\" is not the same as leaving the parameter out.\n"
            "The server total may be different to the balance in the default \"\" account.\n"
            "\nArguments:\n"
            "1. \"account\"      (string, optional) DEPRECATED. The selected account, or \"*\" for entire wallet. It may be the default account using \"\".\n"
            "2. minconf          (numeric, optional, default=1) Only include transactions confirmed at least this many times.\n"
            "3. includeWatchonly (bool, optional, default=false) Also include balance in watchonly addresses (see 'importaddress')\n"
            "\nResult:\n"
            "[                     (json array of string : numeric)\n"
            "  \"color\" : amount  (string : numeric) The total amount in gcoin corresponding to color received at this account\n"
            "  ,...\n"
            "]\n"
            "\nExamples:\n"
            "\nThe total amount in the wallet\n"
            + HelpExampleCli("getbalance", "") +
            "\nThe total amount in the wallet at least 5 blocks confirmed\n"
            + HelpExampleCli("getbalance", "\"*\" 6") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("getbalance", "\"*\", 6")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    colorAmount_t color_amount;

    if (params.size() == 0) {
        pwalletMain->GetBalance(color_amount);
        return ValueFromAmount(color_amount);
    }

    int nMinDepth = 1;
    if (params.size() > 1)
        nMinDepth = params[1].get_int();
    isminefilter filter = ISMINE_SPENDABLE;
    if(params.size() > 2)
        if(params[2].get_bool())
            filter = filter | ISMINE_WATCH_ONLY;

    if (params[0].get_str() == "*") {
        // Calculate total balance a different way from GetBalance()
        // (GetBalance() sums up all unspent TxOuts)
        // getbalance and "getbalance * 1 true" should return the same number
        for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); ++it) {
            const CWalletTx& wtx = (*it).second;
            if (!CheckFinalTx(wtx) || wtx.GetBlocksToMaturity() > 0 || wtx.GetDepthInMainChain() < 0)
                continue;

            CAmount allFee;
            string strSentAccount;
            list<COutputEntry> listReceived;
            list<COutputEntry> listSent;
            wtx.GetAmounts(listReceived, listSent, allFee, strSentAccount, filter);
            if (wtx.GetDepthInMainChain() >= nMinDepth) {
                BOOST_FOREACH(const COutputEntry& r, listReceived) {
                    if (color_amount.count(r.color) == 0)
                        color_amount[r.color] = 0;

                    color_amount[r.color] += r.amount;
                }
            }
            BOOST_FOREACH(const COutputEntry& s, listSent) {
                if (color_amount.count(s.color) == 0) // This should not happen.
                    color_amount[s.color] = 0;

                color_amount[s.color] -= s.amount;
            }
            /*
            if (color_amount.count(wtx.color) == 0) // This should not happen.
                color_amount[wtx.color] = 0;
            */

            //color_amount[wtx.color] -= allFee;
        }

        return ValueFromAmount(color_amount);
    }

    string strAccount = AccountFromValue(params[0]);

    GetAccountBalance(strAccount, nMinDepth, filter, color_amount);

    return ValueFromAmount(color_amount);
}

Value getcolorbalance(const Array& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return Value::null;

    if (fHelp || params.size() < 1 || params.size() > 4)
        throw runtime_error(
            "getcolorbalance color ( \"account\" minconf includeWatchonly )\n"
            "\nIf account is not specified, returns the server's total available color balance.\n"
            "If account is specified (DEPRECATED), returns the color balance in the account.\n"
            "Note that the account \"\" is not the same as leaving the parameter out.\n"
            "The server total may be different to the color balance in the default \"\" account.\n"
            "\nArguments:\n"
            "1. color            (numeric, required) The currency type (color) of the coin.\n"
            "2. \"account\"      (string, optional) The selected account, or \"*\" for entire wallet. It may be the default account using \"\".\n"
            "3. minconf          (numeric, optional, default=1) Only include transactions confirmed at least this many times.\n"
            "4. includeWatchonly (bool, optional, default=false) Also include balance in watchonly addresses (see 'importaddress')\n"
            "\nResult:\n"
            "amount              (numeric) The total amount in specified color gcoin received for this account.\n"
            "\nExamples:\n"
            "\nThe total amount in the wallet\n"
            + HelpExampleCli("getcolorbalance", "1") +
            "\nThe total amount in the wallet at least 5 blocks confirmed\n"
            + HelpExampleCli("getcolorbalance", "1 \"*\" 6") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("getcolorbalance", "1, \"*\", 6")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    const type_Color color = ColorFromValue(params[0]);

    if (params.size() == 1)
        return ValueFromAmount(pwalletMain->GetColorBalance(color));

    int nMinDepth = 1;
    if (params.size() > 2)
        nMinDepth = params[2].get_int();
    isminefilter filter = ISMINE_SPENDABLE;
    if (params.size() > 3)
        if (params[3].get_bool())
            filter = filter | ISMINE_WATCH_ONLY;

    if (params[1].get_str() == "*") {
        // Calculate total balance a different way from GetColorBalance()
        // (GetColorBalance() sums up all unspent TxOuts)
        // getcolorbalance and "getcolorbalance * 1 true" should return the same number
        // getcolorbalance and getcolorbalance '*' 0 should return the same number
        CAmount nBalance = 0;
        for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); ++it) {
            const CWalletTx& wtx = (*it).second;
            if (!CheckFinalTx(wtx) || wtx.GetBlocksToMaturity() > 0 || wtx.GetDepthInMainChain() < 0)
                continue;

            CAmount allFee;
            string strSentAccount;
            list<COutputEntry> listReceived;
            list<COutputEntry> listSent;
            wtx.GetAmounts(listReceived, listSent, allFee, strSentAccount, filter);
            if (wtx.GetDepthInMainChain() >= nMinDepth) {
                BOOST_FOREACH(const COutputEntry& r, listReceived)
                    if (r.color == color)
                        nBalance += r.amount;
            }
            BOOST_FOREACH(const COutputEntry& s, listSent)
                if (s.color == color)
                    nBalance -= s.amount;
            nBalance -= allFee;
        }
        return ValueFromAmount(nBalance);
    }

    string strAccount = AccountFromValue(params[1]);

    CAmount nBalance = GetAccountColorBalance(strAccount, color, nMinDepth, filter);

    return ValueFromAmount(nBalance);
}


Value getaddressbalance(const Array& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return Value::null;

    if (fHelp || params.size() > 2 || params.size() < 1)
        throw runtime_error(
            "getaddressbalance \"gcoin-address\" ( minconf )\n"
            "\nArguments:\n"
            "1. \"address\"     (string, required) Gcoin address.\n"
            "2. minconf         (numeric, optional, default=1) Only include transactions confirmed at least this many times.\n"
            "\nResult:\n"
            "[                     (json array of string : numeric)\n"
            "  \"color\" : amount  (string : numeric) The total amount in gcoin corresponding to color received at this address\n"
            "  ,...\n"
            "]\n"
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    colorAmount_t color_amount;

    string strAddress = params[0].get_str();
    CBitcoinAddress address(strAddress);

    int nMinDepth = 1;
    if (params.size() > 1)
        nMinDepth = params[1].get_int();

    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Gcoin address");

    pwalletMain->GetAddressBalance(strAddress, color_amount, nMinDepth);

    return ValueFromAmount(color_amount);
}

Value getunconfirmedbalance(const Array &params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return Value::null;

    if (fHelp || params.size() != 0)
        throw runtime_error(
                "getunconfirmedbalance\n"
                "Returns the server's total unconfirmed balance\n"
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    colorAmount_t color_amount;
    pwalletMain->GetUnconfirmedBalance(color_amount);
    return ValueFromAmount(color_amount);
}

Value getunconfirmedcolorbalance(const Array &params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return Value::null;

    if (fHelp || params.size() != 1)
        throw runtime_error(
                "getunconfirmedcolorbalance\n"
                "Returns the server's total unconfirmed color balance\n"
                "\nArguments:\n"
                "1. \"color\"       (numeric, required) The currency type (color) of the coin.\n"
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    const type_Color color = ColorFromValue(params[0]);

    return ValueFromAmount(pwalletMain->GetUnconfirmedColorBalance(color));
}

Value getlicenselist(const Array& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return Value::null;

    if (fHelp || params.size() > 1)
        throw runtime_error(
            "getlicenselist\n"
            "\nList licenses.\n"
            "\nIf verbose=0, returns the license in the wallet\n"
            "If verbose is non-zero, returns the entire license list\n"

            "\nArguments:\n"
            "1. verbose       (numeric, optional, default=0) If 0, return license in wallet, others return entire license list\n"
            "\nResult:\n"
            "{\n"
            "   \"color\": {\n"
            "           \"address\" :   (str)   Address possessing the color license. \n"
            "           \"amount\"  :   (float) Amount of the license.\n"
            "   }\n"
            "   ...\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getlicenselist", "")
            + HelpExampleRpc("getlicenselist", "")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    bool fVerbose = false;
    if (params.size() > 0)
        fVerbose = (params[0].get_int() != 0);

    map<type_Color, pair<string, CAmount> > color_amount = plicense->ListLicense();

    Object ret;
    char r1[21];
    for (map<type_Color, pair<string, CAmount> >::iterator it = color_amount.begin(); it != color_amount.end(); it++) {
        CBitcoinAddress address(it->second.first);
        isminefilter filter = ISMINE_SPENDABLE;
        isminefilter mine = IsMine(*pwalletMain, address.Get());
        if (!fVerbose && !(mine & filter))
            continue;
        Object obj;
        obj.push_back(Pair("address", (*it).second.first));
        obj.push_back(Pair("Total Amount", ValueFromAmount((*it).second.second)));
        snprintf(r1, 20, "%" PRIu32, (*it).first);
        ret.push_back(Pair(r1, obj));
    }

    return ret;
}

Value getlicenseinfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            _(__func__) + "\n"
            "Return a JSON object of licenseinfo of color.\n"

            "\nArguments:\n"
            "1. \"color\"      (numeric, required) The color\n"

            "\nResult:\n"
            "{\n"
                 "  \"Owner\" : xxx,            (string) Address of the color owner \n"
                 "  \"Total amount\" : xxx      (numeric) The total amount of the color exist on the blockchain \n"
                 "  \"version\" : n,            (numeric) The version\n"
                 "  \"name\" : xxx,             (string) The name\n"
                 "  \"description\" : xxx,      (string) The description\n"
                 "  \"issuer\" : xxx,           (string) The issuer name\n"
                 "  \"divisibility\" : true,    (bool) The divisibility\n"
                 "  \"fee_type\" : n,           (int) The fee type\n"
                 "  \"fee_rate\" : x.xx,        (double) The fee rate\n"
                 "  \"fee_collector\" : xxx,    (string) The fee collector address\n"
                 "  \"upper_limit\" : xxx,      (numeric) The upper limit\n"
                 "  \"mint_schedule\" : false,  (bool) Have mint schedule or not\n"
                 "  \"member_control\" : false, (bool) Have member control or not \n"
                 "  \"metadata_link\" : xxx,    (string) Hyper link for the metadata \n"
                 "  \"metadata_hash\" : xxx,    (string) Hash for the metadata \n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getlicenseinfo", "1")
            + HelpExampleRpc("getlicenseinfo", "1")
        );

    const type_Color color = ColorFromValue(params[0]);

    CLicenseInfo info;
    if (!plicense->GetLicenseInfo(color, info))
        throw JSONRPCError(RPC_INVALID_PARAMETER, "License color not exist.");

    Object result;
    result.push_back(Pair("Owner", plicense->GetOwner(color)));
    result.push_back(Pair("Total amount", plicense->NumOfCoins(color)/COIN));
    LicenseInfoToJSON(info, result);

    return result;
}

Value movecmd(const Array& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return Value::null;

    if (fHelp || params.size() < 3 || params.size() > 5)
        throw runtime_error(
            "move \"fromaccount\" \"toaccount\" amount color ( minconf \"comment\" )\n"
            "\nDEPRECATED. Move a specified amount from one account in your wallet to another.\n"
            "\nArguments:\n"
            "1. \"fromaccount\"   (string, required) The name of the account to move funds from. May be the default account using \"\".\n"
            "2. \"toaccount\"     (string, required) The name of the account to move funds to. May be the default account using \"\".\n"
            "3. amount            (numeric, required) The amount of the funds to be moved.\n"
            "4. color             (numeric, required) The color of the funds to be moved.\n"
            "5. minconf           (numeric, optional, default=1) Only use funds with at least this many confirmations.\n"
            "6. \"comment\"       (string, optional) An optional comment, stored in the wallet only.\n"
            "\nResult:\n"
            "true|false           (boolean) true if successfull.\n"
            "\nExamples:\n"
            "\nMove 0.01 gcoin from the default account to the account named tabby\n"
            + HelpExampleCli("move", "\"\" \"tabby\" 0.01") +
            "\nMove 0.01 gcoin timotei to akiko with a comment and funds have 6 confirmations\n"
            + HelpExampleCli("move", "\"timotei\" \"akiko\" 0.01 6 \"happy birthday!\"") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("move", "\"timotei\", \"akiko\", 0.01, 6, \"happy birthday!\"")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    string strFrom = AccountFromValue(params[0]);
    string strTo = AccountFromValue(params[1]);
    CAmount nAmount = AmountFromValue(params[2]);
    type_Color color = ColorFromValue(params[3]);
    if (params.size() > 4)
        // unused parameter, used to be nMinDepth, keep type-checking it though
        (void)params[4].get_int();
    string strComment;
    if (params.size() > 5)
        strComment = params[5].get_str();

    CWalletDB walletdb(pwalletMain->strWalletFile);
    if (!walletdb.TxnBegin())
        throw JSONRPCError(RPC_DATABASE_ERROR, "database error");

    int64_t nNow = GetAdjustedTime();

    // Debit
    CAccountingEntry debit;
    debit.nOrderPos = pwalletMain->IncOrderPosNext(&walletdb);
    debit.strAccount = strFrom;
    debit.nCreditDebit.insert(make_pair(color, -nAmount));
    debit.nTime = nNow;
    debit.strOtherAccount = strTo;
    debit.strComment = strComment;
    walletdb.WriteAccountingEntry(debit);

    // Credit
    CAccountingEntry credit;
    credit.nOrderPos = pwalletMain->IncOrderPosNext(&walletdb);
    credit.strAccount = strTo;
    credit.nCreditDebit.insert(make_pair(color, nAmount));
    credit.nTime = nNow;
    credit.strOtherAccount = strFrom;
    credit.strComment = strComment;
    walletdb.WriteAccountingEntry(credit);

    if (!walletdb.TxnCommit())
        throw JSONRPCError(RPC_DATABASE_ERROR, "database error");

    return true;
}


Value sendfrom(const Array& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return Value::null;

    if (fHelp || params.size() < 4 || params.size() > 6)
        throw runtime_error(
            "sendfrom \"fromaddress\" \"toaddress\" amount color ( \"comment\" \"comment-to\" )\n"
            "\nSent an amount from a fixed address to a gcoin address.\n"
            "The amount is a real and is rounded to the nearest 0.00000001."
            + HelpRequiringPassphrase() + "\n"
            "\nArguments:\n"
            "1. \"fromaddress\"     (string, required) The gcoin address to send funds from.\n"
            "2. \"toaddress\"       (string, required) The gcoin address to send funds to.\n"
            "3. amount              (numeric, required) The amount in gcoin. (transaction fee is added on top).\n"
            "4. color               (numeric, required) The currency type (color) of the coin.\n"
            "5. \"comment\"         (string, optional) A comment used to store what the transaction is for. \n"
            "                                     This is not part of the transaction, just kept in your wallet.\n"
            "6. \"comment-to\"      (string, optional) An optional comment to store the name of the person or organization \n"
            "                                     to which you're sending the transaction. This is not part of the transaction, \n"
            "                                     it is just kept in your wallet.\n"
            "\nResult:\n"
            "\"transactionid\"        (string) The transaction id.\n"
            "\nExamples:\n"
            "\nSend 2 gcoin color 1 from the address to the address, must have at least 1 confirmation\n"
            + HelpExampleCli("sendfrom", "\"3O89Awopq5POaUAXq2q1IjiASC71Zzzzsa\" \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 2 1") +
            "\nSend 2 gcoin color 1 from the address to the given address\n"
            + HelpExampleCli("sendfrom", "\"3O89Awopq5POaUAXq2q1IjiASC71Zzzzsa\" \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 2 1\"donation\" \"seans outpost\"") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("sendfrom", "\"3O89Awopq5POaUAXq2q1IjiASC71Zzzzsa\", \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", 2, 1, \"donation\", \"seans outpost\"")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    string strFromAddress = params[0].get_str();
    CBitcoinAddress fromaddress(strFromAddress);

    CBitcoinAddress address(params[1].get_str());

    if (!fromaddress.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid from address");
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid to address");

    CAmount nAmount = AmountFromValue(params[2]);
    const type_Color color = ColorFromValue(params[3]);

    CWalletTx wtx;
    if (params.size() > 4 && params[4].type() != null_type && !params[4].get_str().empty())
        wtx.mapValue["comment"] = params[4].get_str();
    if (params.size() > 5 && params[5].type() != null_type && !params[5].get_str().empty())
        wtx.mapValue["to"]      = params[5].get_str();

    EnsureWalletIsUnlocked();

    SendMoneyFromFixedAddress(strFromAddress, address.Get(), nAmount, color, false, wtx);

    return wtx.GetHash().GetHex();
}

Value sendfromfeeaddress(const Array& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return Value::null;

    if (fHelp || params.size() < 5 || params.size() > 7)
        throw runtime_error(
            "sendfromfeeaddress \"fromaddress\" \"toaddress\" amount color ( \"comment\" \"comment-to\" )\n"
            "\nSent an amount from a fixed address to a gcoin address.\n"
            "The amount is a real and is rounded to the nearest 0.00000001."
            + HelpRequiringPassphrase() + "\n"
            "\nArguments:\n"
            "1. \"fromaddress\"     (string, required) The gcoin address to send funds from.\n"
            "2. \"toaddress\"       (string, required) The gcoin address to send funds to.\n"
            "3. \"feeaddress\"      (string, required) The gcoin address to send fees from.\n"
            "4. amount              (numeric, required) The amount in gcoin. (transaction fee is added on top).\n"
            "5. color               (numeric, required) The currency type (color) of the coin.\n"
            "6. \"comment\"         (string, optional) A comment used to store what the transaction is for. \n"
            "                                     This is not part of the transaction, just kept in your wallet.\n"
            "7. \"comment-to\"      (string, optional) An optional comment to store the name of the person or organization \n"
            "                                     to which you're sending the transaction. This is not part of the transaction, \n"
            "                                     it is just kept in your wallet.\n"
            "\nResult:\n"
            "\"transactionid\"        (string) The transaction id.\n"
            "\nExamples:\n"
            "\nSend 2 gcoin color 1 from the address to the address, must have at least 1 confirmation\n"
            + HelpExampleCli("sendfromfeeaddress", "\"3O89Awopq5POaUAXq2q1IjiASC71Zzzzsa\" \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" \"16LioCHQ5zXxSK3bkZSNzMFAWHixay2KQ5\" 2 1") +
            "\nSend 2 gcoin color 1 from the address to the given address\n"
            + HelpExampleCli("sendfromfeeaddress", "\"3O89Awopq5POaUAXq2q1IjiASC71Zzzzsa\" \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" \"16LioCHQ5zXxSK3bkZSNzMFAWHixay2KQ5\" 2 1\"donation\" \"seans outpost\"") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("sendfromfeeaddress", "\"3O89Awopq5POaUAXq2q1IjiASC71Zzzzsa\", \"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\", \"16LioCHQ5zXxSK3bkZSNzMFAWHixay2KQ5\", 2, 1, \"donation\", \"seans outpost\"")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    string strFromAddress = params[0].get_str();
    CBitcoinAddress fromaddress(strFromAddress);

    CBitcoinAddress address(params[1].get_str());

    string feeFromAddress = params[2].get_str();
    CBitcoinAddress feeaddress(feeFromAddress);

    if (!fromaddress.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid From-Bitcoin address");
    if (!address.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid To-Bitcoin address");
    if (!feeaddress.IsValid())
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid Fee-Bitcoin address");

    CAmount nAmount = AmountFromValue(params[3]);
    const type_Color color = ColorFromValue(params[4]);

    CWalletTx wtx;
    if (params.size() > 5 && params[5].type() != null_type && !params[5].get_str().empty())
        wtx.mapValue["comment"] = params[5].get_str();
    if (params.size() > 6 && params[6].type() != null_type && !params[6].get_str().empty())
        wtx.mapValue["to"]      = params[6].get_str();

    EnsureWalletIsUnlocked();

    SendMoneyFromFixedAddress(strFromAddress, address.Get(), nAmount, color, false, wtx, feeFromAddress);

    return wtx.GetHash().GetHex();
}

Value sendmany(const Array& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return Value::null;

    if (fHelp || params.size() < 3 || params.size() > 6)
        throw runtime_error(
            "sendmany \"fromaccount\" {\"address\":amount,...} color ( minconf \"comment\" [\"address\",...] )\n"
            "\nSend multiple times. Amounts are double-precision floating point numbers."
            + HelpRequiringPassphrase() + "\n"
            "\nArguments:\n"
            "1. \"fromaccount\"         (string, required) DEPRECATED. The account to send the funds from. Should be \"\" for the default account\n"
            "2. \"amounts\"             (string, required) A json object with addresses and amounts\n"
            "    {\n"
            "      \"address\":amount   (numeric) The gcoin address is the key, the numeric amount in gcoin is the value\n"
            "      ,...\n"
            "    }\n"
            "3. color                   (numeric, required) The currency type (color) of the coin.\n"
            "4. minconf                 (numeric, optional, default=1) Only use the balance confirmed at least this many times.\n"
            "5. \"comment\"             (string, optional) A comment\n"
            "6. subtractfeefromamount   (string, optional) A json array with addresses.\n"
            "                           The fee will be equally deducted from the amount of each selected address.\n"
            "                           Those recipients will receive less gcoins than you enter in their corresponding amount field.\n"
            "                           If no addresses are specified here, the sender pays the fee.\n"
            "    [\n"
            "      \"address\"            (string) Subtract fee from this address\n"
            "      ,...\n"
            "    ]\n"
            "\nResult:\n"
            "\"transactionid\"          (string) The transaction id for the send. Only 1 transaction is created regardless of \n"
            "                                    the number of addresses.\n"
            "\nExamples:\n"
            "\nSend two amounts to two different addresses:\n"
            + HelpExampleCli("sendmany", "\"\" \"{\\\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\\\":0.01,\\\"1353tsE8YMTA4EuV7dgUXGjNFf9KpVvKHz\\\":0.02}\" 1") +
            "\nSend two amounts to two different addresses setting the confirmation and comment:\n"
            + HelpExampleCli("sendmany", "\"\" \"{\\\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\\\":0.01,\\\"1353tsE8YMTA4EuV7dgUXGjNFf9KpVvKHz\\\":0.02}\" 1 6 \"testing\"") +
            "\nSend two amounts to two different addresses, subtract fee from amount:\n"
            + HelpExampleCli("sendmany", "\"\" \"{\\\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\\\":0.01,\\\"1353tsE8YMTA4EuV7dgUXGjNFf9KpVvKHz\\\":0.02}\" 1 1 \"\" \"[\\\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\\\",\\\"1353tsE8YMTA4EuV7dgUXGjNFf9KpVvKHz\\\"]\"") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("sendmany", "\"\", \"{\\\"1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XZ\\\":0.01,\\\"1353tsE8YMTA4EuV7dgUXGjNFf9KpVvKHz\\\":0.02}\", 1, 6, \"testing\"")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    string strAccount = AccountFromValue(params[0]);
    Object sendTo = params[1].get_obj();

    const type_Color color = ColorFromValue(params[2]);

    int nMinDepth = 1;
    if (params.size() > 3)
        nMinDepth = params[3].get_int();

    CWalletTx wtx;
    wtx.strFromAccount = strAccount;
    if (params.size() > 4 && params[4].type() != null_type && !params[4].get_str().empty())
        wtx.mapValue["comment"] = params[4].get_str();

    Array subtractFeeFromAmount;
    if (params.size() > 5)
        subtractFeeFromAmount = params[5].get_array();

    set<CBitcoinAddress> setAddress;
    vector<CRecipient> vecSend;

    CAmount totalAmount = 0;
    BOOST_FOREACH(const Pair& s, sendTo) {
        CBitcoinAddress address(s.name_);
        if (!address.IsValid())
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, string("Invalid Bitcoin address: ")+s.name_);

        if (setAddress.count(address))
            throw JSONRPCError(RPC_INVALID_PARAMETER, string("Invalid parameter, duplicated address: ")+s.name_);
        setAddress.insert(address);

        CScript scriptPubKey = GetScriptForDestination(address.Get());
        CAmount nAmount = AmountFromValue(s.value_);
        totalAmount += nAmount;

        bool fSubtractFeeFromAmount = false;
        BOOST_FOREACH(const Value& addr, subtractFeeFromAmount)
            if (addr.get_str() == s.name_)
                fSubtractFeeFromAmount = true;

        CRecipient recipient = {scriptPubKey, nAmount, fSubtractFeeFromAmount};
        vecSend.push_back(recipient);
    }

    EnsureWalletIsUnlocked();

    // Check funds
    CAmount nBalance = GetAccountColorBalance(strAccount, color, nMinDepth, ISMINE_SPENDABLE);
    if (totalAmount > nBalance)
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, "Account has insufficient funds");

    // Send
    CReserveKey keyChange(pwalletMain);
    CAmount nFeeRequired = 0;
    int nChangePosRet = -1;
    string strFailReason;
    bool fCreated = pwalletMain->CreateTransaction(vecSend, color, wtx, keyChange, nFeeRequired, nChangePosRet, strFailReason);
    if (!fCreated)
        throw JSONRPCError(RPC_WALLET_INSUFFICIENT_FUNDS, strFailReason);
    if (!pwalletMain->CommitTransaction(wtx, keyChange))
        throw JSONRPCError(RPC_WALLET_ERROR, "Transaction commit failed");

    return wtx.GetHash().GetHex();
}

// Defined in rpcmisc.cpp
extern CScript _createmultisig_redeemScript(const Array& params);

Value addmultisigaddress(const Array& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return Value::null;

    if (fHelp || params.size() < 2 || params.size() > 3)
    {
        string msg = "addmultisigaddress nrequired [\"key\",...] ( \"account\" )\n"
            "\nAdd a nrequired-to-sign multisignature address to the wallet.\n"
            "Each key is a Bitcoin address or hex-encoded public key.\n"
            "If 'account' is specified (DEPRECATED), assign address to that account.\n"

            "\nArguments:\n"
            "1. nrequired        (numeric, required) The number of required signatures out of the n keys or addresses.\n"
            "2. \"keysobject\"   (string, required) A json array of gcoin addresses or hex-encoded public keys\n"
            "     [\n"
            "       \"address\"  (string) gcoin address or hex-encoded public key\n"
            "       ...,\n"
            "     ]\n"
            "3. \"account\"      (string, optional) DEPRECATED. An account to assign the addresses to.\n"

            "\nResult:\n"
            "\"address\"  (string) A gcoin address associated with the keys.\n"

            "\nExamples:\n"
            "\nAdd a multisig address from 2 addresses\n"
            + HelpExampleCli("addmultisigaddress", "2 \"[\\\"16sSauSf5pF2UkUwvKGq4qjNRzBZYqgEL5\\\",\\\"171sgjn4YtPu27adkKGrdDwzRTxnRkBfKV\\\"]\"") +
            "\nAs json rpc call\n"
            + HelpExampleRpc("addmultisigaddress", "2, \"[\\\"16sSauSf5pF2UkUwvKGq4qjNRzBZYqgEL5\\\",\\\"171sgjn4YtPu27adkKGrdDwzRTxnRkBfKV\\\"]\"")
        ;
        throw runtime_error(msg);
    }

    LOCK2(cs_main, pwalletMain->cs_wallet);

    string strAccount;
    if (params.size() > 2)
        strAccount = AccountFromValue(params[2]);

    // Construct using pay-to-script-hash:
    CScript inner = _createmultisig_redeemScript(params);
    CScriptID innerID(inner);
    pwalletMain->AddCScript(inner);

    pwalletMain->SetAddressBook(innerID, strAccount, "send");
    return CBitcoinAddress(innerID).ToString();
}


struct tallyitem
{
    colorAmount_t colorAmount;
    int nConf;
    vector<uint256> txids;
    bool fIsWatchonly;
    tallyitem()
    {
        nConf = std::numeric_limits<int>::max();
        fIsWatchonly = false;
    }
};

Value ListReceived(const Array& params, bool fByAccounts)
{
    // Minimum confirmations
    int nMinDepth = 1;
    if (params.size() > 0)
        nMinDepth = params[0].get_int();

    // Whether to include empty accounts
    bool fIncludeEmpty = false;
    if (params.size() > 1)
        fIncludeEmpty = params[1].get_bool();

    isminefilter filter = ISMINE_SPENDABLE;
    if (params.size() > 2)
        if (params[2].get_bool())
            filter = filter | ISMINE_WATCH_ONLY;

    // Tally
    map<CBitcoinAddress, tallyitem> mapTally;
    for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); ++it) {
        const CWalletTx& wtx = (*it).second;

        if (wtx.IsCoinBase() || !CheckFinalTx(wtx))
            continue;

        int nDepth = wtx.GetDepthInMainChain();
        if (nDepth < nMinDepth)
            continue;

        BOOST_FOREACH(const CTxOut& txout, wtx.vout) {
            CTxDestination address;
            if (!ExtractDestination(txout.scriptPubKey, address))
                continue;

            isminefilter mine = IsMine(*pwalletMain, address);
            if (!(mine & filter))
                continue;

            tallyitem& item = mapTally[address];
            if (!item.colorAmount.count(txout.color))
                item.colorAmount[txout.color] = 0;
            item.colorAmount[txout.color] += txout.nValue;
            item.nConf = std::min(item.nConf, nDepth);
            item.txids.push_back(wtx.GetHash());
            if (mine & ISMINE_WATCH_ONLY)
                item.fIsWatchonly = true;
        }
    }

    // Reply
    Array ret;
    map<string, tallyitem> mapAccountTally;
    BOOST_FOREACH(const PAIRTYPE(CBitcoinAddress, CAddressBookData)& item, pwalletMain->mapAddressBook) {
        const CBitcoinAddress& address = item.first;
        const string& strAccount = item.second.name;
        map<CBitcoinAddress, tallyitem>::iterator it = mapTally.find(address);
        if (it == mapTally.end() && !fIncludeEmpty)
            continue;

        colorAmount_t colorAmount;
        int nConf = std::numeric_limits<int>::max();
        bool fIsWatchonly = false;
        if (it != mapTally.end()) {
            colorAmount = (*it).second.colorAmount;
            nConf = (*it).second.nConf;
            fIsWatchonly = (*it).second.fIsWatchonly;
        }

        if (fByAccounts) {
            tallyitem& item = mapAccountTally[strAccount];
            for (colorAmount_t::iterator itcA = colorAmount.begin(); itcA != colorAmount.end(); itcA++) {
                if (!item.colorAmount.count((*itcA).first))
                    item.colorAmount[(*itcA).first] = 0;
                item.colorAmount[(*itcA).first] += (*itcA).second;
            }
            item.nConf = std::min(item.nConf, nConf);
            item.fIsWatchonly = fIsWatchonly;
        } else {
            Object obj;
            if (fIsWatchonly)
                obj.push_back(Pair("involvesWatchonly", true));
            obj.push_back(Pair("address",       address.ToString()));
            obj.push_back(Pair("account",       strAccount));
            obj.push_back(Pair("amount",        ValueFromAmount(colorAmount)));
            obj.push_back(Pair("confirmations", (nConf == std::numeric_limits<int>::max() ? 0 : nConf)));
            Array transactions;
            if (it != mapTally.end()) {
                BOOST_FOREACH(const uint256& item, (*it).second.txids)
                    transactions.push_back(item.GetHex());
            }


            obj.push_back(Pair("txids", transactions));
            ret.push_back(obj);
        }
    }

    if (fByAccounts) {
        for (map<string, tallyitem>::iterator it = mapAccountTally.begin(); it != mapAccountTally.end(); ++it) {
            colorAmount_t colorAmount = (*it).second.colorAmount;
            int nConf = (*it).second.nConf;
            Object obj;
            if ((*it).second.fIsWatchonly)
                obj.push_back(Pair("involvesWatchonly", true));
            obj.push_back(Pair("account",       (*it).first));
            obj.push_back(Pair("amount",        ValueFromAmount(colorAmount)));
            obj.push_back(Pair("confirmations", (nConf == std::numeric_limits<int>::max() ? 0 : nConf)));
            ret.push_back(obj);
        }
    }

    return ret;
}

Value listreceivedbyaddress(const Array& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return Value::null;

    if (fHelp || params.size() > 3)
        throw runtime_error(
            _(__func__) + " ( minconf includeempty includeWatchonly)\n"
            "\nList balances by receiving address.\n"
            "\nArguments:\n"
            "1. minconf       (numeric, optional, default=1) The minimum number of confirmations before payments are included.\n"
            "2. includeempty  (numeric, optional, default=false) Whether to include addresses that haven't received any payments.\n"
            "3. includeWatchonly (bool, optional, default=false) Whether to include watchonly addresses (see 'importaddress').\n"

            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"involvesWatchonly\" : true,        (bool) Only returned if imported addresses were involved in transaction\n"
            "    \"address\" : \"receivingaddress\",  (string) The receiving address\n"
            "    \"account\" : \"accountname\",       (string) DEPRECATED. The account of the receiving address. The default account is \"\".\n"
            "    [                                    (json array of string : numeric)\n"
            "       \"color\" : amount                (string : numeric) The total amount in gcoin corresponding to color received at this address\n"
            "       ,...\n"
            "    ]\n"
            "    \"confirmations\" : n                (numeric) The number of confirmations of the most recent transaction included\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nExamples:\n"
            + HelpExampleCli("listreceivedbyaddress", "")
            + HelpExampleCli("listreceivedbyaddress", "6 true")
            + HelpExampleRpc("listreceivedbyaddress", "6, true, true")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    return ListReceived(params, false);
}

Value listreceivedbyaccount(const Array& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return Value::null;

    if (fHelp || params.size() > 3)
        throw runtime_error(
            "listreceivedbyaccount ( minconf includeempty includeWatchonly)\n"
            "\nDEPRECATED. List balances by account.\n"
            "\nArguments:\n"
            "1. minconf      (numeric, optional, default=1) The minimum number of confirmations before payments are included.\n"
            "2. includeempty (boolean, optional, default=false) Whether to include accounts that haven't received any payments.\n"
            "3. includeWatchonly (bool, optional, default=false) Whether to include watchonly addresses (see 'importaddress').\n"

            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"involvesWatchonly\" : true,   (bool) Only returned if imported addresses were involved in transaction\n"
            "    \"account\" : \"accountname\",  (string) The account name of the receiving account\n"
            "    [                               (json array of string : numeric)\n"
            "       \"color\" : amount           (string : numeric) The total amount in btc corresponding to color received at this address\n"
            "       ,...\n"
            "    ]\n"
            "    \"confirmations\" : n           (numeric) The number of confirmations of the most recent transaction included\n"
            "  }\n"
            "  ,...\n"
            "]\n"

            "\nExamples:\n"
            + HelpExampleCli("listreceivedbyaccount", "")
            + HelpExampleCli("listreceivedbyaccount", "6 true")
            + HelpExampleRpc("listreceivedbyaccount", "6, true, true")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    return ListReceived(params, true);
}

static void MaybePushAddress(Object & entry, const CTxDestination &dest)
{
    CBitcoinAddress addr;
    if (addr.Set(dest))
        entry.push_back(Pair("address", addr.ToString()));
}

void ListTransactions(const CWalletTx& wtx, const string& strAccount, int nMinDepth, bool fLong, Array& ret, const isminefilter& filter)
{
    CAmount nFee;
    string strSentAccount;
    list<COutputEntry> listReceived;
    list<COutputEntry> listSent;

    wtx.GetAmounts(listReceived, listSent, nFee, strSentAccount, filter);

    bool fAllAccounts = (strAccount == string("*"));
    bool involvesWatchonly = wtx.IsFromMe(ISMINE_WATCH_ONLY);

    // Sent
    if ((!listSent.empty() || nFee != 0) && (fAllAccounts || strAccount == strSentAccount)) {
        BOOST_FOREACH(const COutputEntry& s, listSent) {
            Object entry;
            if (involvesWatchonly || (::IsMine(*pwalletMain, s.destination) & ISMINE_WATCH_ONLY))
                entry.push_back(Pair("involvesWatchonly", true));
            entry.push_back(Pair("account", strSentAccount));
            MaybePushAddress(entry, s.destination);
            entry.push_back(Pair("category", "send"));
            entry.push_back(Pair("amount", ValueFromAmount(-s.amount)));
            entry.push_back(Pair("vout", s.vout));
            entry.push_back(Pair("fee", ValueFromAmount(-nFee)));
            if (fLong)
                WalletTxToJSON(wtx, entry);
            ret.push_back(entry);
        }
    }

    // Received
    if (listReceived.size() > 0 && wtx.GetDepthInMainChain() >= nMinDepth) {
        BOOST_FOREACH(const COutputEntry& r, listReceived) {
            string account;
            if (pwalletMain->mapAddressBook.count(r.destination))
                account = pwalletMain->mapAddressBook[r.destination].name;
            if (fAllAccounts || (account == strAccount)) {
                Object entry;
                if (involvesWatchonly || (::IsMine(*pwalletMain, r.destination) & ISMINE_WATCH_ONLY))
                    entry.push_back(Pair("involvesWatchonly", true));
                entry.push_back(Pair("account", account));
                MaybePushAddress(entry, r.destination);
                if (wtx.IsCoinBase()) {
                    if (wtx.GetDepthInMainChain() < 1)
                        entry.push_back(Pair("category", "orphan"));
                    else if (wtx.GetBlocksToMaturity() > 0)
                        entry.push_back(Pair("category", "immature"));
                    else
                        entry.push_back(Pair("category", "generate"));
                } else {
                    entry.push_back(Pair("category", "receive"));
                }
                entry.push_back(Pair("amount", ValueFromAmount(r.amount)));
                entry.push_back(Pair("vout", r.vout));
                if (fLong)
                    WalletTxToJSON(wtx, entry);
                ret.push_back(entry);
            }
        }
    }
}

void AcentryToJSON(const CAccountingEntry& acentry, const string& strAccount, Array& ret)
{
    bool fAllAccounts = (strAccount == string("*"));

    if (fAllAccounts || acentry.strAccount == strAccount) {
        Object entry;
        entry.push_back(Pair("account", acentry.strAccount));
        entry.push_back(Pair("category", "move"));
        entry.push_back(Pair("time", acentry.nTime));
        entry.push_back(Pair("amount", ValueFromAmount(acentry.nCreditDebit)));
        entry.push_back(Pair("otheraccount", acentry.strOtherAccount));
        entry.push_back(Pair("comment", acentry.strComment));
        ret.push_back(entry);
    }
}

Value listtransactions(const Array& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return Value::null;

    if (fHelp || params.size() > 4)
        throw runtime_error(
            _(__func__) + " ( \"account\" count from includeWatchonly)\n"
            "\nReturns up to 'count' most recent transactions skipping the first 'from' transactions for account 'account'.\n"
            "\nArguments:\n"
            "1. \"account\"    (string, optional) DEPRECATED. The account name. Should be \"*\".\n"
            "2. count          (numeric, optional, default=10) The number of transactions to return\n"
            "3. from           (numeric, optional, default=0) The number of transactions to skip\n"
            "4. includeWatchonly (bool, optional, default=false) Include transactions to watchonly addresses (see 'importaddress')\n"
            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"account\":\"accountname\",   (string) DEPRECATED. The account name associated with the transaction. \n"
            "                                                It will be \"\" for the default account.\n"
            "    \"address\":\"address\",       (string) The gcoin address of the transaction. Not present for \n"
            "                                                move transactions (category = move).\n"
            "    \"category\":\"send|receive|move\", (string) The transaction category. 'move' is a local (off blockchain)\n"
            "                                                transaction between accounts, and not associated with an address,\n"
            "                                                transaction id or block. 'send' and 'receive' transactions are \n"
            "                                                associated with an address, transaction id and block details\n"
            "    \"amount\": x.xxx,     (numeric) The amount in gcoin. This is negative for the 'send' category, and for the\n"
            "                                         'move' category for moves outbound. It is positive for the 'receive' category,\n"
            "                                         and for the 'move' category for inbound funds.\n"
            "    \"vout\" : n,          (numeric) the vout value\n"
            "    \"fee\": x.xxx,        (numeric) The amount of the fee in gcoin. This is negative and only available for the \n"
            "                                         'send' category of transactions.\n"
            "    \"confirmations\": n,  (numeric) The number of confirmations for the transaction. Available for 'send' and \n"
            "                                         'receive' category of transactions.\n"
            "    \"blockhash\": \"hashvalue\", (string) The block hash containing the transaction. Available for 'send' and 'receive'\n"
            "                                          category of transactions.\n"
            "    \"blockindex\": n,     (numeric) The block index containing the transaction. Available for 'send' and 'receive'\n"
            "                                          category of transactions.\n"
            "    \"txid\": \"transactionid\", (string) The transaction id. Available for 'send' and 'receive' category of transactions.\n"
            "    \"time\": xxx,         (numeric) The transaction time in seconds since epoch (midnight Jan 1 1970 GMT).\n"
            "    \"timereceived\": xxx, (numeric) The time received in seconds since epoch (midnight Jan 1 1970 GMT). Available \n"
            "                                          for 'send' and 'receive' category of transactions.\n"
            "    \"comment\": \"...\",  (string) If a comment is associated with the transaction.\n"
            "    \"otheraccount\": \"accountname\",  (string) For the 'move' category of transactions, the account the funds came \n"
            "                                          from (for receiving funds, positive amounts), or went to (for sending funds,\n"
            "                                          negative amounts).\n"
            "  }\n"
            "]\n"

            "\nExamples:\n"
            "\nList the most recent 10 transactions in the systems\n"
            + HelpExampleCli("listtransactions", "") +
            "\nList transactions 100 to 120\n"
            + HelpExampleCli("listtransactions", "\"*\" 20 100") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("listtransactions", "\"*\", 20, 100")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    string strAccount = "*";
    if (params.size() > 0)
        strAccount = params[0].get_str();
    int nCount = 10;
    if (params.size() > 1)
        nCount = params[1].get_int();
    int nFrom = 0;
    if (params.size() > 2)
        nFrom = params[2].get_int();
    isminefilter filter = ISMINE_SPENDABLE;
    if (params.size() > 3)
        if (params[3].get_bool())
            filter = filter | ISMINE_WATCH_ONLY;

    if (nCount < 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Negative count");
    if (nFrom < 0)
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Negative from");

    Array ret;

    list<CAccountingEntry> acentries;
    CWallet::TxItems txOrdered = pwalletMain->OrderedTxItems(acentries, strAccount);

    // iterate backwards until we have nCount items to return:
    for (CWallet::TxItems::reverse_iterator it = txOrdered.rbegin(); it != txOrdered.rend(); ++it) {
        CWalletTx *const pwtx = (*it).second.first;
        if (pwtx != 0)
            ListTransactions(*pwtx, strAccount, 0, true, ret, filter);

        CAccountingEntry *const pacentry = (*it).second.second;
        if (pacentry != 0)
            AcentryToJSON(*pacentry, strAccount, ret);

        if ((int)ret.size() >= (nCount+nFrom)) break;
    }
    // ret is newest to oldest

    if (nFrom > (int)ret.size())
        nFrom = ret.size();
    if ((nFrom + nCount) > (int)ret.size())
        nCount = ret.size() - nFrom;
    Array::iterator first = ret.begin();
    std::advance(first, nFrom);
    Array::iterator last = ret.begin();
    std::advance(last, nFrom+nCount);

    if (last != ret.end()) ret.erase(last, ret.end());
    if (first != ret.begin()) ret.erase(ret.begin(), first);

    std::reverse(ret.begin(), ret.end()); // Return oldest to newest

    return ret;
}

Value listwalletaddress(const Array& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return Value::null;

    if (fHelp || params.size() > 2)
        throw runtime_error(
            _(__func__) + " \"group-of-addresses\" \"number-of-addresses\"\n"
            "\n List addresses in the wallet.\n"
            + HelpRequiringPassphrase() +
            "\nArguments:\n"
            "1. group-of-addresses  (string, optional) The group you select to get. (-a:all, -i:imported, -p: keypool, others: label of keys).\n"
            "2. number-of-addresses (unsigned_int, optional) The number of addresses you want to get from your wallet.\n"
            "\nResult:\n"
            "{\n"
            "   \"address\", (string) An address in your wallet.\n"
            "   ...\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("listwalletaddress", "-a 3")
            + HelpExampleRpc("listwalletaddress", "-a 3")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    std::set<CKeyID> keyids;
    pwalletMain->GetKeys(keyids);

    std::vector<CPubKey> keys;
    pwalletMain->ViewKeyPool(keys);

    string group = "-a";
    if (params.size() >= 1)
            group = params[0].get_str();

    // number of address
    int number = keyids.size() + keys.size();
    if (params.size() == 2)
        number = params[1].get_int();

    Array a;
    int count = 0;
    if (group == "-a") { // list all addresses in wallet
        for (std::set<CKeyID>::iterator it = keyids.begin(); it != keyids.end(); ++it) {
            if (count >= number)
                break;
            string addr = CBitcoinAddress(*it).ToString();
            a.push_back(addr);
            count++;
        }
    } else if (group == "-i") { // list addresses imported
        for (std::set<CKeyID>::iterator it = keyids.begin(); it != keyids.end(); ++it) {
            if (count >= number)
                break;
            string addr = CBitcoinAddress(*it).ToString();
            if (pwalletMain->mapKeyMetadata[*it].fromImport) {
                 a.push_back(addr);
                 count++;
            }
        }
    } else if (group == "-p") { // list addresses from keypool
        for (std::vector<CPubKey>::iterator it = keys.begin(); it != keys.end(); ++it) {
            if (count >= number)
                break;
            string addr = CBitcoinAddress((*it).GetID()).ToString();
            a.push_back(addr);
            count++;
        }
    } else { // list address match the label
       for (std::set<CKeyID>::iterator it = keyids.begin(); it != keyids.end(); ++it) {
            if (count >= number)
                break;
            string addr = CBitcoinAddress(*it).ToString();
            if (pwalletMain->mapAddressBook[*it].name == group) {
                 a.push_back(addr);
                 count++;
            }
        }
    }
    return a;
}

Value listonewalletaddress(const Array& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return Value::null;

    if (fHelp || params.size() > 1)
        throw runtime_error(
            _(__func__) + " address-rank\n"
            "\n List one address in the wallet.\n"
            + HelpRequiringPassphrase() +
            "\nArguments:\n"
            "1. address-rank    (unsigned_int, optional) The rank of the address in the wallet.\n"
            "\nResult:\n"
            "\"address\" (string)    The {address-rank}th address in your wallet\n"
            "\nExamples:\n"
            + HelpExampleCli("listonewalletaddress", "3")
            + HelpExampleRpc("listonewalletaddress", "3")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    int rank = 0;
    if (params.size() == 1)
        rank = params[0].get_int();
    std::vector<CPubKey> keys;
    pwalletMain->ViewKeyPool(keys);
    if ((unsigned int)rank >= keys.size())
        throw JSONRPCError(RPC_INVALID_PARAMETER, "the rank is larger than the key pool size");

    Object obj;
    string addr = CBitcoinAddress(keys[rank].GetID()).ToString();
    obj.push_back(Pair("address", addr));

    return obj;
}

// Generate new address randomly that does not exist in keypool.
Value gennewaddress(const Array& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return Value::null;

    if (fHelp || params.size() != 1)
        throw runtime_error(
            _(__func__) + "( \"account\" )\n"
            "\nGenerate and return new Bitcoin address for receiving payments.\n"
            "\nArguments:\n"
            "1. \"number\"      (int) Number of address you want to generate\n"
            "\nResult:\n"
            "{\n"
            "   \"address\", (string) An address you gen.\n"
            "   ...\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("gennewaddress", "\"number\"")
            + HelpExampleRpc("gennewaddress", "\"number\"")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    int number = params[0].get_int();

    Array a;
    for (int i = 0; i < number; i++) {
        // Generate a new key that is added to wallet
        CPubKey newKey = pwalletMain->GenerateNewKey();
        CKeyID keyID = newKey.GetID();

        a.push_back(CBitcoinAddress(keyID).ToString());
    }
    return a;
}

Value listaccounts(const Array& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return Value::null;

    if (fHelp || params.size() > 2)
        throw runtime_error(
            "listaccounts ( minconf includeWatchonly)\n"
            "\nDEPRECATED. Returns Object that has account names as keys, account balances as values.\n"
            "\nArguments:\n"
            "1. minconf     (numeric, optional, default=1) Only include transactions with at least this many confirmations\n"
            "2. includeWatchonly (bool, optional, default=false) Include balances in watchonly addresses (see 'importaddress')\n"
            "\nResult:\n"
            "{                      (json object where keys are account names, and values are numeric balances\n"
            "  \"account\": x.xxx,  (numeric) The property name is the account name, and the value is the total balance for the account.\n"
            "  ...\n"
            "}\n"
            "\nExamples:\n"
            "\nList account balances where there at least 1 confirmation\n"
            + HelpExampleCli("listaccounts", "") +
            "\nList account balances including zero confirmation transactions\n"
            + HelpExampleCli("listaccounts", "0") +
            "\nList account balances for 6 or more confirmations\n"
            + HelpExampleCli("listaccounts", "6") +
            "\nAs json rpc call\n"
            + HelpExampleRpc("listaccounts", "6")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    int nMinDepth = 1;
    if (params.size() > 0)
        nMinDepth = params[0].get_int();
    isminefilter includeWatchonly = ISMINE_SPENDABLE;
    if (params.size() > 1)
        if (params[1].get_bool())
            includeWatchonly = includeWatchonly | ISMINE_WATCH_ONLY;

    map<string, colorAmount_t > mapAccountBalances;
    BOOST_FOREACH(const PAIRTYPE(CTxDestination, CAddressBookData)& entry, pwalletMain->mapAddressBook) {
        if (IsMine(*pwalletMain, entry.first) & includeWatchonly) // This address belongs to me
            mapAccountBalances[entry.second.name] = colorAmount_t();
    }

    for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); ++it) {
        const CWalletTx& wtx = (*it).second;
        CAmount nFee;
        string strSentAccount;
        list<COutputEntry> listReceived;
        list<COutputEntry> listSent;
        int nDepth = wtx.GetDepthInMainChain();
        if (wtx.GetBlocksToMaturity() > 0 || nDepth < 0)
            continue;
        wtx.GetAmounts(listReceived, listSent, nFee, strSentAccount, includeWatchonly);

        BOOST_FOREACH(const COutputEntry& s, listSent) {
            if (mapAccountBalances.count(strSentAccount) == 0 || mapAccountBalances[strSentAccount].count(s.color) == 0)
                mapAccountBalances[strSentAccount][s.color] = 0;
            mapAccountBalances[strSentAccount][s.color] -= s.amount;
        }
        if (nDepth >= nMinDepth) {
            BOOST_FOREACH(const COutputEntry& r, listReceived) {
                if (pwalletMain->mapAddressBook.count(r.destination))
                    mapAccountBalances[pwalletMain->mapAddressBook[r.destination].name][r.color] += r.amount;
                else {

                    if (mapAccountBalances.count("") == 0 || mapAccountBalances[""].count(r.color) == 0)
                        mapAccountBalances[""][r.color] = 0;

                    mapAccountBalances[""][r.color] += r.amount;
                }
            }
        }
    }

    list<CAccountingEntry> acentries;
    CWallet::TxItems txOrdered = pwalletMain->OrderedTxItems(acentries, "*");
    for (CWallet::TxItems::iterator it = txOrdered.begin(); it != txOrdered.end(); ++it) {
        CWalletTx *const pwtx = (*it).second.first;
        CAccountingEntry *const entry = (*it).second.second;
        // if we can't get wallet tx or account info
        BOOST_FOREACH(const CTxOut txout, pwtx->vout) {
            if (pwtx != 0 && entry != 0) {
                if (mapAccountBalances.count(entry->strAccount) == 0 || mapAccountBalances[entry->strAccount].count(txout.color) == 0)
                    mapAccountBalances[entry->strAccount][txout.color] = 0;
                if (entry->nCreditDebit.count(txout.color))
                    mapAccountBalances[entry->strAccount][txout.color] += entry->nCreditDebit[txout.color];
            }
        }
    }

    Object ret;
    for (map<string, colorAmount_t >::const_iterator it1 = mapAccountBalances.begin(); it1 != mapAccountBalances.end(); it1++)
        ret.push_back(Pair((*it1).first, ValueFromAmount((*it1).second)));
    return ret;
}

Value listsinceblock(const Array& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return Value::null;

    if (fHelp)
        throw runtime_error(
            _(__func__) + " ( \"blockhash\" target-confirmations includeWatchonly)\n"
            "\nGet all transactions in blocks since block [blockhash], or all transactions if omitted\n"
            "\nArguments:\n"
            "1. \"blockhash\"   (string, optional) The block hash to list transactions since\n"
            "2. target-confirmations:    (numeric, optional) The confirmations required, must be 1 or more\n"
            "3. includeWatchonly:        (bool, optional, default=false) Include transactions to watchonly addresses (see 'importaddress')"
            "\nResult:\n"
            "{\n"
            "  \"transactions\": [\n"
            "    \"account\":\"accountname\",       (string) DEPRECATED. The account name associated with the transaction. Will be \"\" for the default account.\n"
            "    \"address\":\"address\",    (string) The gcoin address of the transaction. Not present for move transactions (category = move).\n"
            "    \"category\":\"send|receive\",     (string) The transaction category. 'send' has negative amounts, 'receive' has positive amounts.\n"
            "    \"amount\": x.xxx,          (numeric) The amount in gcoin. This is negative for the 'send' category, and for the 'move' category for moves \n"
            "                                          outbound. It is positive for the 'receive' category, and for the 'move' category for inbound funds.\n"
            "    \"vout\" : n,               (numeric) the vout value\n"
            "    \"fee\": x.xxx,             (numeric) The amount of the fee in gcoin. This is negative and only available for the 'send' category of transactions.\n"
            "    \"confirmations\": n,       (numeric) The number of confirmations for the transaction. Available for 'send' and 'receive' category of transactions.\n"
            "    \"blockhash\": \"hashvalue\",     (string) The block hash containing the transaction. Available for 'send' and 'receive' category of transactions.\n"
            "    \"blockindex\": n,          (numeric) The block index containing the transaction. Available for 'send' and 'receive' category of transactions.\n"
            "    \"blocktime\": xxx,         (numeric) The block time in seconds since epoch (1 Jan 1970 GMT).\n"
            "    \"txid\": \"transactionid\",  (string) The transaction id. Available for 'send' and 'receive' category of transactions.\n"
            "    \"time\": xxx,              (numeric) The transaction time in seconds since epoch (Jan 1 1970 GMT).\n"
            "    \"timereceived\": xxx,      (numeric) The time received in seconds since epoch (Jan 1 1970 GMT). Available for 'send' and 'receive' category of transactions.\n"
            "    \"comment\": \"...\",       (string) If a comment is associated with the transaction.\n"
            "    \"to\": \"...\",            (string) If a comment to is associated with the transaction.\n"
             "  ],\n"
            "  \"lastblock\": \"lastblockhash\"     (string) The hash of the last block\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("listsinceblock", "")
            + HelpExampleCli("listsinceblock", "\"000000000000000bacf66f7497b7dc45ef753ee9a7d38571037cdb1a57f663ad\" 6")
            + HelpExampleRpc("listsinceblock", "\"000000000000000bacf66f7497b7dc45ef753ee9a7d38571037cdb1a57f663ad\", 6")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    CBlockIndex *pindex = NULL;
    int target_confirms = 1;
    isminefilter filter = ISMINE_SPENDABLE;

    if (params.size() > 0) {
        uint256 blockId;

        blockId.SetHex(params[0].get_str());
        BlockMap::iterator it = mapBlockIndex.find(blockId);
        if (it != mapBlockIndex.end())
            pindex = it->second;
    }

    if (params.size() > 1) {
        target_confirms = params[1].get_int();

        if (target_confirms < 1)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter");
    }

    if (params.size() > 2)
        if (params[2].get_bool())
            filter = filter | ISMINE_WATCH_ONLY;

    int depth = pindex ? (1 + chainActive.Height() - pindex->nHeight) : -1;

    Array transactions;

    for (map<uint256, CWalletTx>::iterator it = pwalletMain->mapWallet.begin(); it != pwalletMain->mapWallet.end(); it++) {
        CWalletTx tx = (*it).second;

        if (depth == -1 || tx.GetDepthInMainChain() < depth)
            ListTransactions(tx, "*", 0, true, transactions, filter);
    }

    CBlockIndex *pblockLast = chainActive[chainActive.Height() + 1 - target_confirms];
    uint256 lastblock = pblockLast ? pblockLast->GetBlockHash() : uint256();

    Object ret;
    ret.push_back(Pair("transactions", transactions));
    ret.push_back(Pair("lastblock", lastblock.GetHex()));

    return ret;
}

Value gettransaction(const Array& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return Value::null;

    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            "gettransaction \"txid\" ( includeWatchonly )\n"
            "\nGet detailed information about in-wallet transaction <txid>\n"
            "\nArguments:\n"
            "1. \"txid\"    (string, required) The transaction id\n"
            "2. \"includeWatchonly\"    (bool, optional, default=false) Whether to include watchonly addresses in balance calculation and details[]\n"
            "\nResult:\n"
            "{\n"
            "  \"amount\" : x.xxx,          (numeric) The transaction amount in gcoin\n"
            "  \"confirmations\" : n,       (numeric) The number of confirmations\n"
            "  \"blockhash\" : \"hash\",    (string) The block hash\n"
            "  \"blockindex\" : xx,         (numeric) The block index\n"
            "  \"blocktime\" : ttt,         (numeric) The time in seconds since epoch (1 Jan 1970 GMT)\n"
            "  \"txid\" : \"transactionid\",   (string) The transaction id.\n"
            "  \"time\" : ttt,              (numeric) The transaction time in seconds since epoch (1 Jan 1970 GMT)\n"
            "  \"timereceived\" : ttt,      (numeric) The time received in seconds since epoch (1 Jan 1970 GMT)\n"
            "  \"details\" : [\n"
            "    {\n"
            "      \"account\" : \"accountname\",   (string) DEPRECATED. The account name involved in the transaction, can be \"\" for the default account.\n"
            "      \"address\" : \"address\",       (string) The gcoin address involved in the transaction\n"
            "      \"category\" : \"send|receive\", (string) The category, either 'send' or 'receive'\n"
            "      \"amount\" : x.xxx               (numeric) The amount in gcoin\n"
            "      \"vout\" : n,                    (numeric) the vout value\n"
            "    }\n"
            "    ,...\n"
            "  ],\n"
            "  \"hex\" : \"data\"           (string) Raw data for transaction\n"
            "}\n"

            "\nExamples:\n"
            + HelpExampleCli("gettransaction", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\"")
            + HelpExampleCli("gettransaction", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\" true")
            + HelpExampleRpc("gettransaction", "\"1075db55d416d3ca199f55b6084e2115b9345e16c5cf302fc80e9d5fbf5d48d\"")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    uint256 hash;
    hash.SetHex(params[0].get_str());

    isminefilter filter = ISMINE_SPENDABLE;
    if (params.size() > 1)
        if (params[1].get_bool())
            filter = filter | ISMINE_WATCH_ONLY;

    Object entry;
    if (!pwalletMain->mapWallet.count(hash))
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid or non-wallet transaction id");
    const CWalletTx& wtx = pwalletMain->mapWallet[hash];

    CAmount nCredit = wtx.GetCredit(filter);
    CAmount nDebit = wtx.GetDebit(filter);
    CAmount nNet = nCredit - nDebit;
    CAmount nFee = (wtx.IsFromMe(filter) ? wtx.GetValueOut() - nDebit : 0);

    entry.push_back(Pair("amount", ValueFromAmount(nNet - nFee)));
    if (wtx.IsFromMe(filter))
        entry.push_back(Pair("fee", ValueFromAmount(nFee)));

    WalletTxToJSON(wtx, entry);

    Array details;
    ListTransactions(wtx, "*", 0, false, details, filter);
    entry.push_back(Pair("details", details));

    string strHex = EncodeHexTx(static_cast<CTransaction>(wtx));
    entry.push_back(Pair("hex", strHex));

    return entry;
}


Value backupwallet(const Array& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return Value::null;

    if (fHelp || params.size() != 1)
        throw runtime_error(
            _(__func__) + " \"destination\"\n"
            "\nSafely copies wallet.dat to destination, which can be a directory or a path with filename.\n"
            "\nArguments:\n"
            "1. \"destination\"   (string) The destination directory or file\n"
            "\nExamples:\n"
            + HelpExampleCli("backupwallet", "\"backup.dat\"")
            + HelpExampleRpc("backupwallet", "\"backup.dat\"")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    string strDest = params[0].get_str();
    if (!BackupWallet(*pwalletMain, strDest))
        throw JSONRPCError(RPC_WALLET_ERROR, "Error: Wallet backup failed!");

    return Value::null;
}


Value keypoolrefill(const Array& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return Value::null;

    if (fHelp || params.size() > 1)
        throw runtime_error(
            _(__func__) + " ( newsize )\n"
            "\nFills the keypool."
            + HelpRequiringPassphrase() + "\n"
            "\nArguments\n"
            "1. newsize     (numeric, optional, default=100) The new keypool size\n"
            "\nExamples:\n"
            + HelpExampleCli("keypoolrefill", "")
            + HelpExampleRpc("keypoolrefill", "")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    // 0 is interpreted by TopUpKeyPool() as the default keypool size given by -keypool
    unsigned int kpSize = 0;
    if (params.size() > 0) {
        if (params[0].get_int() < 0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, expected valid size.");
        kpSize = (unsigned int)params[0].get_int();
    }

    EnsureWalletIsUnlocked();
    pwalletMain->TopUpKeyPool(kpSize);

    if (pwalletMain->GetKeyPoolSize() < kpSize)
        throw JSONRPCError(RPC_WALLET_ERROR, "Error refreshing keypool.");

    return Value::null;
}

// Fill up the keypool with hd key.
Value hdkeypoolrefill(const Array& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return Value::null;

    if (fHelp || params.size() > 1)
        throw runtime_error(
            _(__func__) + " ( newsize )\n"
            "\nFills the keypool."
            + HelpRequiringPassphrase() + "\n"
            "\nArguments\n"
            "1. newsize     (numeric, optional, default=100) The new keypool size\n"
            "\nExamples:\n"
            + HelpExampleCli("keypoolrefill", "")
            + HelpExampleRpc("keypoolrefill", "")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    // 0 is interpreted by TopUpKeyPool() as the default keypool size given by -keypool
    unsigned int kpSize = 0;
    if (params.size() > 0) {
        if (params[0].get_int() < 0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, expected valid size.");
        kpSize = (unsigned int)params[0].get_int();
    }

    EnsureWalletIsUnlocked();
    pwalletMain->HDTopUpKeyPool(kpSize);

    if (pwalletMain->GetKeyPoolSize() < kpSize)
        throw JSONRPCError(RPC_WALLET_ERROR, "Error refreshing keypool.");

    return Value::null;
}

static void LockWallet(CWallet* pWallet)
{
    LOCK(cs_nWalletUnlockTime);
    nWalletUnlockTime = 0;
    pWallet->Lock();
}

Value walletpassphrase(const Array& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return Value::null;

    if (pwalletMain->IsCrypted() && (fHelp || params.size() != 2))
        throw runtime_error(
            _(__func__) + " \"passphrase\" timeout\n"
            "\nStores the wallet decryption key in memory for 'timeout' seconds.\n"
            "This is needed prior to performing transactions related to private keys such as sending gcoins\n"
            "\nArguments:\n"
            "1. \"passphrase\"     (string, required) The wallet passphrase\n"
            "2. timeout            (numeric, required) The time to keep the decryption key in seconds.\n"
            "\nNote:\n"
            "Issuing the walletpassphrase command while the wallet is already unlocked will set a new unlock\n"
            "time that overrides the old one.\n"
            "\nExamples:\n"
            "\nunlock the wallet for 60 seconds\n"
            + HelpExampleCli("walletpassphrase", "\"my pass phrase\" 60") +
            "\nLock the wallet again (before 60 seconds)\n"
            + HelpExampleCli("walletlock", "") +
            "\nAs json rpc call\n"
            + HelpExampleRpc("walletpassphrase", "\"my pass phrase\", 60")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    if (fHelp)
        return true;
    if (!pwalletMain->IsCrypted())
        throw JSONRPCError(RPC_WALLET_WRONG_ENC_STATE, "Error: running with an unencrypted wallet, but walletpassphrase was called.");

    // Note that the walletpassphrase is stored in params[0] which is not mlock()ed
    SecureString strWalletPass;
    strWalletPass.reserve(100);
    // TODO: get rid of this .c_str() by implementing SecureString::operator=(std::string)
    // Alternately, find a way to make params[0] mlock()'d to begin with.
    strWalletPass = params[0].get_str().c_str();

    if (strWalletPass.length() > 0) {
        if (!pwalletMain->Unlock(strWalletPass))
            throw JSONRPCError(RPC_WALLET_PASSPHRASE_INCORRECT, "Error: The wallet passphrase entered was incorrect.");
    } else
        throw runtime_error(
            _(__func__) + " <passphrase> <timeout>\n"
            "Stores the wallet decryption key in memory for <timeout> seconds.");

    int64_t nSleepTime = params[1].get_int64();
    LOCK(cs_nWalletUnlockTime);
    nWalletUnlockTime = GetTime() + nSleepTime;
    RPCRunLater("lockwallet", boost::bind(LockWallet, pwalletMain), nSleepTime);

    return Value::null;
}


Value walletpassphrasechange(const Array& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return Value::null;

    if (pwalletMain->IsCrypted() && (fHelp || params.size() != 2))
        throw runtime_error(
            _(__func__) + " \"oldpassphrase\" \"newpassphrase\"\n"
            "\nChanges the wallet passphrase from 'oldpassphrase' to 'newpassphrase'.\n"
            "\nArguments:\n"
            "1. \"oldpassphrase\"      (string) The current passphrase\n"
            "2. \"newpassphrase\"      (string) The new passphrase\n"
            "\nExamples:\n"
            + HelpExampleCli("walletpassphrasechange", "\"old one\" \"new one\"")
            + HelpExampleRpc("walletpassphrasechange", "\"old one\", \"new one\"")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    if (fHelp)
        return true;
    if (!pwalletMain->IsCrypted())
        throw JSONRPCError(RPC_WALLET_WRONG_ENC_STATE, "Error: running with an unencrypted wallet, but walletpassphrasechange was called.");

    // TODO: get rid of these .c_str() calls by implementing SecureString::operator=(std::string)
    // Alternately, find a way to make params[0] mlock()'d to begin with.
    SecureString strOldWalletPass;
    strOldWalletPass.reserve(100);
    strOldWalletPass = params[0].get_str().c_str();

    SecureString strNewWalletPass;
    strNewWalletPass.reserve(100);
    strNewWalletPass = params[1].get_str().c_str();

    if (strOldWalletPass.length() < 1 || strNewWalletPass.length() < 1)
        throw runtime_error(
            _(__func__) + " <oldpassphrase> <newpassphrase>\n"
            "Changes the wallet passphrase from <oldpassphrase> to <newpassphrase>.");

    if (!pwalletMain->ChangeWalletPassphrase(strOldWalletPass, strNewWalletPass))
        throw JSONRPCError(RPC_WALLET_PASSPHRASE_INCORRECT, "Error: The wallet passphrase entered was incorrect.");

    return Value::null;
}


Value walletlock(const Array& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return Value::null;

    if (pwalletMain->IsCrypted() && (fHelp || params.size() != 0))
        throw runtime_error(
            _(__func__) + "\n"
            "\nRemoves the wallet encryption key from memory, locking the wallet.\n"
            "After calling this method, you will need to call walletpassphrase again\n"
            "before being able to call any methods which require the wallet to be unlocked.\n"
            "\nExamples:\n"
            "\nSet the passphrase for 2 minutes to perform a transaction\n"
            + HelpExampleCli("walletpassphrase", "\"my pass phrase\" 120") +
            "\nPerform a send (requires passphrase set)\n"
            + HelpExampleCli("sendtoaddress", "\"1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd\" 1.0") +
            "\nClear the passphrase since we are done before 2 minutes is up\n"
            + HelpExampleCli("walletlock", "") +
            "\nAs json rpc call\n"
            + HelpExampleRpc("walletlock", "")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    if (fHelp)
        return true;
    if (!pwalletMain->IsCrypted())
        throw JSONRPCError(RPC_WALLET_WRONG_ENC_STATE, "Error: running with an unencrypted wallet, but walletlock was called.");

    {
        LOCK(cs_nWalletUnlockTime);
        pwalletMain->Lock();
        nWalletUnlockTime = 0;
    }

    return Value::null;
}


Value encryptwallet(const Array& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return Value::null;

    if (!pwalletMain->IsCrypted() && (fHelp || params.size() != 1))
        throw runtime_error(
            _(__func__) + " \"passphrase\"\n"
            "\nEncrypts the wallet with 'passphrase'. This is for first time encryption.\n"
            "After this, any calls that interact with private keys such as sending or signing \n"
            "will require the passphrase to be set prior the making these calls.\n"
            "Use the walletpassphrase call for this, and then walletlock call.\n"
            "If the wallet is already encrypted, use the walletpassphrasechange call.\n"
            "Note that this will shutdown the server.\n"
            "\nArguments:\n"
            "1. \"passphrase\"    (string) The pass phrase to encrypt the wallet with. It must be at least 1 character, but should be long.\n"
            "\nExamples:\n"
            "\nEncrypt you wallet\n"
            + HelpExampleCli("encryptwallet", "\"my pass phrase\"") +
            "\nNow set the passphrase to use the wallet, such as for signing or sending gcoin\n"
            + HelpExampleCli("walletpassphrase", "\"my pass phrase\"") +
            "\nNow we can so something like sign\n"
            + HelpExampleCli("signmessage", "\"address\" \"test message\"") +
            "\nNow lock the wallet again by removing the passphrase\n"
            + HelpExampleCli("walletlock", "") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("encryptwallet", "\"my pass phrase\"")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    if (fHelp)
        return true;
    if (pwalletMain->IsCrypted())
        throw JSONRPCError(RPC_WALLET_WRONG_ENC_STATE, "Error: running with an encrypted wallet, but encryptwallet was called.");

    // TODO: get rid of this .c_str() by implementing SecureString::operator=(std::string)
    // Alternately, find a way to make params[0] mlock()'d to begin with.
    SecureString strWalletPass;
    strWalletPass.reserve(100);
    strWalletPass = params[0].get_str().c_str();

    if (strWalletPass.length() < 1)
        throw runtime_error(
            _(__func__) + " <passphrase>\n"
            "Encrypts the wallet with <passphrase>.");

    if (!pwalletMain->EncryptWallet(strWalletPass))
        throw JSONRPCError(RPC_WALLET_ENCRYPTION_FAILED, "Error: Failed to encrypt the wallet.");

    // BDB seems to have a bad habit of writing old data into
    // slack space in .dat files; that is bad if the old data is
    // unencrypted private keys. So:
    StartShutdown();
    return "wallet encrypted; Bitcoin server stopping, restart to run with encrypted wallet. The keypool has been flushed, you need to make a new backup.";
}

Value lockunspent(const Array& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return Value::null;

    if (fHelp || params.size() < 1 || params.size() > 2)
        throw runtime_error(
            _(__func__) + " unlock [{\"txid\":\"txid\",\"vout\":n},...]\n"
            "\nUpdates list of temporarily unspendable outputs.\n"
            "Temporarily lock (unlock=false) or unlock (unlock=true) specified transaction outputs.\n"
            "A locked transaction output will not be chosen by automatic coin selection, when spending gcoins.\n"
            "Locks are stored in memory only. Nodes start with zero locked outputs, and the locked output list\n"
            "is always cleared (by virtue of process exit) when a node stops or fails.\n"
            "Also see the listunspent call\n"
            "\nArguments:\n"
            "1. unlock            (boolean, required) Whether to unlock (true) or lock (false) the specified transactions\n"
            "2. \"transactions\"  (string, required) A json array of objects. Each object the txid (string) vout (numeric)\n"
            "     [           (json array of json objects)\n"
            "       {\n"
            "         \"txid\":\"id\",    (string) The transaction id\n"
            "         \"vout\": n         (numeric) The output number\n"
            "       }\n"
            "       ,...\n"
            "     ]\n"

            "\nResult:\n"
            "true|false    (boolean) Whether the command was successful or not\n"

            "\nExamples:\n"
            "\nList the unspent transactions\n"
            + HelpExampleCli("listunspent", "") +
            "\nLock an unspent transaction\n"
            + HelpExampleCli("lockunspent", "false \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\"") +
            "\nList the locked transactions\n"
            + HelpExampleCli("listlockunspent", "") +
            "\nUnlock the transaction again\n"
            + HelpExampleCli("lockunspent", "true \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\"") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("lockunspent", "false, \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\"")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    if (params.size() == 1)
        RPCTypeCheck(params, boost::assign::list_of(bool_type));
    else
        RPCTypeCheck(params, boost::assign::list_of(bool_type)(array_type));

    bool fUnlock = params[0].get_bool();

    if (params.size() == 1) {
        if (fUnlock)
            pwalletMain->UnlockAllCoins();
        return true;
    }

    Array outputs = params[1].get_array();
    BOOST_FOREACH(Value& output, outputs) {
        if (output.type() != obj_type)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, expected object");
        const Object& o = output.get_obj();

        RPCTypeCheck(o, boost::assign::map_list_of("txid", str_type)("vout", int_type));

        string txid = find_value(o, "txid").get_str();
        if (!IsHex(txid))
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, expected hex txid");

        int nOutput = find_value(o, "vout").get_int();
        if (nOutput < 0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid parameter, vout must be positive");

        COutPoint outpt(uint256S(txid), nOutput);

        if (fUnlock)
            pwalletMain->UnlockCoin(outpt);
        else
            pwalletMain->LockCoin(outpt);
    }

    return true;
}

Value listlockunspent(const Array& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return Value::null;

    if (fHelp || params.size() > 0)
        throw runtime_error(
            _(__func__) + "\n"
            "\nReturns list of temporarily unspendable outputs.\n"
            "See the lockunspent call to lock and unlock transactions for spending.\n"
            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"txid\" : \"transactionid\",     (string) The transaction id locked\n"
            "    \"vout\" : n                      (numeric) The vout value\n"
            "  }\n"
            "  ,...\n"
            "]\n"
            "\nExamples:\n"
            "\nList the unspent transactions\n"
            + HelpExampleCli("listunspent", "") +
            "\nLock an unspent transaction\n"
            + HelpExampleCli("lockunspent", "false \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\"") +
            "\nList the locked transactions\n"
            + HelpExampleCli("listlockunspent", "") +
            "\nUnlock the transaction again\n"
            + HelpExampleCli("lockunspent", "true \"[{\\\"txid\\\":\\\"a08e6907dbbd3d809776dbfc5d82e371b764ed838b5655e72f463568df1aadf0\\\",\\\"vout\\\":1}]\"") +
            "\nAs a json rpc call\n"
            + HelpExampleRpc("listlockunspent", "")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    vector<COutPoint> vOutpts;
    pwalletMain->ListLockedCoins(vOutpts);

    Array ret;

    BOOST_FOREACH(COutPoint &outpt, vOutpts) {
        Object o;

        o.push_back(Pair("txid", outpt.hash.GetHex()));
        o.push_back(Pair("vout", (int)outpt.n));
        ret.push_back(o);
    }

    return ret;
}

Value settxfee(const Array& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return Value::null;

    if (fHelp || params.size() < 1 || params.size() > 1)
        throw runtime_error(
            _(__func__) + " amount\n"
            "\nSet the transaction fee per kB.\n"
            "\nArguments:\n"
            "1. amount         (numeric, required) The transaction fee in BTC/kB rounded to the nearest 0.00000001\n"
            "\nResult\n"
            "true|false        (boolean) Returns true if successful\n"
            "\nExamples:\n"
            + HelpExampleCli("settxfee", "0.00001")
            + HelpExampleRpc("settxfee", "0.00001")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    // Amount
    CAmount nAmount = 0;
    if (params[0].get_real() != 0.0)
        nAmount = AmountFromValue(params[0]);        // rejects 0.0 amounts

    payTxFee = CFeeRate(nAmount, 1000);
    return true;
}

Value getwalletinfo(const Array& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return Value::null;

    if (fHelp || params.size() != 0)
        throw runtime_error(
            _(__func__) + "\n"
            "Returns an object containing various wallet state info.\n"
            "\nResult:\n"
            "{\n"
            "  \"walletversion\": xxxxx,        (numeric) the wallet version\n"
            "  \"balance\": xxxxxxx,            (numeric) the total confirmed gcoin balance of the wallet\n"
            "  \"unconfirmed_balance\": xxx,    (numeric) the total unconfirmed gcoin balance of the wallet\n"
            "  \"immature_balance\": xxxxxx,    (numeric) the total immature balance of the wallet\n"
            "  \"txcount\": xxxxxxx,            (numeric) the total number of transactions in the wallet\n"
            "  \"keypoololdest\": xxxxxx,       (numeric) the timestamp (seconds since GMT epoch) of the oldest pre-generated key in the key pool\n"
            "  \"keystoresize\": xxxx,          (numeric) how many new keys are stored\n"
            "  \"unlocked_until\": ttt,         (numeric) the timestamp in seconds since epoch (midnight Jan 1 1970 GMT) that the wallet is unlocked for transfers, or 0 if the wallet is locked\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getwalletinfo", "")
            + HelpExampleRpc("getwalletinfo", "")
        );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    Object obj;
    colorAmount_t color_amount;

    std::set<CKeyID> keyids;
    pwalletMain->GetKeys(keyids);

    obj.push_back(Pair("walletversion", pwalletMain->GetVersion()));
    pwalletMain->GetBalance(color_amount);
    obj.push_back(Pair("balance",       ValueFromAmount(color_amount)));
    obj.push_back(Pair("txcount",       (int)pwalletMain->mapWallet.size()));
    obj.push_back(Pair("keypoololdest", pwalletMain->GetOldestKeyPoolTime()));
    obj.push_back(Pair("keystoresize",   (int)keyids.size()));
    if (pwalletMain->IsCrypted())
        obj.push_back(Pair("unlocked_until", nWalletUnlockTime));
    return obj;
}

Value resendwallettransactions(const Array& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return Value::null;

    if (fHelp || params.size() != 0)
        throw runtime_error(
            "resendwallettransactions\n"
            "Immediately re-broadcast unconfirmed wallet transactions to all peers.\n"
            "Intended only for testing; the wallet code periodically re-broadcasts\n"
            "automatically.\n"
            "Returns array of transaction ids that were re-broadcast.\n"
            );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    vector<uint256> txids = pwalletMain->ResendWalletTransactionsBefore(GetTime());
    Array result;
    BOOST_FOREACH(const uint256& txid, txids)
    {
        result.push_back(txid.ToString());
    }
    return result;
}

Value mint(const Array& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return Value::null;

    if (fHelp || params.size() < 2 || params.size() > 3)
        throw runtime_error(
            _(__func__) + " \"amount\" color \n"
            "\nmint color-coin\n"
            + HelpRequiringPassphrase() +
            "\nArguments:\n"
            "2. \"amount\"      (numeric, required) The amount of gcoin you want to mint. eg 10\n"
            "2. \"color\"       (numeric, required) The color you want to mint. eg 5\n"
            "\nResult:\n"
            "\"transactionid\"  (string) The transaction id.\n"
            "\nExamples:\n"
            + HelpExampleCli("mint", "\"10\" 5")
            + HelpExampleRpc("mint", "\"10\", 5, \"donation\", \"seans outpost\"")
        );
    // Amount
    CAmount nAmount = params[0].get_int64();
    const type_Color color = ColorFromValue(params[1]);

    LOCK2(cs_main, pwalletMain->cs_wallet);

    // Wallet comments
    CWalletTx wtx;
    EnsureWalletIsUnlocked();
    string strError = pwalletMain->MintMoney(nAmount, color, wtx);
    if (strError != "")
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    return wtx.GetHash().GetHex();
}


/*
///////////
BIP32 stack
///////////

default chainpath after bip44
m = master key
<num>' or <num>h = hardened key
c stands for internal/external chain switch
  c=0 for external addresses
  c=1 for internal addresses

example "m/44'/0'/0'/c" will result in m/44'/0'/0'/0/0 for the first external key
example "m/44'/0'/0'/c" will result in m/44'/0'/0'/1/0 for the first internal key
example "m/44'/0'/0'/c" will result in m/44'/0'/0'/0/1 for the second external key
example "m/44'/0'/0'/c" will result in m/44'/0'/0'/1/1 for the second internal key
*/
const std::string hd_default_chainpath = "m/44'/0'/0'/c";

Value hdaddchain(const Array& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return Value::null;

    if (fHelp)
        throw runtime_error(
                            "hdaddchain (<chainpath>|default) (<masterseed_hex | master_priv_key>)\n"
                            "\nAdds a HD/Bip32 chain \n"
                            "\nArguments:\n"
                            "1. chainpath        (string, optional, default="+hd_default_chainpath+") chainpath for hd wallet structure\n"
                            "   m stands for master, c for internal/external key-switch, k stands for upcounting child key index"
                            "2. masterseed_hex   (string/hex, optional) use this seed for master key generation\n"
                            "2. master_priv_key  (string/base58check, optional) will import the given extended master private key for this chain of keys\n"
                            "\nResult\n"
                            "{\n"
                            "  \"seed_hex\" : \"<hexstr>\",  (string) seed used during master key generation (only if no masterseed hex was provided\n"
                            "}\n"

                            "\nExamples\n"
                            + HelpExampleCli("hdaddchain", "")
                            + HelpExampleCli("hdaddchain", "m/44'/0'/0'/c/k")
                            + HelpExampleRpc("hdaddchain", "m/44'/0'/0'/c/k")
                            );

    Object result;

    assert(pwalletMain != NULL);
    EnsureWalletIsUnlocked();

    const unsigned int bip32MasterSeedLength = 32;
    CKeyingMaterial vSeed = CKeyingMaterial(bip32MasterSeedLength);
    bool fGenerateMasterSeed = true;
    HDChainID chainId;
    std::string chainPath = hd_default_chainpath;
    if (params.size() > 0 && params[0].get_str() != "default")
        chainPath = params[0].get_str(); //todo bip32 chainpath sanity

    std::string xpubOut;
    std::string xprivOut;
    if (params.size() > 1)
    {
        if (params[1].get_str().size() > 32*2) //hex
        {
            //assume it's a base58check encoded key
            xprivOut = params[1].get_str();
        }
        else
        {
            if (!IsHex(params[1].get_str()))
                throw runtime_error("HD master seed must be encoded in hex");

            std::vector<unsigned char> seed = ParseHex(params[1].get_str());
            if (seed.size() != bip32MasterSeedLength)
                throw runtime_error("HD master seed must be "+itostr(bip32MasterSeedLength*8)+"bit");

            memcpy(&vSeed[0], &seed[0], bip32MasterSeedLength);
            memory_cleanse(&seed[0], bip32MasterSeedLength);
            fGenerateMasterSeed = false;
        }
    }



    pwalletMain->HDAddHDChain(chainPath, fGenerateMasterSeed, vSeed, chainId, xprivOut, xpubOut);
    if (fGenerateMasterSeed)
        result.push_back(Pair("seed_hex", HexStr(vSeed)));

    result.push_back(Pair("extended_master_pubkey", xpubOut));
    result.push_back(Pair("extended_master_privkey", xprivOut));
    result.push_back(Pair("chainid", chainId.GetHex()));

    memory_cleanse(&vSeed[0], bip32MasterSeedLength);
    memory_cleanse(&xprivOut[0], xpubOut.size());
    memory_cleanse(&xpubOut[0], xpubOut.size());

    return result;
}

Value hdsetchain(const Array& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return Value::null;

    if (fHelp || params.size() != 1)
        throw runtime_error(
                            "hdsetchain <chainid>\n"
                            "\nReturns some hd relevant information.\n"
                            "\nArguments:\n"
                            "1. \"chainid\"        (string|hex, required) chainid is a gcoin hash of the master public key of the corresponding chain.\n"
                            "\nExamples:\n"
                            + HelpExampleCli("hdsetchain", "")
                            + HelpExampleCli("hdgetinfo", "True")
                            + HelpExampleRpc("hdgetinfo", "")
                            );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    HDChainID chainId;
    if (!IsHex(params[0].get_str()))
        throw runtime_error("Chain id format is invalid");

    chainId.SetHex(params[0].get_str());

    if (!pwalletMain->HDSetActiveChainID(chainId))
        throw runtime_error("Could not set active chain");

    return Value::null;
}

Value hdgetinfo(const Array& params, bool fHelp)
{
    if (!EnsureWalletIsAvailable(fHelp))
        return Value::null;

    if (fHelp || params.size() != 0)
        throw runtime_error(
                            "hdgetinfo\n"
                            "\nReturns some hd relevant information.\n"
                            "\nArguments:\n"
                            "{\n"
                            "  \"chainid\" : \"<chainid>\",  string) A hash of the master public key\n"
                            "  \"creationtime\" : The creation time in seconds since epoch (midnight Jan 1 1970 GMT).\n"
                            "  \"chainpath\" : \"<keyschainpath>\",  string) The chainpath (like m/44'/0'/0'/c)\n"
                            "}\n"
                            "\nExamples:\n"
                            + HelpExampleCli("hdgetinfo", "")
                            + HelpExampleCli("hdgetinfo", "True")
                            + HelpExampleRpc("hdgetinfo", "")
                            );

    LOCK2(cs_main, pwalletMain->cs_wallet);

    std::vector<HDChainID> chainIDs;
    if (!pwalletMain->GetAvailableChainIDs(chainIDs))
        throw runtime_error("Could not load chain ids");

    Array result;
    BOOST_FOREACH(const HDChainID& chainId, chainIDs)
    {
        CHDChain chain;
        if (!pwalletMain->GetChain(chainId, chain))
            throw runtime_error("Could not load chain");

        Object chainObject;
        chainObject.push_back(Pair("chainid", chainId.GetHex()));
        chainObject.push_back(Pair("creationtime", chain.nCreateTime));
        chainObject.push_back(Pair("chainpath", chain.chainPath));

        result.push_back(chainObject);
    }

    return result;
}

/* end BIP32 stack */
