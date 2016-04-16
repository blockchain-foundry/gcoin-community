// Copyright (c) 2014-2016 The Gcoin Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GCOIN_TEST_H
#define GCOIN_TEST_H

#include <boost/test/unit_test.hpp>
#include <boost/algorithm/string.hpp>

#include <stdint.h>
#include <map>
#include <string>

#include "main.h"
#include "wallet/wallet.h"
#include "cache.h"
#include "rpcserver.h"
#include "rpcclient.h"
#include "consensus/validation.h"
#include "validationinterface.h"
#include "base58.h"

extern std::map<uint256, CMutableTransaction> transactions;

extern type_transaction_handler::HandlerInterface *handler;

extern CWallet* pwalletMain;

extern alliance_member::AllianceMember *palliance;
extern color_license::ColorLicense *plicense;
extern activate_addr::ActivateAddr *pactivate;

bool GetTransaction_UnitTest(
        const uint256 &tx_hash, CTransaction &result,
        uint256 &block_hash, const CBlock *block, bool allow_slow);


bool GetCoinsFromCache_UnitTest(const COutPoint &outpoint,
        CCoins &coins,
        bool fUseMempool);

bool CheckTxFeeAndColor_UnitTest(const CTransaction tx);

void CreateTransaction(
        const uint256 &tx_hash, const tx_type &type);

void ConnectTransactions(const uint256 &src_hash,
        const uint256 &dst_hash,
        int64_t value,
        const std::string &address,
        const type_Color &color,
        const std::string &misc = "");

std::string CreateAddress();

CTxDestination CreateDestination();

json_spirit::Value CallRPC(std::string args);

struct GlobalSetupFixture
{
    GlobalSetupFixture()
    {
        AlternateFunc_GetTransaction = GetTransaction_UnitTest;
        AlternateFunc_GetCoinsFromCache = GetCoinsFromCache_UnitTest;
        AlternateFunc_CheckTxFeeAndColor = CheckTxFeeAndColor_UnitTest;
        transactions.clear();
    }

    ~GlobalSetupFixture()
    {
        AlternateFunc_GetTransaction = NULL;
        AlternateFunc_GetCoinsFromCache = NULL;
        AlternateFunc_CheckTxFeeAndColor = NULL;
        transactions.clear();
    }
};

/*!
 * @brief Cleans up and setups the environment.
 */
struct CacheSetupFixture
{
    CacheSetupFixture()
    {
        palliance = new alliance_member::AllianceMember();
        plicense = new color_license::ColorLicense();
        pminer = new block_miner::BlockMiner();
        pactivate = new activate_addr::ActivateAddr();
        porder = new order_list::OrderList();
    }

    ~CacheSetupFixture()
    {
        delete palliance;
        delete plicense;
        delete pminer;
        delete pactivate;
        delete porder;
    }
};

class CWallet_UnitTest : public CWallet
{
public:
    CWallet_UnitTest(std::string strWalletFileIn) :
            CWallet(strWalletFileIn), color_(0), type_(0), misc_(""),
            return_string_(""), color_admin_amount_(0)
    {
    }

    inline CTxDestination address() const
    {
        return address_;
    }

    inline type_Color color() const
    {
        return color_;
    }

    inline bool map_values_equal() const
    {
        return map_values_equal_;
    }

    inline void setColor(const type_Color color)
    {
        color_ = color;
    }

    inline void setType(int type)
    {
        type_ = type;
    }

    inline void setAddress(CTxDestination address)
    {
        address_ = address;
    }

    inline void setMisc(std::string misc)
    {
        misc_ = misc;
    }

    inline void setExpectedMap(mapValue_t expected_map_values)
    {
        expected_map_values_ = expected_map_values;
    }

    inline void setReturn(std::string return_string)
    {
        return_string_ = return_string;
    }

    inline void setColorAdmin(int64_t color_admin_amount)
    {
        color_admin_amount_ = color_admin_amount;
    }

    inline void setLicense(int64_t license_amount)
    {
        license_amount_ = license_amount;
    }

    DBErrors LoadWallet(bool& fFirstRunRet);

    int64_t GetColor0Balance() const
    {
        return color_admin_amount_;
    }

    int64_t GetSendLicenseBalance(const type_Color& color) const
    {
        return license_amount_;
    }

    bool CreateTypeTransaction(const std::vector<CRecipient>& vecSend, const type_Color &send_color, int type,
            CWalletTx &wtxNew, std::string &strFailReason, const std::string &misc = "")
    {
        return (send_color == color_ && type == type_ && misc == misc_);
    }

    inline bool CommitTransaction(CWalletTx &wtxNew, CReserveKey &reservekey)
    {
        return true;
    }

private:
    bool CheckMapValueExpected_(const mapValue_t &tx_map_value);

    type_Color color_;
    int type_;
    CTxDestination address_;
    std::string misc_;
    mapValue_t expected_map_values_;
    std::string return_string_;
    int64_t color_admin_amount_, license_amount_;
    bool map_values_equal_;
};

struct WalletSetupFixture
{
    WalletSetupFixture()
    {
        pwalletOld = pwalletMain;
        pwalletTest = new CWallet_UnitTest("wallet.dat");
        pwalletMain = pwalletTest;
    }

    ~WalletSetupFixture()
    {
        delete pwalletTest;
        pwalletMain = pwalletOld;
    }

    mapValue_t expected_map_values;
    json_spirit::Array params;

    CWallet *pwalletOld;
    CWallet_UnitTest *pwalletTest;
};
#endif
