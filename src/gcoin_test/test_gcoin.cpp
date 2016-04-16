// Copyright (c) 2014-2016 The Gcoin Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#define BOOST_TEST_MODULE Gcoin Test Suite

#include <boost/filesystem.hpp>
#include <boost/test/unit_test.hpp>

#include "chainparams.h"
#include "main.h"
#include "net.h"
#include "noui.h"
#include "random.h"
#include "util.h"
#ifdef ENABLE_WALLET
#include "db.h"
#include "wallet/wallet.h"
#include "init.h"
#endif

// 要顯示多少message請參考
// ./test_gcoin --help
// 和
// http://www.boost.org/doc/libs/1_45_0/libs/test/doc/html/utf/user-guide/runtime-config/reference.html


/*!
 * @brief Localizes the `TestingSetup` struct.
 */
namespace
{


/*!
 * @brief Global settings.
 */
struct TestingSetup
{
    TestingSetup()
    {
        fPrintToDebugLog = false;
        SelectParams(CBaseChainParams::GCOIN);

        ECC_Start();
        noui_connect();
#ifdef ENABLE_WALLET
        bitdb.MakeMock();
#endif
        RegisterNodeSignals(GetNodeSignals());

        pathTemp = GetTempPath() /
                   strprintf("test_gcoin_%lu_%i",
                             static_cast<unsigned long>(GetTime()),
                             static_cast<int>(GetRand(100000)));
        boost::filesystem::create_directories(pathTemp);
        mapArgs["-datadir"] = pathTemp.string();
#ifdef ENABLE_WALLET
        bool fFirstRun;
        pwalletMain = new CWallet("wallet.dat");
        pwalletMain->LoadWallet(fFirstRun);
        RegisterValidationInterface(pwalletMain);
#endif
    }

    ~TestingSetup()
    {
        UnregisterNodeSignals(GetNodeSignals());
        ECC_Stop();
#ifdef ENABLE_WALLET
        delete pwalletMain;
        pwalletMain = NULL;
        bitdb.Flush(true);
#endif
        boost::filesystem::remove_all(pathTemp);
    }

    boost::filesystem::path pathTemp;
};

}  // namespace

BOOST_GLOBAL_FIXTURE(TestingSetup);
