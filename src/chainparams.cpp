// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Copyright (c) 2014-2016 The Gcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.h"

#include "arith_uint256.h"
#include "base58.h"
#include "hash.h"
#include "random.h"
#include "streams.h"
#include "util.h"
#include "utilstrencodings.h"

#include <assert.h>

#include <boost/assign/list_of.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/thread.hpp>

using namespace std;

#include "chainparamsseeds.h"

/**
 * Main network
 */
/**
 * What makes a good checkpoint block?
 * + Is surrounded by blocks with reasonable timestamps
 *   (no blocks before with a timestamp after, none after with
 *    timestamp before)
 * + Contains no strange transactions
 */

uint32_t GetGenesisNonce(CBlockHeader *genesisH)
{
    uint32_t nNonce = 0;
    arith_uint256 hashTarget = arith_uint256().SetCompact(genesisH->nBits);
    arith_uint256 hash;
    //arith_uint256* hash = &hasht;
    while (true) {
        bool fFound = false;//ScanHash(genesis, nNonce, &hash);
        // Write the first 76 bytes of the block header to a double-SHA256 state.
        CHash256 hasher;
        CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
        ss << *genesisH;
        assert(ss.size() == 80);
        hasher.Write((unsigned char*)&ss[0], 76);

        while (true) {
            nNonce++;

            // Write the last 4 bytes of the block header (the nonce) to a copy of
            // the double-SHA256 state, and compute the result.
            CHash256(hasher).Write((unsigned char*)&nNonce, 4).Finalize((unsigned char*)&hash);

            // Return the nonce if the hash has at least some zero bits,
            // caller will check if it has enough to reach the target
            if (((uint16_t*)&hash)[15] == 0) {
                fFound = true;
                break;
            }
            // If nothing found after trying for a while, return -1
            if ((nNonce & 0xffff) == 0) {
                fFound = false;
                break;
            }
            if ((nNonce & 0xfff) == 0)
                boost::this_thread::interruption_point();
        }

        if (fFound) {
            if (hash <= hashTarget) {
                return nNonce;
            }
        }
    }
}

class CMainParams : public CChainParams
{
public:
    CMainParams()
    {
        strNetworkID = "main";
        consensus.nSubsidyHalvingInterval = 1;
        consensus.nMajorityEnforceBlockUpgrade = 750;
        consensus.nMajorityRejectBlockOutdated = 950;
        consensus.nMajorityWindow = 1000;
        consensus.powLimit = ArithToUint256(arith_uint256().SetCompact(0x1e0ffff0)); //ArithToUint256(~arith_uint256(0) >> 20);
        consensus.nPowTargetTimespan = 60 * 60; // two weeks
        consensus.nPowTargetSpacing = 15;
        consensus.fPowAllowMinDifficultyBlocks = false;

        // The message start string is designed to be unlikely to occur in normal data.
        // The characters are rarely used upper ASCII, not valid as UTF-8, and produce
        // a large 4-byte int at any alignment.
        pchMessageStart[0] = 0xab;  //171,G:71
        pchMessageStart[1] = 0xa7;  //167,C:67
        pchMessageStart[2] = 0x97;  //151,3:51
        pchMessageStart[3] = 0x95;  //148,0:48

        // generate from website: http://kjur.github.io/jsrsasign/sample-ecdsa.html
        vAlertPubKey = ParseHex("046107198704dcb7519548b578656dc29462c6a5355de7fa86cc2146f7bab7788b52b9913b8a412877fc73bcb65a4e5cf4ac4ea0c23f59aceac9c25d9454c343bc");
        nDefaultPort = 55666;//26958;
        nMinerThreads = 0;
        // After nTargetTimespan / nTargetSpacing blocks, the difficulty changes
        // 7 * 24 * 60 * 60 / (5 * 60) = 2520 blocks
        nDynamicDiff = 2.0;   // The difficulty adjust parameter
        nAllianceThreshold = 0.66; 
        nDynamicMiner = 5;  // Number of miners in a row to be considered

        const char* pszTimestamp = "OpenNet GCoin Project 2014.9 GCoin";
        CMutableTransaction txNew;
        txNew.vin.resize(1);
        txNew.vout.resize(1);
        txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(4) << std::vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
        txNew.vout[0].nValue = 0 * COIN;
        txNew.vout[0].scriptPubKey = CScript() << ParseHex("04a3a8584b519bb42f63defcdd1bec62e685d8204ebe83a02f80cae170c207934591a1e739bad2f5ed632844c636504d8587ecabaf0b3168afb4f613895fd1105a") << OP_CHECKSIG;
        genesis.vtx.push_back(txNew);
        genesis.hashPrevBlock.SetNull();
        genesis.nVersion = 1;
        genesis.nTime    = 1421909240;
        //0x1e0ffff0 : six 0s   /  0x1d00ffff : eight 0s (mining pool accept difficulty at least eight 0s)
        genesis.nBits    = 0x1e0ffff0;//0x1d00ffff;
        UpdateGenesis();

        vFixedSeeds.clear();
        vSeeds.clear();

        base58Prefixes[PUBKEY_ADDRESS] = boost::assign::list_of(0);
        base58Prefixes[SCRIPT_ADDRESS] = boost::assign::list_of(5);
        base58Prefixes[SECRET_KEY] =     boost::assign::list_of(128);
        base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0x04)(0x88)(0xB2)(0x1E);
        base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0x04)(0x88)(0xAD)(0xE4);

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_gcoin, pnSeed6_gcoin + ARRAYLEN(pnSeed6_gcoin));

        fRequireRPCPassword = true;
        fMiningRequiresPeers = false;
        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        fMineBlocksOnDemand = false;
        fTestnetToBeDeprecatedFieldRPC = false;

        checkpointData = (Checkpoints::CCheckpointData) {
            boost::assign::map_list_of
            ( 0, uint256S("000001677dc00bfa1df90d3f6ea119b521f9bd66178a4e9d50f175526db983c6")),
            0,
            0,
            0
        };
    }

    void AddAlliance(const std::string &addr) {
        CMutableTransaction tx;
        tx.vin.resize(1);
        tx.vout.resize(1);
        tx.type = VOTE;
        const char* pszTimestamp = "OpenNet GCoin Project 2014.9 GCoin";
        tx.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(4) << std::vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
        tx.vout[0].color = 0 * COIN;
        tx.vout[0].nValue = COIN;
        tx.vout[0].scriptPubKey = GetScriptForDestination(CBitcoinAddress(addr).Get());
        genesis.vtx.push_back(tx);
    }

    void UpdateGenesis() {
        genesis.hashMerkleRoot = genesis.BuildMerkleTree();
        genesis.nNonce = GetGenesisNonce((CBlockHeader *) &genesis);
        consensus.hashGenesisBlock = genesis.GetHash();
    }
};
static CMainParams mainParams;

/**
 * Testnet (v3)
 */
class CTestNetParams : public CMainParams {
public:
    CTestNetParams() {
        strNetworkID = "test";
        consensus.nMajorityEnforceBlockUpgrade = 51;
        consensus.nMajorityRejectBlockOutdated = 75;
        consensus.nMajorityWindow = 100;
        consensus.fPowAllowMinDifficultyBlocks = true;
        pchMessageStart[0] = 0x0b;
        pchMessageStart[1] = 0x11;
        pchMessageStart[2] = 0x09;
        pchMessageStart[3] = 0x07;
        vAlertPubKey = ParseHex("04302390343f91cc401d56d68b123028bf52e5fca1939df127f63c6467cdf9c8e2c14b61104cf817d0b780da337893ecc4aaff1309e536162dabbdb45200ca2b0a");
        nDefaultPort = 18333;
        nMinerThreads = 0;
        nPruneAfterHeight = 1000;

        //! Modify the testnet genesis block so the timestamp is valid for a later start.
        genesis.nTime = 1296688602;
        genesis.nNonce = 414098458;
        consensus.hashGenesisBlock = genesis.GetHash();
        //assert(consensus.hashGenesisBlock == uint256S("0x000000000933ea01ad0ee984209779baaec3ced90fa3f408719526f8d77f4943"));

        vFixedSeeds.clear();
        vSeeds.clear();
        vSeeds.push_back(CDNSSeedData("alexykot.me", "testnet-seed.alexykot.me"));
        vSeeds.push_back(CDNSSeedData("bitcoin.petertodd.org", "testnet-seed.bitcoin.petertodd.org"));
        vSeeds.push_back(CDNSSeedData("bluematt.me", "testnet-seed.bluematt.me"));
        vSeeds.push_back(CDNSSeedData("bitcoin.schildbach.de", "testnet-seed.bitcoin.schildbach.de"));

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0x04)(0x35)(0x87)(0xCF).convert_to_container<std::vector<unsigned char> >();
        base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0x04)(0x35)(0x83)(0x94).convert_to_container<std::vector<unsigned char> >();

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_test, pnSeed6_test + ARRAYLEN(pnSeed6_test));

        fRequireRPCPassword = true;
        fMiningRequiresPeers = true;
        fDefaultConsistencyChecks = false;
        fRequireStandard = false;
        fMineBlocksOnDemand = false;
        fTestnetToBeDeprecatedFieldRPC = true;

        checkpointData = (Checkpoints::CCheckpointData) {
            boost::assign::map_list_of
            ( 546, uint256S("000000002a936ca763904c3c35fce2f3556c559c0214345d31b1bcebf76acb70")),
            1337966069,
            1488,
            300
        };

    }
};
static CTestNetParams testNetParams;

/**
 * Regression test
 */
class CRegTestParams : public CTestNetParams {
public:
    CRegTestParams() {
        strNetworkID = "regtest";
        consensus.nSubsidyHalvingInterval = 150;
        consensus.nMajorityEnforceBlockUpgrade = 750;
        consensus.nMajorityRejectBlockOutdated = 950;
        consensus.nMajorityWindow = 1000;
        consensus.powLimit = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        pchMessageStart[0] = 0xfa;
        pchMessageStart[1] = 0xbf;
        pchMessageStart[2] = 0xb5;
        pchMessageStart[3] = 0xda;
        nMinerThreads = 1;
        genesis.nTime = 1296688602;
        genesis.nBits = 0x207fffff;
        genesis.nNonce = 2;
        consensus.hashGenesisBlock = genesis.GetHash();
        nDefaultPort = 18444;
        //assert(consensus.hashGenesisBlock == uint256S("0x0f9188f13cb7b2c71f2a335e3a4fc328bf5beb436012afca590b1a11466e2206"));
        nPruneAfterHeight = 1000;

        vFixedSeeds.clear(); //! Regtest mode doesn't have any fixed seeds.
        vSeeds.clear();  //! Regtest mode doesn't have any DNS seeds.

        fRequireRPCPassword = false;
        fMiningRequiresPeers = false;
        fDefaultConsistencyChecks = true;
        fRequireStandard = false;
        fMineBlocksOnDemand = true;
        fTestnetToBeDeprecatedFieldRPC = false;

        checkpointData = (Checkpoints::CCheckpointData){
            boost::assign::map_list_of
            ( 0, uint256S("0f9188f13cb7b2c71f2a335e3a4fc328bf5beb436012afca590b1a11466e2206")),
            0,
            0,
            0
        };
    }
};
static CRegTestParams regTestParams;

static CChainParams *pCurrentParams = 0;

CChainParams &Params()
{
    assert(pCurrentParams);
    return *pCurrentParams;
}

CChainParams &Params(CBaseChainParams::Network network) {
    switch (network) {
        case CBaseChainParams::MAIN:
            return mainParams;
        case CBaseChainParams::TESTNET:
            return testNetParams;
        case CBaseChainParams::REGTEST:
            return regTestParams;
        default:
            assert(false && "Unimplemented network");
            return mainParams;
    }
}

void SelectParams(CBaseChainParams::Network network) {
    SelectBaseParams(network);
    pCurrentParams = &Params(network);
}

bool SelectParamsFromCommandLine()
{
    CBaseChainParams::Network network = NetworkIdFromCommandLine();
    if (network == CBaseChainParams::MAX_NETWORK_TYPES)
        return false;

    SelectParams(network);
    return true;
}
