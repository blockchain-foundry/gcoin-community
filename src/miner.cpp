// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Copyright (c) 2014-2016 The Gcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "miner.h"

#include "base58.h"
#include "amount.h"
#include "cache.h"
#include "chainparams.h"
#include "consensus/consensus.h"
#include "consensus/validation.h"
#include "hash.h"
#include "script/sign.h"
#include "main.h"
#include "net.h"
#include "pow.h"
#include "primitives/transaction.h"
#include "timedata.h"
#include "util.h"
#include "utilmoneystr.h"
#include "timedata.h"
#ifdef ENABLE_WALLET
#include "wallet/wallet.h"
#endif

#include <boost/thread.hpp>
#include <boost/tuple/tuple.hpp>

using namespace std;

#include "utilerror.h"

using std::vector;
using std::map;
using std::list;
using std::set;
using std::auto_ptr;
//////////////////////////////////////////////////////////////////////////////
//
// GcoinMiner
//

//
// Unconfirmed transactions in the memory pool often depend on other
// transactions in the memory pool. When we select transactions from the
// pool, we select by highest priority or fee rate, so we might consider
// transactions that depend on transactions that aren't yet in the block.
// The COrphan class keeps track of these 'temporary orphans' while
// CreateBlock is figuring out which transactions to include.
//
class COrphan
{
public:
    const CTransaction* ptx;
    set<uint256> setDependsOn;
    CFeeRate feeRate;
    double dPriority;

    COrphan(const CTransaction* ptxIn) : ptx(ptxIn), feeRate(0), dPriority(0)
    {
    }
};

uint64_t nLastBlockTx = 0;
uint64_t nLastBlockSize = 0;

// We want to sort transactions by priority and fee rate, so:
typedef boost::tuple<double, CFeeRate, const CTransaction*> TxPriority;
class TxPriorityCompare
{
    bool byFee;

public:
    TxPriorityCompare(bool _byFee) : byFee(_byFee) { }

    bool operator()(const TxPriority& a, const TxPriority& b)
    {
        if (byFee)
        {
            if (a.get<1>() == b.get<1>())
                return a.get<0>() < b.get<0>();
            return a.get<1>() < b.get<1>();
        }
        else
        {
            if (a.get<0>() == b.get<0>())
                return a.get<1>() < b.get<1>();
            return a.get<0>() < b.get<0>();
        }
    }
};

void UpdateTime(CBlock* pblock, const Consensus::Params& consensusParams, const CBlockIndex* pindexPrev)
{
    pblock->nTime = std::max(std::max(pindexPrev->GetMedianTimePast()+1, GetAdjustedTime()), pblock->GetBlockStartTime() + 1);

    // Updating time can change work required on testnet:
    if (consensusParams.fPowAllowMinDifficultyBlocks)
        pblock->nBits = GetNextWorkRequired(pindexPrev, pblock, consensusParams);
}

CBlockTemplate* CreateNewBlock(const CScript& scriptPubKeyIn, CWallet *pwallet, bool fMiningPool, uint32_t nStartTime)
{
    // Create new block
    auto_ptr<CBlockTemplate> pblocktemplate(new CBlockTemplate());
    if (!pblocktemplate.get())
        return NULL;
    CBlock *pblock = &pblocktemplate->block; // pointer for convenience

    // -regtest only: allow overriding block.nVersion with
    // -blockversion=N to test forking scenarios
    if (Params().MineBlocksOnDemand())
        pblock->nVersion = GetArg("-blockversion", pblock->nVersion);

    // Create coinbase tx
    CMutableTransaction txNew;
    txNew.vin.resize(1);
    txNew.vin[0].prevout.SetNull();
    txNew.vout.resize(1);
    txNew.nLockTime = nStartTime == 0? GetAdjustedTime(): nStartTime;

    // Add dummy coinbase tx as first transaction
    pblock->vtx.push_back(CTransaction());
    pblocktemplate->vTxFees.push_back(-1); // updated at end
    pblocktemplate->vTxSigOps.push_back(-1); // updated at end

    // Largest block you're willing to create:
    unsigned int nBlockMaxSize = GetArg("-blockmaxsize", DEFAULT_BLOCK_MAX_SIZE);
    // Limit to betweeen 1K and MAX_BLOCK_SIZE-1K for sanity:
    nBlockMaxSize = std::max((unsigned int)1000, std::min((unsigned int)(MAX_BLOCK_SIZE-1000), nBlockMaxSize));

    // How much of the block should be dedicated to high-priority transactions,
    // included regardless of the fees they pay
    unsigned int nBlockPrioritySize = GetArg("-blockprioritysize", DEFAULT_BLOCK_PRIORITY_SIZE);
    nBlockPrioritySize = std::min(nBlockMaxSize, nBlockPrioritySize);

    // Minimum block size you want to create; block will be filled with free transactions
    // until there are no more or the block reaches this size:
    unsigned int nBlockMinSize = GetArg("-blockminsize", DEFAULT_BLOCK_MIN_SIZE);
    nBlockMinSize = std::min(nBlockMaxSize, nBlockMinSize);

    // Collect memory pool transactions into the block
    {
        LOCK2(cs_main, mempool.cs);
        LogPrintf("CreateNewBlock() : pool size = %d\n", mempool.mapTx.size());
        CBlockIndex* pindexPrev = chainActive.Tip();
        const int nHeight = pindexPrev->nHeight + 1;
        pblock->nTime = GetAdjustedTime();
        CCoinsViewCache view(pcoinsTip);

        // Priority order to process transactions
        list<COrphan> vOrphan; // list memory doesn't move
        map<uint256, vector<COrphan*> > mapDependers;
        bool fPrintPriority = GetBoolArg("-printpriority", false);

        // This vector will be sorted into a priority queue:
        vector<TxPriority> vecPriority;
        vecPriority.reserve(mempool.mapTx.size());
        for (map<uint256, CTxMemPoolEntry>::iterator mi = mempool.mapTx.begin();
             mi != mempool.mapTx.end(); ++mi) {
            const CTransaction& tx = mi->second.GetTx();
            if (!IsFinalTx(tx, nHeight, pblock->nTime))
                continue;

            COrphan* porphan = NULL;
            double dPriority = 0;
            CAmount nTotalIn = 0;
            bool fMissingInputs = false;

            // Coinbase tx dont need to check this
            if (!tx.IsCoinBase()) {
                BOOST_FOREACH(const CTxIn& txin, tx.vin)
                {
                    // Read prev transaction
                    if (!view.HaveCoins(txin.prevout.hash))
                    {
                        // This should never happen; all transactions in the memory
                        // pool should connect to either transactions in the chain
                        // or other transactions in the memory pool.
                        if (!mempool.mapTx.count(txin.prevout.hash))
                        {
                            LogPrintf("ERROR: mempool transaction missing input\n");
                            if (fDebug) assert("mempool transaction missing input" == 0);
                            fMissingInputs = true;
                            if (porphan)
                                vOrphan.pop_back();
                            break;
                        }

                        // Has to wait for dependencies
                        if (!porphan)
                        {
                            // Use list for automatic deletion
                            vOrphan.push_back(COrphan(&tx));
                            porphan = &vOrphan.back();
                        }
                        mapDependers[txin.prevout.hash].push_back(porphan);
                        porphan->setDependsOn.insert(txin.prevout.hash);
                        nTotalIn += mempool.mapTx[txin.prevout.hash].GetTx().vout[txin.prevout.n].nValue;
                        continue;
                    }

                    const CCoins* coins = view.AccessCoins(txin.prevout.hash);
                    assert(coins);

                    CAmount nValueIn = coins->vout[txin.prevout.n].nValue;
                    nTotalIn += nValueIn;

                    int nConf = nHeight - coins->nHeight;

                    dPriority += (double)nValueIn * nConf;
                }
            }

            if (fMissingInputs) continue;
            // Priority is sum(valuein * age) / modified_txsize
            unsigned int nTxSize = ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);
            dPriority = tx.ComputePriority(dPriority, nTxSize);

            uint256 hash = tx.GetHash();
            mempool.ApplyDeltas(hash, dPriority, nTotalIn);

            CFeeRate feeRate(nTotalIn-tx.GetValueOut(), nTxSize);

            if (porphan)
            {
                porphan->dPriority = dPriority;
                porphan->feeRate = feeRate;
            }
            else
                vecPriority.push_back(TxPriority(dPriority, feeRate, &mi->second.GetTx()));
        }

        // Collect transactions into block
        uint64_t nBlockSize = 1000;
        uint64_t nBlockTx = 0;
        int nBlockSigOps = 100;
        bool fSortedByFee = (nBlockPrioritySize <= 0);

        TxPriorityCompare comparer(fSortedByFee);
        std::make_heap(vecPriority.begin(), vecPriority.end(), comparer);
        unsigned int cnt = 0;

        while (!vecPriority.empty()) {
            // Take highest priority transaction off the priority queue:
            double dPriority = vecPriority.front().get<0>();
            CFeeRate feeRate = vecPriority.front().get<1>();
            const CTransaction& tx = *(vecPriority.front().get<2>());

            //!@# kill that tx off the vector
            std::pop_heap(vecPriority.begin(), vecPriority.end(), comparer);
            vecPriority.pop_back();

            // Size limits
            unsigned int nTxSize = ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);
            if (nBlockSize + nTxSize >= nBlockMaxSize)
                continue;

            // Legacy limits on sigOps:
            unsigned int nTxSigOps = GetLegacySigOpCount(tx);
            if (nBlockSigOps + nTxSigOps >= MAX_BLOCK_SIGOPS)
                continue;

            // Skip free transactions if we're past the minimum block size:
            const uint256& hash = tx.GetHash();
            double dPriorityDelta = 0;
            CAmount nFeeDelta = 0;
            mempool.ApplyDeltas(hash, dPriorityDelta, nFeeDelta);
            if (fSortedByFee && (dPriorityDelta <= 0) && (nFeeDelta <= 0) && (feeRate < ::minRelayTxFee) && (nBlockSize + nTxSize >= nBlockMinSize))
                continue;

            // Prioritise by fee once past the priority size or we run out of high-priority
            // transactions:
            if (!view.HaveInputs(tx))
                continue;

            nTxSigOps += GetP2SHSigOpCount(tx, view);
            if (nBlockSigOps + nTxSigOps >= MAX_BLOCK_SIGOPS)
                continue;

            // Note that flags: we don't want to set mempool/IsStandard()
            // policy here, but we still have to ensure that the block we
            // create only contains transactions that are valid in new blocks.
            CValidationState state;
            if (!CheckInputs(tx, state, view, true, MANDATORY_SCRIPT_VERIFY_FLAGS, true))
                continue;

            UpdateCoins(tx, state, view, nHeight);

            // Added
            pblock->vtx.push_back(tx);
            pblocktemplate->vTxFees.push_back(0);
            pblocktemplate->vTxSigOps.push_back(nTxSigOps);
            nBlockSize += nTxSize;
            ++nBlockTx;
            nBlockSigOps += nTxSigOps;

            if (tx.type == NORMAL)
                cnt++;

            if (fPrintPriority)
            {
                LogPrintf("priority %.1f fee %s txid %s\n",
                    dPriority, feeRate.ToString(), tx.GetHash().ToString());
            }

            // Add transactions that depend on this one to the priority queue
            if (mapDependers.count(hash))
            {
                BOOST_FOREACH(COrphan* porphan, mapDependers[hash])
                {
                    if (!porphan->setDependsOn.empty())
                    {
                        porphan->setDependsOn.erase(hash);
                        if (porphan->setDependsOn.empty())
                        {
                            vecPriority.push_back(TxPriority(porphan->dPriority, porphan->feeRate, porphan->ptx));
                            std::push_heap(vecPriority.begin(), vecPriority.end(), comparer);
                        }
                    }
                }
            }
        }

        // Coinbase transaction.
        txNew.vout[0].color = 0;
        txNew.vout[0].scriptPubKey = scriptPubKeyIn;
        txNew.vout[0].nValue = 0;
        if (cnt > 0) {
            CTxOut txout;
            TxFee.SetOutputForFee(txout, scriptPubKeyIn, cnt);
            txNew.vout.push_back(txout);
        }
        txNew.vin[0].scriptSig = CScript() << OP_0 << OP_0;
        if (pwallet != NULL) {
            unsigned int ntxNewSize = ::GetSerializeSize(txNew, SER_NETWORK, PROTOCOL_VERSION);
            nBlockTx += ntxNewSize;
            if (!SignSignature(*pwallet, scriptPubKeyIn, txNew, 0))
                throw std::runtime_error("Signing transaction failed at mining reward transaction");
        }

        pblock->vtx[0] = txNew;
        pblocktemplate->vTxFees[0] = -txNew.vout[0].nValue;

        nLastBlockTx = nBlockTx;
        nLastBlockSize = nBlockSize;
        LogPrintf("%s(): total size %u  MAX : %u\n", __func__, nBlockSize,nBlockMaxSize);

        // Fill in header
        pblock->hashPrevBlock  = pindexPrev->GetBlockHash();
        UpdateTime(pblock, Params().GetConsensus(), pindexPrev);
        pblock->nBits          = GetNextWorkRequired(pindexPrev, pblock, Params().GetConsensus());
        pblock->nNonce         = 0;
        pblocktemplate->vTxSigOps[0] = GetLegacySigOpCount(pblock->vtx[0]);

        CValidationState state;
        if (!TestBlockValidity(state, *pblock, pindexPrev, false, false))
            throw std::runtime_error("CreateNewBlock(): TestBlockValidity failed");
    }

    return pblocktemplate.release();
}

CBlockTemplate* CreateNewBlock(CWallet *pwallet, CPubKey pubkey, uint32_t& nStartTime)
{
    CScript scriptPubKeyIn = CScript() << ToByteVector(pubkey) << OP_CHECKSIG;
    return CreateNewBlock(scriptPubKeyIn, pwallet, nStartTime);
}

void IncrementExtraNonce(CBlock* pblock, CBlockIndex* pindexPrev, unsigned int& nExtraNonce)
{
    // Update nExtraNonce
    static uint256 hashPrevBlock;
    if (hashPrevBlock != pblock->hashPrevBlock) {
        nExtraNonce = 0;
        hashPrevBlock = pblock->hashPrevBlock;
    }
    ++nExtraNonce;

    // Coinbase tx is signed already.
    // unsigned int nHeight = pindexPrev->nHeight+1; // Height first in coinbase required for block.version=2
    CMutableTransaction txCoinbase(pblock->vtx[0]);
    //txCoinbase.vin[0].scriptSig = (CScript() << nHeight << CScriptNum(nExtraNonce)) + COINBASE_FLAGS;
    assert(txCoinbase.vin[0].scriptSig.size() <= 100);

    pblock->vtx[0] = txCoinbase;
    pblock->hashMerkleRoot = pblock->BuildMerkleTree();
}


#ifdef ENABLE_WALLET
//////////////////////////////////////////////////////////////////////////////
//
// Internal miner
//

//
// ScanHash scans nonces looking for a hash with at least some zero bits.
// The nonce is usually preserved between calls, but periodically or if the
// nonce is 0xffff0000 or above, the block is rebuilt and nNonce starts over at
// zero.
//
bool static ScanHash(const CBlockHeader *pblock, uint32_t& nNonce, uint256 *phash)
{
    // Write the first 76 bytes of the block header to a double-SHA256 state.
    CHash256 hasher;
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << *pblock;
    assert(ss.size() == 80);
    hasher.Write((unsigned char*)&ss[0], 76);

    while (true) {
        nNonce++;

        // Write the last 4 bytes of the block header (the nonce) to a copy of
        // the double-SHA256 state, and compute the result.
        CHash256(hasher).Write((unsigned char*)&nNonce, 4).Finalize((unsigned char*)phash);

        // Return the nonce if the hash has at least some zero bits,
        // caller will check if it has enough to reach the target
        if (((uint16_t*)phash)[15] == 0)
            return true;

        // If nothing found after trying for a while, return -1
        if ((nNonce & 0xfff) == 0)
            return false;
    }
}

CBlockTemplate* CreateNewBlockWithKey(CReserveKey& reservekey)
{
    CPubKey pubkey;
    if (!reservekey.GetReservedKey(pubkey))
        return NULL;

    CScript scriptPubKey = CScript() << ToByteVector(pubkey) << OP_CHECKSIG;
    return CreateNewBlock(scriptPubKey);
}
static bool ProcessBlockFound(CBlock* pblock, CWallet& wallet)
{
    LogPrintf("%s\n", pblock->ToString());
    LogPrintf("generated %s\n", FormatMoney(pblock->vtx[0].vout[0].nValue));

    // Found a solution
    {
        LOCK(cs_main);
        if (pblock->hashPrevBlock != chainActive.Tip()->GetBlockHash())
            return error("GcoinMiner: generated block is stale");
    }

    // Track how many getdata requests this block gets
    {
        LOCK(wallet.cs_wallet);
        wallet.mapRequestCount[pblock->GetHash()] = 0;
    }

    if (!SignBlockHeader(wallet, (*pblock)))
        return error("GcoinMiner : SignBlockHeader failed");

    // Process this block the same as if we had received it from another node
    CValidationState state;
    if (!ProcessNewBlock(state, NULL, pblock, true, NULL))
        return error("GcoinMiner: ProcessNewBlock, block not accepted");

    return true;
}

// use to veify whether we can mine or not.
bool EnableCreateBlock()
{
    {
        LOCK(mempool.cs);
        if (mempool.mapTx.size() > 0) {
            LogPrintf("EnableCreateBlock : pool have %d transction\n", mempool.mapTx.size());
            return true;
        }
    }
    LOCK(cs_main);
    CBlockIndex* pindex = chainActive.Tip();

    // we allow first block
    if (pindex->nHeight == 0)
        return true;

    for (int i = 0; i < COINBASE_MATURITY && pindex; i++) {
        CBlock block;
        if (!ReadBlockFromDisk(block, pindex)) {
            LogPrintf("ERROR : %s() Read block fail at block hash %s\n", __func__, pindex->GetBlockHash().ToString());
            return false;
        }
        if (block.vtx.size() > 1) {
            LogPrintf("%s() : height %d have transactions\n", __func__, pindex->nHeight);
            return true;
        }
        pindex = pindex->pprev;
    }
    return false;
}

void static GcoinMiner(CWallet *pwallet, CPubKey pubkey)
{
    LogPrintf("GcoinMiner started\n");
    SetThreadPriority(THREAD_PRIORITY_LOWEST);
    RenameThread("gcoin-miner");
    const CChainParams& chainparams = Params();

    // Each thread has its own key and counter
    CReserveKey reservekey(pwallet);
    unsigned int nExtraNonce = 0;
    try {
        uint32_t try_times = 0;
        bool fRetry = false;
        uint32_t nStartTime = 0;
        while (true) {
            try_times++;
            if (chainparams.MiningRequiresPeers()) {
                // Busy-wait for the network to come online so we don't waste time mining
                // on an obsolete chain. In regtest mode we expect to fly solo.
                do {
                    bool fvNodesEmpty;
                    {
                        LOCK(cs_vNodes);
                        fvNodesEmpty = vNodes.empty();
                    }
                    if (!fvNodesEmpty && !IsInitialBlockDownload())
                        break;
                    MilliSleep(1000);
                } while (true);
            }

            // Busy-wait for tx come in so we don't waste time mining
            while (!EnableCreateBlock())
                MilliSleep(3000);

            //
            // Create new block
            //
            unsigned int nTransactionsUpdatedLast = mempool.GetTransactionsUpdated();
            CBlockIndex* pindexPrev = chainActive.Tip();

            if (!fRetry) {
                nStartTime = 0;
            }
            auto_ptr<CBlockTemplate> pblocktemplate;
            {
                LOCK(cs_main);
                if (!EnableCreateBlock()) {
                    try_times = 0;
                    fRetry = false;
                    continue;
                }
                pblocktemplate.reset(CreateNewBlock(pwallet, pubkey, nStartTime));
            }

            if (!pblocktemplate.get())
            {
                LogPrintf("Error in GcoinMiner: Keypool ran out, please call keypoolrefill before restarting the mining thread\n");
                return;
            }

            CBlock *pblock = &pblocktemplate->block;
            if (fRetry) {
                fRetry = false;
            } else {
                nStartTime = pblock->vtx[0].nLockTime;
            }
            IncrementExtraNonce(pblock, pindexPrev, nExtraNonce);

            LogPrintf("Running GcoinMiner with %u transactions in block (%u bytes)\n", pblock->vtx.size(),
                ::GetSerializeSize(*pblock, SER_NETWORK, PROTOCOL_VERSION));
            //
            // Search
            //
            int64_t nStart = GetAdjustedTime();
            arith_uint256 hashTarget = arith_uint256().SetCompact(pblock->nBits);
            uint256 hash;
            uint32_t nNonce = 0;
            while (true) {
                bool fFound = ScanHash(pblock, nNonce, &hash);
                // Check if something found
                if (fFound) {
                    std::string addr = GetTxOutputAddr(pblock->vtx[0], 0);
                    unsigned int nMining = pminer->NumOfMiners();
                    arith_uint256 hashTemp(hashTarget / pow(Params().DynamicDiff(), pblkminer->NumOfMined(addr, nMining)));
                    if (UintToArith256(hash) <= hashTemp) {
                        // Found a solution
                        pblock->nNonce = nNonce;
                        assert(hash == pblock->GetHash());

                        SetThreadPriority(THREAD_PRIORITY_NORMAL);
                        LogPrintf("GcoinMiner:\n");
                        LogPrintf("proof-of-work found  \n  hash: %s  \ntarget: %s\n", hash.GetHex(), hashTarget.GetHex());
                        LogPrintf("Try times : %d, cost time : %d\n", try_times, pblock->nTime - pblock->vtx[0].nLockTime);
                        try_times = 0;
                        ProcessBlockFound(pblock, *pwallet);
                        SetThreadPriority(THREAD_PRIORITY_LOWEST);

                        // In regression test mode, stop mining after a block is found.
                        if (chainparams.MineBlocksOnDemand())
                            throw boost::thread_interrupted();

                        break;
                    }
                }

                // Check for stop or if block needs to be rebuilt
                boost::this_thread::interruption_point();
                // Regtest mode doesn't require peers
                if (vNodes.empty() && chainparams.MiningRequiresPeers())
                    break;
                if (nNonce >= 0xffff0000) {
                    fRetry = true;
                    break;
                }
                if (mempool.GetTransactionsUpdated() != nTransactionsUpdatedLast && GetTime() - nStart > 60) {
                    fRetry = true;
                    break;
                }
                if (pindexPrev != chainActive.Tip()) {
                    fRetry = false;
                    try_times = 0;
                    break;
                }

                // Update nTime every few seconds
                UpdateTime(pblock, chainparams.GetConsensus(), pindexPrev);
                if (chainparams.GetConsensus().fPowAllowMinDifficultyBlocks)
                {
                    // Changing pblock->nTime can change work required on testnet:
                    hashTarget.SetCompact(pblock->nBits);
                }
            }
        }
    }
    catch (const boost::thread_interrupted&)
    {
        LogPrintf("GcoinMiner terminated\n");
        throw;
    }
    catch (const std::runtime_error &e)
    {
        LogPrintf("GcoinMiner runtime error: %s\n", e.what());
        return;
    }
}

void GenerateGcoins(bool fGenerate, CWallet* pwallet, int nThreads)
{
    static boost::thread_group* minerThreads = NULL;

    if (nThreads < 0) {
        // In regtest threads defaults to 1
        if (Params().DefaultMinerThreads())
            nThreads = Params().DefaultMinerThreads();
        else
            nThreads = boost::thread::hardware_concurrency();
    }

    if (minerThreads != NULL)
    {
        minerThreads->interrupt_all();
        delete minerThreads;
        minerThreads = NULL;
    }

    if (nThreads == 0 || !fGenerate)
        return;

    CReserveKey reservekey(pwallet);
    CPubKey pubkey;
    pubkey = pwallet->vchDefaultKey;

    //only miner can mine block
    std::string addr = CBitcoinAddress(pubkey.GetID()).ToString();
    if (palliance->NumOfMembers() != 0 && !pminer->IsMiner(addr)) {
        mapArgs["-gen"] = "false";
        return;
    }

    minerThreads = new boost::thread_group();
    for (int i = 0; i < nThreads; i++)
        minerThreads->create_thread(boost::bind(&GcoinMiner, pwallet, pubkey));
}

#endif // ENABLE_WALLET
