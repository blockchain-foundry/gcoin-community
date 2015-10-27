// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparamsbase.h"

#include "util.h"

#include <assert.h>

//
// Main network
//

class CBaseMainParams : public CBaseChainParams
{
public:
    CBaseMainParams()
    {
        networkID = CBaseChainParams::MAIN;
        nRPCPort = 8332;
    }
};
static CBaseMainParams mainParams;

//
// Testnet (v3)
//
class CBaseTestNetParams : public CBaseMainParams
{
public:
    CBaseTestNetParams()
    {
        networkID = CBaseChainParams::TESTNET;
        nRPCPort = 18332;
        strDataDir = "testnet3";
    }
};
static CBaseTestNetParams testNetParams;

//
// Regression test
//
class CBaseRegTestParams : public CBaseTestNetParams
{
public:
    CBaseRegTestParams()
    {
        networkID = CBaseChainParams::REGTEST;
        strDataDir = "regtest";
    }
};
static CBaseRegTestParams regTestParams;

//
// GCoin 5 min/tx
//
class CBaseGCoinParams : public CBaseMainParams
{
  public:
    CBaseGCoinParams()
    {
      networkID = CBaseChainParams::GCOIN;
      nRPCPort = 26957;
      strDataDir = "gcoin";
    }
};
static CBaseGCoinParams gCoinParams;





static CBaseChainParams *pCurrentBaseParams = 0;

const CBaseChainParams &BaseParams()
{
    assert(pCurrentBaseParams);
    return *pCurrentBaseParams;
}

void SelectBaseParams(CBaseChainParams::Network network)
{
    switch (network) {
    case CBaseChainParams::MAIN:
        pCurrentBaseParams = &mainParams;
        break;
    case CBaseChainParams::TESTNET:
        pCurrentBaseParams = &testNetParams;
        break;
    case CBaseChainParams::REGTEST:
        pCurrentBaseParams = &regTestParams;
        break;
    case CBaseChainParams::GCOIN:
        pCurrentBaseParams = &gCoinParams;
        break;
    default:
        assert(false && "Unimplemented network");
        return;
    }
}

bool SelectBaseParamsFromCommandLine()
{
    bool fRegTest = GetBoolArg("-regtest", false);
    bool fTestNet = GetBoolArg("-testnet", false);
    bool fGCoin = GetBoolArg("-gcoin", false);
    if ((fRegTest ? 1 : 0) + (fTestNet ? 1 : 0) + (fGCoin ? 1 : 0) > 1)
        return false;

    if (fRegTest)
        SelectBaseParams(CBaseChainParams::REGTEST);
    else if (fTestNet)
        SelectBaseParams(CBaseChainParams::TESTNET);
    else if (fGCoin)
        SelectBaseParams(CBaseChainParams::GCOIN);
    else
        SelectBaseParams(CBaseChainParams::MAIN);

    return true;
}

bool AreBaseParamsConfigured()
{
    return pCurrentBaseParams != NULL;
}
