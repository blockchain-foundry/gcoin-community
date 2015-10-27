// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "miner.h"
#include "script.h"
#include "keystore.h"
#include "core.h"
#include "hash.h"
#include "main.h"
#include "net.h"
#include "pow.h"
#include "util.h"
#include "utilmoneystr.h"
#ifdef ENABLE_WALLET
#include "wallet.h"
#endif

#include <boost/thread.hpp>
#include "timedata.h"

using std::vector;
using std::map;
using std::list;
using std::set;
using std::auto_ptr;
//////////////////////////////////////////////////////////////////////////////
//
// BitcoinMiner
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
        if (byFee) {
            if (a.get<1>() == b.get<1>())
                return a.get<0>() < b.get<0>();
            return a.get<1>() < b.get<1>();
        } else {
            if (a.get<0>() == b.get<0>())
                return a.get<1>() < b.get<1>();
            return a.get<0>() < b.get<0>();
        }
    }
};

CBlockTemplate* CreateNewBlock(const CScript& scriptPubKeyIn, CWallet *pwallet, bool fMiningPool)
{
    // Create new block
    auto_ptr<CBlockTemplate> pblocktemplate(new CBlockTemplate());
    if (!pblocktemplate.get())
        return NULL;
    CBlock *pblock = &pblocktemplate->block; // pointer for convenience

    // Create coinbase tx
    CMutableTransaction txNew;
    txNew.vin.resize(1);
    txNew.vin[0].prevout.SetNull();
    txNew.vout.resize(1);
    txNew.vout[0].scriptPubKey = scriptPubKeyIn;
    txNew.out_itself = true;
    txNew.nLockTime = GetAdjustedTime();
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
    int64_t nFees = 0;
    map<int, CMutableTransaction> color_tx;
    {
        LOCK2(cs_main, mempool.cs);
        LogPrintf("CreateNewBlock() : pool size = %d\n", mempool.mapTx.size());
        CBlockIndex* pindexPrev = chainActive.Tip();
        CCoinsViewCache view(*pcoinsTip, true);

        // Priority order to process transactions
        list<COrphan> vOrphan; // list memory doesn't move
        map<uint256, vector<COrphan*> > mapDependers;
        bool fPrintPriority = GetBoolArg("-printpriority", false);

        // This vector will be sorted into a priority queue:
        vector<CMutableTransaction> vecCoinBase;
        vector<TxPriority> vecPriority;
        vecPriority.reserve(mempool.mapTx.size());
        for (map<uint256, CTxMemPoolEntry>::iterator mi = mempool.mapTx.begin();
             mi != mempool.mapTx.end(); ++mi) {
            const CTransaction& tx = mi->second.GetTx();

            //We allow mint(coinbase) transaction.
            //if (tx.IsCoinBase() || !IsFinalTx(tx, pindexPrev->nHeight + 1))
            if (!IsFinalTx(tx, pindexPrev->nHeight + 1))
                continue;

            COrphan* porphan = NULL;
            double dPriority = 0;
            int64_t nTotalIn = 0;
            bool fMissingInputs = false;
            bool colorInvalid = false;
            bool color_0 = false;

            if (!tx.IsCoinBase()) {
                BOOST_FOREACH(const CTxIn& txin, tx.vin) {
                    // Read prev transaction
                    if (!view.HaveCoins(txin.prevout.hash)) {
                        // This should never happen; all transactions in the memory
                        // pool should connect to either transactions in the chain
                        // or other transactions in the memory pool.
                        if (!mempool.mapTx.count(txin.prevout.hash)) {
                            LogPrintf("ERROR: mempool transaction missing input\n");
                            if (fDebug) assert("mempool transaction missing input" == 0);
                            fMissingInputs = true;
                            if (porphan)
                                vOrphan.pop_back();
                            break;
                        }

                        // Has to wait for dependencies
                        if (!porphan) {
                            // Use list for automatic deletion
                            vOrphan.push_back(COrphan(&tx));
                            porphan = &vOrphan.back();
                        }
                        mapDependers[txin.prevout.hash].push_back(porphan);
                        porphan->setDependsOn.insert(txin.prevout.hash);
                        nTotalIn += mempool.mapTx[txin.prevout.hash].GetTx().vout[txin.prevout.n].nValue;
                        continue;
                    }

                    uint256 hashBlock = 0;
                    CTransaction prevTx;
                    if (!GetTransaction(txin.prevout.hash, prevTx, hashBlock, NULL, true)) {
                        colorInvalid = true;
                        LogPrintf("GetTransaction failed.\n");
                        break;
                    }
                    //!@# test if the previous transaction's color equals the color of the present transaction
                    if (prevTx.color != 0 && tx.type != LICENSE) {
                        if (tx.color != prevTx.color) {
                            colorInvalid = true;
                            LogPrintf("Color Not Matched: tx.color = %d, prevtx.color = %d\n.", tx.color, prevTx.color);
                            LogPrintf("tx.hash = %s\n prevtx.hash = %s\n", tx.GetHash().GetHex(), prevTx.GetHash().GetHex());
                            break;
                        }
                    } else
                        color_0 = true;

                    const CCoins &coins = view.GetCoins(txin.prevout.hash);
                    int64_t nValueIn = coins.vout[txin.prevout.n].nValue;
                    nTotalIn += nValueIn;

                    int nConf = pindexPrev->nHeight - coins.nHeight + 1;

                    dPriority += (double)nValueIn * nConf;
                }
            }
            if (fMissingInputs) continue;
            if (colorInvalid) continue;

            if (!color_0 && !tx.IsCoinBase()) {
                if (tx.out_itself) {
                    if (nTotalIn-tx.GetValueOut() != (int64_t) ((tx.GetValueOut() - tx.vout[tx.vout.size() - 1].nValue) * nFeeRate) && tx.color != 0) {
                        LogPrintf("Illegal Transaction Fee : %d  expect : %d\n",nTotalIn-tx.GetValueOut(), (int64_t) ((tx.GetValueOut() - tx.vout[tx.vout.size() - 1].nValue) * nFeeRate));
                        continue;
                    }
                } else if (nTotalIn-tx.GetValueOut() != (int64_t) (tx.GetValueOut() * nFeeRate) && tx.color != 0) {
                    LogPrintf("Illegal Transaction Fee : %d  expect : %d\n",nTotalIn-tx.GetValueOut(), (int64_t) (tx.GetValueOut() * nFeeRate));
                    continue;
                }
            }
            // Priority is sum(valuein * age) / modified_txsize
            unsigned int nTxSize = ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);
            dPriority = tx.ComputePriority(dPriority, nTxSize);

            uint256 hash = tx.GetHash();
            mempool.ApplyDeltas(hash, dPriority, nTotalIn);

            CFeeRate feeRate(nTotalIn-tx.GetValueOut(), nTxSize);

            if (porphan) {
                porphan->dPriority = dPriority;
                porphan->feeRate = feeRate;
            } else
                vecPriority.push_back(TxPriority(dPriority, feeRate, &mi->second.GetTx()));
        }

        // Collect transactions into block
        uint64_t nBlockSize = 1000;
        uint64_t nBlockTx = 0;
        int nBlockSigOps = 100;
        bool fSortedByFee = (nBlockPrioritySize <= 0);

        TxPriorityCompare comparer(fSortedByFee);
        //!@# turn vector into priority Queue
        std::make_heap(vecPriority.begin(), vecPriority.end(), comparer);
        while (!vecPriority.empty()) {
            // Take highest priority transaction off the priority queue:
            double dPriority = vecPriority.front().get<0>();
            CFeeRate feeRate = vecPriority.front().get<1>();
            const CTransaction& tx = *(vecPriority.front().get<2>());

            //!@# kill that tx off the vector
            std::pop_heap(vecPriority.begin(), vecPriority.end(), comparer);
            vecPriority.pop_back();

            unsigned int ntxCBSize = 0;
            bool color_not_exist = color_tx.find(tx.color) == color_tx.end();
            if (color_not_exist) {
                CMutableTransaction txCB;
                txCB.vin.resize(1);
                txCB.vin[0].prevout.SetNull();
                txCB.vout.resize(1);
                txCB.vout[0].scriptPubKey = scriptPubKeyIn;
                txCB.color = tx.color;
                txCB.vin[0].scriptSig = CScript() << OP_0 << OP_0;
                ntxCBSize = ::GetSerializeSize(txCB, SER_NETWORK, PROTOCOL_VERSION);
                txCB.vout[0].nValue = 0;
                color_tx[tx.color] = txCB;
                txCB.out_itself = true;
            }
            // Size limits
            unsigned int nTxSize = ::GetSerializeSize(tx, SER_NETWORK, PROTOCOL_VERSION);
            if (nBlockSize + nTxSize + ntxCBSize >= nBlockMaxSize)
                continue;


            //!@# TODO: understand what is this
            // Legacy limits on sigOps:
            unsigned int nTxSigOps = GetLegacySigOpCount(tx);
            unsigned int nTxCBSigOps = GetLegacySigOpCount(color_tx[tx.color]);
            if (nBlockSigOps + nTxSigOps + nTxCBSigOps >= MAX_BLOCK_SIGOPS)
                continue;

            // Skip free transactions if we're past the minimum block size:
            const uint256& hash = tx.GetHash();
            double dPriorityDelta = 0;
            int64_t nFeeDelta = 0;
            mempool.ApplyDeltas(hash, dPriorityDelta, nFeeDelta);
            if (fSortedByFee && (dPriorityDelta <= 0) && (nFeeDelta <= 0) && (feeRate < ::minRelayTxFee) && (nBlockSize + nTxSize + ntxCBSize >= nBlockMinSize))
                continue;

            // Prioritise by fee once past the priority size or we run out of high-priority
            // transactions:
	        /*
            if (!fSortedByFee &&
                ((nBlockSize + nTxSize + ntxCBSize >= nBlockPrioritySize) || !AllowFree(dPriority)))
            {
                fSortedByFee = true;
                comparer = TxPriorityCompare(fSortedByFee);
                std::make_heap(vecPriority.begin(), vecPriority.end(), comparer);
            }
            */
            if (!view.HaveInputs(tx))
                continue;

            int64_t nTxFees = view.GetValueIn(tx) - tx.GetValueOut();

            nTxSigOps += GetP2SHSigOpCount(tx, view);
            if (nBlockSigOps + nTxSigOps + nTxCBSigOps >= MAX_BLOCK_SIGOPS)
                continue;

            //!@# TODO: here might be a point to enter
            // Note that flags: we don't want to set mempool/IsStandard()
            // policy here, but we still have to ensure that the block we
            // create only contains transactions that are valid in new blocks.
            CValidationState state;
            if (!CheckInputs(tx, state, view, true, MANDATORY_SCRIPT_VERIFY_FLAGS))
                continue;

            CTxUndo txundo;
            UpdateCoins(tx, state, view, txundo, pindexPrev->nHeight+1);

            // Added
            pblock->vtx.push_back(tx);
            pblocktemplate->vTxFees.push_back(nTxFees);
            pblocktemplate->vTxSigOps.push_back(nTxSigOps);
            nBlockSize += (nTxSize + ntxCBSize);
            ++nBlockTx;
            nBlockSigOps += (nTxSigOps + nTxCBSigOps);
            //nFees += nTxFees;
            color_tx[tx.color].vout[0].nValue += nTxFees;


            if (fPrintPriority)
                LogPrintf("priority %.1f fee %s txid %s\n",
                    dPriority, feeRate.ToString(), tx.GetHash().ToString());

            // Add transactions that depend on this one to the priority queue
            if (mapDependers.count(hash)) {
                BOOST_FOREACH(COrphan* porphan, mapDependers[hash]) {
                    if (!porphan->setDependsOn.empty()) {
                        porphan->setDependsOn.erase(hash);
                        if (porphan->setDependsOn.empty()) {
                            vecPriority.push_back(TxPriority(porphan->dPriority, porphan->feeRate, porphan->ptx));
                            std::push_heap(vecPriority.begin(), vecPriority.end(), comparer);
                        }
                    }
                }
            }
        }
        // add coinbase transaction to block
        for (map<int, CMutableTransaction>::iterator it = color_tx.begin(); it != color_tx.end(); it++) {
            if (it->second.vout[0].nValue > 0) {
                CMutableTransaction wtxNew = it->second;
                // we need to sign coinbase transaction now.
                if (pwallet != NULL)
                    if (!SignSignature(*pwallet, scriptPubKeyIn, wtxNew, 0))
                        throw std::runtime_error("Signing transaction failed at mining reward transaction");
                pblock->vtx.push_back(wtxNew);
                ++nBlockTx;
            }
        }


        // Compute final coinbase transaction.
        txNew.vout[0].nValue = GetBlockValue(pindexPrev->nHeight+1, nFees);
        txNew.vin[0].scriptSig = CScript() << OP_0 << OP_0;
        if (pwallet != NULL) {
            unsigned int ntxNewSize = ::GetSerializeSize(txNew, SER_NETWORK, PROTOCOL_VERSION);
            nBlockTx += ntxNewSize;
            if (!SignSignature(*pwallet, scriptPubKeyIn, txNew, 0))
                throw std::runtime_error("Signing transaction failed at mining reward transaction");
        }

        // we need to sign coinbase transaction now.
        pblock->vtx[0] = txNew;
        pblocktemplate->vTxFees[0] = -nFees;

        nLastBlockTx = nBlockTx;
        nLastBlockSize = nBlockSize;
        LogPrintf("%s(): total size %u  MAX : %u\n", __func__, nBlockSize,nBlockMaxSize);

        // Fill in header
        pblock->hashPrevBlock  = pindexPrev->GetBlockHash();
        UpdateTime(pblock, pindexPrev);
        pblock->nBits          = GetNextWorkRequired(pindexPrev, pblock);
        pblock->nNonce         = 0;
        pblocktemplate->vTxSigOps[0] = GetLegacySigOpCount(pblock->vtx[0]);

        CBlockIndex indexDummy(*pblock);
        indexDummy.pprev = pindexPrev;
        indexDummy.nHeight = pindexPrev->nHeight + 1;
        CCoinsViewCache viewNew(*pcoinsTip, true);
        CValidationState state;
        if (!ConnectBlock(*pblock, state, &indexDummy, viewNew, true, fMiningPool))
            throw std::runtime_error(_(__func__) + "() : ConnectBlock failed");
    }

    return pblocktemplate.release();
}


CBlockTemplate* CreateNewBlock(CWallet *pwallet, CPubKey pubkey)
{
    CScript scriptPubKeyIn = CScript() << pubkey << OP_CHECKSIG;
    return CreateNewBlock(scriptPubKeyIn, pwallet);
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

    // coinbase tx is signed already.
    //unsigned int nHeight = pindexPrev->nHeight+1; // Height first in coinbase required for block.version=2
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
double dHashesPerSec = 0.0;
int64_t nHPSTimerStart = 0;

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
        if ((nNonce & 0xffff) == 0)
            return false;
        if ((nNonce & 0xfff) == 0)
            boost::this_thread::interruption_point();
    }
}

CBlockTemplate* CreateNewBlockWithKey(CReserveKey& reservekey)
{
    CPubKey pubkey;
    if (!reservekey.GetReservedKey(pubkey))
        return NULL;

    CScript scriptPubKey = CScript() << pubkey << OP_CHECKSIG;
    return CreateNewBlock(scriptPubKey);
}

bool ProcessBlockFound(CBlock* pblock, CWallet& wallet)
{
    LogPrintf("%s\n", pblock->ToString());
    LogPrintf("generated %s\n", FormatMoney(pblock->vtx[0].vout[0].nValue));

    // Found a solution
    {
        LOCK(cs_main);
        if (pblock->hashPrevBlock != chainActive.Tip()->GetBlockHash())
            return error("BitcoinMiner : generated block is stale");
    }

    // Remove key from key pool
    //reservekey.KeepKey();

    // Track how many getdata requests this block gets
    {
        LOCK(wallet.cs_wallet);
        wallet.mapRequestCount[pblock->GetHash()] = 0;
    }

    // Process this block the same as if we had received it from another node
    CValidationState state;
    if (!ProcessBlock(state, NULL, pblock))
        return error("BitcoinMiner : ProcessBlock, block not accepted");

    return true;
}

// use to veify whether we can mine or not.
bool EnableCreateBlock() {
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
            LogPrintf("%s() : hegiht %d have transactions\n", __func__, pindex->nHeight);
            return true;
        }
        pindex = pindex->pprev;
    }
    return false;
}

void static BitcoinMiner(CWallet *pwallet, CPubKey pubkey)
{
    LogPrintf("BitcoinMiner started\n");
    SetThreadPriority(THREAD_PRIORITY_LOWEST);
    RenameThread("bitcoin-miner");

    // Each thread has its own key and counter
    unsigned int nExtraNonce = 0;
    try {
        while (true) {
            if (Params().MiningRequiresPeers()) {
                // Busy-wait for the network to come online so we don't waste time mining
                // on an obsolete chain. In regtest mode we expect to fly solo.
                while (vNodes.empty())
                    MilliSleep(1000);
            }

            // Busy-wait for tx come in so we don't waste time mining
            while (!EnableCreateBlock())
                MilliSleep(3000);

            //
            // Create new block
            //
            unsigned int nTransactionsUpdatedLast = mempool.GetTransactionsUpdated();
            CBlockIndex* pindexPrev = chainActive.Tip();
            auto_ptr<CBlockTemplate> pblocktemplate;

            {
                LOCK(cs_main);
                if(!EnableCreateBlock())
                    continue;
                pblocktemplate.reset(CreateNewBlock(pwallet, pubkey));
            }

            if (!pblocktemplate.get()) {
                LogPrintf("Error in BitcoinMiner: Keypool ran out, please call keypoolrefill before restarting the mining thread\n");
                return;
            }
            CBlock *pblock = &pblocktemplate->block;
            IncrementExtraNonce(pblock, pindexPrev, nExtraNonce);

            LogPrintf("Running BitcoinMiner with %u transactions in block (%u bytes)\n", pblock->vtx.size(),
                ::GetSerializeSize(*pblock, SER_NETWORK, PROTOCOL_VERSION));
            //
            // Search
            //
            int64_t nStart = GetAdjustedTime();
            uint256 hashTarget = uint256().SetCompact(pblock->nBits);
            uint256 hash;
            uint32_t nNonce = 0;
            uint32_t nOldNonce = 0;
            while (true) {
                bool fFound = ScanHash(pblock, nNonce, &hash);
                uint32_t nHashesDone = nNonce - nOldNonce;
                nOldNonce = nNonce;

                // Check if something found
                if (fFound) {
                    if (hash <= hashTarget) {
                        // Found a solution
                        pblock->nNonce = nNonce;
                        assert(hash == pblock->GetHash());

                        SetThreadPriority(THREAD_PRIORITY_NORMAL);
                        LogPrintf("BitcoinMiner:\n");
                        LogPrintf("proof-of-work found  \n  hash: %s  \ntarget: %s\n", hash.GetHex(), hashTarget.GetHex());
                        ProcessBlockFound(pblock, *pwallet);
                        SetThreadPriority(THREAD_PRIORITY_LOWEST);

                        // In regression test mode, stop mining after a block is found.
                        if (Params().MineBlocksOnDemand())
                            throw boost::thread_interrupted();

                        break;
                    }
                }

                // Meter hashes/sec
                static int64_t nHashCounter;
                if (nHPSTimerStart == 0) {
                    nHPSTimerStart = GetTimeMillis();
                    nHashCounter = 0;
                }
                else
                    nHashCounter += nHashesDone;
                if (GetTimeMillis() - nHPSTimerStart > 4000) {
                    static CCriticalSection cs;
                    {
                        LOCK(cs);
                        if (GetTimeMillis() - nHPSTimerStart > 4000) {
                            dHashesPerSec = 1000.0 * nHashCounter / (GetTimeMillis() - nHPSTimerStart);
                            nHPSTimerStart = GetTimeMillis();
                            nHashCounter = 0;
                            static int64_t nLogTime;
                            if (GetTime() - nLogTime > 30 * 60) {
                                nLogTime = GetTime();
                                LogPrintf("hashmeter %6.0f khash/s\n", dHashesPerSec/1000.0);
                            }
                        }
                    }
                }

                // Check for stop or if block needs to be rebuilt
                boost::this_thread::interruption_point();
                // Regtest mode doesn't require peers
                if (vNodes.empty() && Params().MiningRequiresPeers())
                    break;
                if (nNonce >= 0xffff0000)
                    break;
                if (mempool.GetTransactionsUpdated() != nTransactionsUpdatedLast && GetTime() - nStart > 60)
                    break;
                if (pindexPrev != chainActive.Tip())
                    break;

                // Update nTime every few seconds
                UpdateTime(pblock, pindexPrev);
                if (Params().AllowMinDifficultyBlocks()) {
                    // Changing pblock->nTime can change work required on testnet:
                    hashTarget.SetCompact(pblock->nBits);
                }
            }
        }
    } catch (boost::thread_interrupted) {
        LogPrintf("BitcoinMiner terminated\n");
        throw;
    }
}

void GenerateBitcoins(bool fGenerate, CWallet* pwallet, int nThreads)
{
    static boost::thread_group* minerThreads = NULL;

    if (nThreads < 0) {
        // In regtest threads defaults to 1
        if (Params().DefaultMinerThreads())
            nThreads = Params().DefaultMinerThreads();
        else
            nThreads = boost::thread::hardware_concurrency();
    }

    if (minerThreads != NULL) {
        minerThreads->interrupt_all();
        delete minerThreads;
        minerThreads = NULL;
    }

    if (nThreads == 0 || !fGenerate)
        return;

    CReserveKey reservekey(pwallet);
    CPubKey pubkey;
    if (!reservekey.GetReservedKey(pubkey))
        return;

    //only alliance member can mine block
    std::string addr = CBitcoinAddress(pubkey.GetID()).ToString();
    if (!alliance_member::IsMember(addr) && chainActive.Height() != 0) {
        mapArgs["-gen"] = "false";
        return;
    }
    minerThreads = new boost::thread_group();
    for (int i = 0; i < nThreads; i++)
        minerThreads->create_thread(boost::bind(&BitcoinMiner, pwallet, pubkey));
}

#endif // ENABLE_WALLET
