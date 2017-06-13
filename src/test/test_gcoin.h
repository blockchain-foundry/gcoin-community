#ifndef BITCOIN_TEST_TEST_BITCOIN_H
#define BITCOIN_TEST_TEST_BITCOIN_H

#include "txdb.h"

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/thread.hpp>
#include <boost/test/unit_test.hpp>

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
#include "test_gcoin.h"

extern std::map<uint256, CMutableTransaction> transactions;

extern type_transaction_handler::HandlerInterface *handler;

extern CWallet* pwalletMain;

extern alliance_member::AllianceMember *palliance;
extern color_license::ColorLicense *plicense;
extern block_miner::BlockMiner *pblkminer;
extern miner::Miner *pminer;

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
CPubKey GenerateNewKey();

CTxDestination CreateDestination();

json_spirit::Array createArgs(int nRequired, const char* address1=NULL, const char* address2=NULL);

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
        pblkminer = new block_miner::BlockMiner();
        pminer = new miner::Miner();
    }

    ~CacheSetupFixture()
    {
        delete palliance;
        delete plicense;
        delete pblkminer;
        delete pminer;
    }
};

class CWallet_UnitTest : public CWallet
{
public:
    CWallet_UnitTest(std::string strWalletFileIn) :
            CWallet(strWalletFileIn), color_(0),
            return_string_(""), color_admin_amount_(0), license_amount_(0)
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

    inline void setAddress(CTxDestination address)
    {
        address_ = address;
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

    inline void setColorAmount(int64_t color_amount)
    {
        color_amount_ = color_amount;
    }

    inline void setLicense(int64_t license_amount)
    {
        license_amount_ = license_amount;
    }

    inline void setPubKey(std::vector<CPubKey> vPubKey)
    {
        vPubKey_ = vPubKey;
    }

    DBErrors LoadWallet(bool& fFirstRunRet);

    int64_t GetColor0Balance() const
    {
        return color_admin_amount_;
    }

    int64_t GetColorBalanceFromFixedAddress(const std::string& strFromAddress, const type_Color& color) const
    {
        return color_amount_;
    }

    int64_t GetSendLicenseBalance(const type_Color& color) const
    {
        return license_amount_;
    }

    bool CreateTransaction(const std::vector<CRecipient>& vecSend, const type_Color& send_color,
        CWalletTx& wtxNew, CReserveKey& reservekey, CAmount& nFeeRet, int& nChangePosRet,
        std::string& strFailReason, const CCoinControl *coinControl = NULL,
        const std::vector<CPubKey>& vPubKey = std::vector<CPubKey>(),
        const std::string& strFromAddress = "", const std::string& feeFromAddress = "")
    {
        if (send_color != color_)
            return false;
        if (vPubKey.size() != vPubKey_.size())
            return false;
        for (unsigned int i = 0; i < vPubKey.size(); i++)
            if (vPubKey[i] != vPubKey_[i])
                return false;
        return true;
    }

    bool CreateLicenseTransaction(const std::vector<CRecipient>& vecSend, const type_Color& send_color, CWalletTx& wtxNew,
                                  string& strFailReason, bool &fComplete)
    {
        return (send_color == color_ );
    }

    inline bool CommitTransaction(CWalletTx &wtxNew, CReserveKey &reservekey)
    {
        return true;
    }

private:
    bool CheckMapValueExpected_(const mapValue_t &tx_map_value);

    type_Color color_;
    CTxDestination address_;
    mapValue_t expected_map_values_;
    std::string return_string_;
    int64_t color_admin_amount_, license_amount_, color_amount_;
    std::vector<CPubKey> vPubKey_;
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
        pwalletMain = pwalletOld;
    }

    mapValue_t expected_map_values;
    json_spirit::Array params;

    CWallet *pwalletOld;
    CWallet_UnitTest *pwalletTest;
};

class CCoinsViewCache_UnitTest : public CCoinsViewCache
{
public:
    CCoinsViewCache_UnitTest(CCoinsView *baseIn): CCoinsViewCache(baseIn), hash(ArithToUint256(arith_uint256(0))), txout()
    {
    }
    inline void setUTXO(const uint256& hashIn, unsigned int indexIn, CAmount amount, CScript scriptPubKeyIn, type_Color color)
    {
        hash = hashIn;
        index = indexIn;
        txout = CTxOut(amount, scriptPubKeyIn, color);
    }
    bool GetAddrCoins(const std::string &addr, CTxOutMap &mapTxOut, bool fLicense) const
    {
        mapTxOut.insert(pair<COutPoint, CTxOut> (COutPoint(hash, index), txout));
        return true;
    }
private:
    uint256 hash;
    unsigned int index;
    CTxOut txout;
};

struct CoinsSetupFixture
{
    CoinsSetupFixture()
    {
        pcoinsOld = pcoinsTip;
        CCoinsViewDB *pcoinsdb = new CCoinsViewDB(1 << 23, true);
        pcoinsTest = new CCoinsViewCache_UnitTest(pcoinsdb);
        pcoinsTip = pcoinsTest;
    }

    ~CoinsSetupFixture()
    {
        pcoinsTip = pcoinsOld;
    }

    CCoinsViewCache *pcoinsOld;
    CCoinsViewCache_UnitTest *pcoinsTest;
};

/** Basic testing setup.
 * This just configures logging and chain parameters.
 */
struct BasicTestingSetup {
    BasicTestingSetup();
    ~BasicTestingSetup();
};

/** Testing setup that configures a complete environment.
 * Included are data directory, coins database, script check threads
 * and wallet (if enabled) setup.
 */
struct TestingSetup: public BasicTestingSetup, public CacheSetupFixture, public GlobalSetupFixture{
    CCoinsViewDB *pcoinsdbview;
    boost::filesystem::path pathTemp;
    boost::thread_group threadGroup;

    TestingSetup();
    ~TestingSetup();
};

#endif
