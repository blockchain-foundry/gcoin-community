// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.h"

#include "assert.h"
#include "random.h"
#include "util.h"

#include <boost/assign/list_of.hpp>
#include <boost/thread.hpp>


struct SeedSpec6 {
    uint8_t addr[16];
    uint16_t port;
};

#include "chainparamsseeds.h"

//
// Main network
//

// Convert the pnSeeds6 array into usable address objects.
static void convertSeed6(std::vector<CAddress> &vSeedsOut, const SeedSpec6 *data, unsigned int count)
{
    // It'll only connect to one or two seed nodes because once it connects,
    // it'll get a pile of addresses with newer timestamps.
    // Seed nodes are given a random 'last seen time' of between one and two
    // weeks ago.
    const int64_t nOneWeek = 7 * 24 * 60 * 60;
    for (unsigned int i = 0; i < count; i++) {
        struct in6_addr ip;
        memcpy(&ip, data[i].addr, sizeof(ip));
        CAddress addr(CService(ip, data[i].port));
        addr.nTime = GetTime() - GetRand(nOneWeek) - nOneWeek;
        vSeedsOut.push_back(addr);
    }
}

unsigned int pnGCoinSeed[] = {
};

uint32_t GetGenesisNonce(CBlockHeader *genesisH)
{
    uint32_t nNonce = 0;
    uint256 hashTarget = uint256().SetCompact(genesisH->nBits);
    uint256 hash;
    //uint256* hash = &hasht;
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
                //return nNonce;
                std::stringstream ss;
                ss << nNonce;
                std::string s;
                ss >> s;
                throw std::runtime_error(s);
            }
        }
    }

}

class CMainParams : public CChainParams
{
public:
    CMainParams()
    {
        networkID = CBaseChainParams::MAIN;
        strNetworkID = "main";
        // The message start string is designed to be unlikely to occur in normal data.
        // The characters are rarely used upper ASCII, not valid as UTF-8, and produce
        // a large 4-byte int at any alignment.
        pchMessageStart[0] = 0xf9;
        pchMessageStart[1] = 0xbe;
        pchMessageStart[2] = 0xb4;
        pchMessageStart[3] = 0xd9;
        vAlertPubKey = ParseHex("04fc9702847840aaf195de8442ebecedf5b095cdbb9bc716bda9110971b28a49e0ead8564ff0db22209e0374782c093bb899692d524e9d6a6956e7c5ecbcd68284");
        nDefaultPort = 8333;
        bnProofOfWorkLimit = ~uint256(0) >> 32;
        nSubsidyHalvingInterval = 1;
        nEnforceBlockUpgradeMajority = 750;
        nRejectBlockOutdatedMajority = 950;
        nToCheckBlockUpgradeMajority = 1000;
        nMinerThreads = 0;
        nTargetTimespan = 14 * 24 * 60 * 60; // two weeks
        nTargetSpacing = 10 * 60;

        // Build the genesis block. Note that the output of the genesis coinbase cannot
        // be spent as it did not originally exist in the database.
        //
        // CBlock(hash=000000000019d6, ver=1, hashPrevBlock=00000000000000, hashMerkleRoot=4a5e1e, nTime=1231006505, nBits=1d00ffff, nNonce=2083236893, vtx=1)
        //   CTransaction(hash=4a5e1e, ver=1, vin.size=1, vout.size=1, nLockTime=0)
        //     CTxIn(COutPoint(000000, -1), coinbase 04ffff001d0104455468652054696d65732030332f4a616e2f32303039204368616e63656c6c6f72206f6e206272696e6b206f66207365636f6e64206261696c6f757420666f722062616e6b73)
        //     CTxOut(nValue=50.00000000, scriptPubKey=0x5F1DF16B2B704C8A578D0B)
        //   vMerkleTree: 4a5e1e
        const char* pszTimestamp = "The Times 03/Jan/2009 Chancellor on brink of second bailout for banks";
        CMutableTransaction txNew;
        txNew.vin.resize(1);
        txNew.vout.resize(1);
        txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(4) << std::vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
        txNew.vout[0].nValue = 50 * COIN;
        txNew.vout[0].scriptPubKey = CScript() << ParseHex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f") << OP_CHECKSIG;
        genesis.vtx.push_back(txNew);
        genesis.hashPrevBlock = 0;
        genesis.hashMerkleRoot = genesis.BuildMerkleTree();
        genesis.nVersion = 1;
        genesis.nTime    = 1231006505;
        genesis.nBits    = 0x1e0ffff0;//0x1d00ffff;
        genesis.nNonce = 57116; //GetGenesisNonce((CBlockHeader *) &genesis);

        hashGenesisBlock = genesis.GetHash();

//        assert(hashGenesisBlock == uint256("0x000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f"));
//        assert(genesis.hashMerkleRoot == uint256("0x4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b"));

        vSeeds.push_back(CDNSSeedData("bitcoin.sipa.be", "seed.bitcoin.sipa.be"));
        vSeeds.push_back(CDNSSeedData("bluematt.me", "dnsseed.bluematt.me"));
        vSeeds.push_back(CDNSSeedData("dashjr.org", "dnsseed.bitcoin.dashjr.org"));
        vSeeds.push_back(CDNSSeedData("bitcoinstats.com", "seed.bitcoinstats.com"));
        vSeeds.push_back(CDNSSeedData("bitnodes.io", "seed.bitnodes.io"));
        vSeeds.push_back(CDNSSeedData("open-nodes.org", "seeds.bitcoin.open-nodes.org"));
        vSeeds.push_back(CDNSSeedData("xf2.org", "bitseed.xf2.org"));

        base58Prefixes[PUBKEY_ADDRESS] = boost::assign::list_of(0);
        base58Prefixes[SCRIPT_ADDRESS] = boost::assign::list_of(5);
        base58Prefixes[SECRET_KEY] =     boost::assign::list_of(128);
        base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0x04)(0x88)(0xB2)(0x1E);
        base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0x04)(0x88)(0xAD)(0xE4);

        convertSeed6(vFixedSeeds, pnSeed6_main, ARRAYLEN(pnSeed6_main));

        fRequireRPCPassword = true;
        fMiningRequiresPeers = true;
        fDefaultCheckMemPool = false;
        fAllowMinDifficultyBlocks = false;
        fRequireStandard = true;
        fMineBlocksOnDemand = false;
    }
};
static CMainParams mainParams;

//
// Testnet (v3)
//

class CTestNetParams : public CMainParams
{
public:
    CTestNetParams()
    {
        networkID = CBaseChainParams::TESTNET;
        strNetworkID = "test";
        // The message start string is designed to be unlikely to occur in normal data.
        // The characters are rarely used upper ASCII, not valid as UTF-8, and produce
        // a large 4-byte int at any alignment.
        pchMessageStart[0] = 0x0b;
        pchMessageStart[1] = 0x11;
        pchMessageStart[2] = 0x09;
        pchMessageStart[3] = 0x07;
        vAlertPubKey = ParseHex("04302390343f91cc401d56d68b123028bf52e5fca1939df127f63c6467cdf9c8e2c14b61104cf817d0b780da337893ecc4aaff1309e536162dabbdb45200ca2b0a");
        nDefaultPort = 18333;
        nEnforceBlockUpgradeMajority = 51;
        nRejectBlockOutdatedMajority = 75;
        nToCheckBlockUpgradeMajority = 100;
        nMinerThreads = 0;
        nTargetTimespan = 14 * 24 * 60 * 60; // two weeks
        nTargetSpacing = 10 * 60;

        // Modify the testnet genesis block so the timestamp is valid for a later start.
        genesis.nTime = 1296688602;
        genesis.nNonce = 206278; //GetGenesisNonce((CBlockHeader *) &genesis);
        hashGenesisBlock = genesis.GetHash();
        // assert(hashGenesisBlock == uint256("0x000000000933ea01ad0ee984209779baaec3ced90fa3f408719526f8d77f4943"));



        vFixedSeeds.clear();
        vSeeds.clear();
        vSeeds.push_back(CDNSSeedData("alexykot.me", "testnet-seed.alexykot.me"));
        vSeeds.push_back(CDNSSeedData("bitcoin.petertodd.org", "testnet-seed.bitcoin.petertodd.org"));
        vSeeds.push_back(CDNSSeedData("bluematt.me", "testnet-seed.bluematt.me"));

        base58Prefixes[PUBKEY_ADDRESS] = boost::assign::list_of(111);
        base58Prefixes[SCRIPT_ADDRESS] = boost::assign::list_of(196);
        base58Prefixes[SECRET_KEY]     = boost::assign::list_of(239);
        base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0x04)(0x35)(0x87)(0xCF);
        base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0x04)(0x35)(0x83)(0x94);

        convertSeed6(vFixedSeeds, pnSeed6_test, ARRAYLEN(pnSeed6_test));

        fRequireRPCPassword = true;
        fMiningRequiresPeers = true;
        fDefaultCheckMemPool = false;
        fAllowMinDifficultyBlocks = true;
        fRequireStandard = false;
        fMineBlocksOnDemand = false;
    }
};
static CTestNetParams testNetParams;

//
// Regression test
//
class CRegTestParams : public CTestNetParams
{
public:
    CRegTestParams()
    {
        networkID = CBaseChainParams::REGTEST;
        strNetworkID = "regtest";
        pchMessageStart[0] = 0xfa;
        pchMessageStart[1] = 0xbf;
        pchMessageStart[2] = 0xb5;
        pchMessageStart[3] = 0xda;
        nSubsidyHalvingInterval = 1;
        nEnforceBlockUpgradeMajority = 750;
        nRejectBlockOutdatedMajority = 950;
        nToCheckBlockUpgradeMajority = 1000;
        nMinerThreads = 1;
        nTargetTimespan = 14 * 24 * 60 * 60; // two weeks
        nTargetSpacing = 10 * 60;
        bnProofOfWorkLimit = ~uint256(0) >> 1;
        genesis.nTime = 1296688602;
        genesis.nBits = 0x207fffff;
        genesis.nNonce = 27617; //GetGenesisNonce((CBlockHeader *) &genesis);
        hashGenesisBlock = genesis.GetHash();
        nDefaultPort = 18444;
        //assert(hashGenesisBlock == uint256("0x0f9188f13cb7b2c71f2a335e3a4fc328bf5beb436012afca590b1a11466e2206"));

        vSeeds.clear();  // Regtest mode doesn't have any DNS seeds.

        fRequireRPCPassword = false;
        fMiningRequiresPeers = false;
        fDefaultCheckMemPool = true;
        fAllowMinDifficultyBlocks = true;
        fRequireStandard = false;
        fMineBlocksOnDemand = true;
    }
};
static CRegTestParams regTestParams;

class CGCoinParams : public CChainParams
{
public:
    CGCoinParams()
    {
        networkID = CBaseChainParams::GCOIN;
        strNetworkID = "gcoin";
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
        bnProofOfWorkLimit = ~uint256(0) >> 20;
        nSubsidyHalvingInterval = 1;      // GCoin30: mining reward is cut in half every 420,000 blocks,
        nEnforceBlockUpgradeMajority = 750;
        nRejectBlockOutdatedMajority = 950;
        nToCheckBlockUpgradeMajority = 1000;
        nMinerThreads = 0;
        nTargetTimespan = 60 * 60; // GCOIN: 1 hour
        nTargetSpacing = 15;            // GCoin30: 15 sec/blocks
        // After nTargetTimespan / nTargetSpacing blocks, the difficulty changes
        // 7 * 24 * 60 * 60 / (5 * 60) = 2520 blocks

        const char* pszTimestamp = "OpenNet GCoin Project 2014.9 GCoin";
        CMutableTransaction txNew;
        txNew.vin.resize(1);
        txNew.vout.resize(1);
        txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(4) << std::vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
        txNew.vout[0].nValue = 0 * COIN;
        txNew.vout[0].scriptPubKey = CScript() << ParseHex("04a3a8584b519bb42f63defcdd1bec62e685d8204ebe83a02f80cae170c207934591a1e739bad2f5ed632844c636504d8587ecabaf0b3168afb4f613895fd1105a") << OP_CHECKSIG;
        genesis.vtx.push_back(txNew);
        genesis.hashPrevBlock = 0;
        genesis.hashMerkleRoot = genesis.BuildMerkleTree();
        genesis.nVersion = 1;
        genesis.nTime    = 1421909240;
        //0x1e0ffff0 : six 0s   /  0x1d00ffff : eight 0s (mining pool accept difficulty at least eight 0s)
        genesis.nBits    = 0x1e0ffff0;//0x1d00ffff;
        // 968862 : nonce of 0x1e0ffff0   /  3442184461 : nonce of 0x1d00ffff;
        genesis.nNonce   = 1792207;//GetGenesisNonce((CBlockHeader *) &genesis);
        hashGenesisBlock = genesis.GetHash();
        // genesis hash: 00000a7490f9c2e2a23c904d583143c3a6c0790eac612b84bf5e05d32e3cdfc9

        base58Prefixes[PUBKEY_ADDRESS] = boost::assign::list_of(0);
        base58Prefixes[SCRIPT_ADDRESS] = boost::assign::list_of(5);
        base58Prefixes[SECRET_KEY] =     boost::assign::list_of(128);
        base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0x04)(0x88)(0xB2)(0x1E);
        base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0x04)(0x88)(0xAD)(0xE4);

        // Convert the pnSeeds array into usable address objects.
        for (unsigned int i = 0; i < ARRAYLEN(pnGCoinSeed); i++) {
            // It'll only connect to one or two seed nodes because once it connects,
            // it'll get a pile of addresses with newer timestamps.
            // Seed nodes are given a random 'last seen time' of between one and two
            // weeks ago.
            const int64_t nOneWeek = 7*24*60*60;
            struct in_addr ip;
            memcpy(&ip, &pnGCoinSeed[i], sizeof(ip));
            CAddress addr(CService(ip, GetDefaultPort()));
            addr.nTime = GetTime() - GetRand(nOneWeek) - nOneWeek;
            vFixedSeeds.push_back(addr);
        }

        fRequireRPCPassword = true;
        fMiningRequiresPeers = false;
        fDefaultCheckMemPool = false;
        fAllowMinDifficultyBlocks = false;
        fRequireStandard = true;
        fMineBlocksOnDemand = false;
    }
};
static CGCoinParams gCoinParams;

static CChainParams *pCurrentParams = 0;

const CChainParams &Params()
{
    assert(pCurrentParams);
    return *pCurrentParams;
}

void SelectParams(CBaseChainParams::Network network)
{
    SelectBaseParams(network);
    switch (network) {
    case CBaseChainParams::MAIN:
        pCurrentParams = &mainParams;
        break;
    case CBaseChainParams::TESTNET:
        pCurrentParams = &testNetParams;
        break;
    case CBaseChainParams::REGTEST:
        pCurrentParams = &regTestParams;
        break;
    case CBaseChainParams::GCOIN:
        pCurrentParams = &gCoinParams;
        break;
    default:
        assert(false && "Unimplemented network");
        return;
    }
}

bool SelectParamsFromCommandLine()
{
    if (!SelectBaseParamsFromCommandLine())
        return false;

    SelectParams(BaseParams().NetworkID());
    return true;
}
