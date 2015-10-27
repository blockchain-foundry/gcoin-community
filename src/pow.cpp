// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "pow.h"

#include "chainparams.h"
#include "core.h"
#include "main.h"
#include "timedata.h"
#include "uint256.h"
#include "util.h"

int64_t GetActualMiningTimespan(const CBlockIndex* pindexLast) {
    int64_t TotalTime = 0;
    if (pindexLast->nHeight >= Params().Interval() - 1) {
        const CBlockIndex* pindex = pindexLast;
        for (int i = 0; i < Params().Interval() && pindex != NULL; i++) {
            CBlock block;
            if (!ReadBlockFromDisk(block, pindex)) {
                LogPrintf("ERROR : %s() Read block fail at block hash %s\n", __func__, pindex->GetBlockHash().ToString());
                return Params().TargetTimespan();
            }
            TotalTime += (pindex->nTime - block.vtx[0].nLockTime);
            pindex = pindex->pprev;
        }
    }
    return TotalTime;
}

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock)
{
    unsigned int nProofOfWorkLimit = Params().ProofOfWorkLimit().GetCompact();

    // Genesis block
    if (pindexLast == NULL)
        return nProofOfWorkLimit;

    // Only change once per interval
    if ((pindexLast->nHeight + 1) % Params().Interval() != 0) {
        if (Params().AllowMinDifficultyBlocks()) {
            // Special difficulty rule for testnet:
            // If the new block's timestamp is more than 2* 10 minutes
            // then allow mining of a min-difficulty block.
            if (pblock->GetBlockTime() > pindexLast->GetBlockTime() + Params().TargetSpacing()*2)
                return nProofOfWorkLimit;
            else {
                // Return the last non-special-min-difficulty-rules-block
                const CBlockIndex* pindex = pindexLast;
                while (pindex->pprev && pindex->nHeight % Params().Interval() != 0 && pindex->nBits == nProofOfWorkLimit)
                    pindex = pindex->pprev;
                return pindex->nBits;
            }
        }
        return pindexLast->nBits;
    }

    // Go back by what we want to be 14 days worth of blocks
    const CBlockIndex* pindexFirst = pindexLast;
    for (int i = 0; pindexFirst && i < Params().Interval()-1; i++)
        pindexFirst = pindexFirst->pprev;
    assert(pindexFirst);

    // Limit adjustment step
    int64_t nActualTimespan = GetActualMiningTimespan(pindexLast);
    LogPrintf("  nActualTimespan = %d  before bounds\n", nActualTimespan);
    if (nActualTimespan < Params().TargetTimespan()/4)
        nActualTimespan = Params().TargetTimespan()/4;
    if (nActualTimespan > Params().TargetTimespan()*4)
        nActualTimespan = Params().TargetTimespan()*4;

    // Retarget
    uint256 bnNew;
    uint256 bnOld;
    bnNew.SetCompact(pindexLast->nBits);
    bnOld = bnNew;
    bnNew *= nActualTimespan;
    bnNew /= Params().TargetTimespan();

    if (bnNew > Params().ProofOfWorkLimit())
        bnNew = Params().ProofOfWorkLimit();

    /// debug print
    LogPrintf("%s RETARGET\n", __func__);
    LogPrintf("Params().TargetTimespan() = %d    nActualTimespan = %d\n", Params().TargetTimespan(), nActualTimespan);
    LogPrintf("Before: %08x  %s\n", pindexLast->nBits, bnOld.ToString());
    LogPrintf("After:  %08x  %s\n", bnNew.GetCompact(), bnNew.ToString());

    return bnNew.GetCompact();
}

bool CheckProofOfWork(uint256 hash, unsigned int nBits)
{
    bool fNegative;
    bool fOverflow;
    uint256 bnTarget;
    bnTarget.SetCompact(nBits, &fNegative, &fOverflow);

    // Check range
    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > Params().ProofOfWorkLimit())
        return error("%s() : nBits below minimum work", __func__);

    // Check proof of work matches claimed amount
    if (hash > bnTarget)
        return error("%s() : hash doesn't match nBits", __func__);

    return true;
}

//
// true if nBits is greater than the minimum amount of work that could
// possibly be required deltaTime after minimum work required was nBase
//
bool CheckMinWork(unsigned int nBits, unsigned int nBase, int64_t deltaTime)
{
    bool fOverflow = false;
    uint256 bnNewBlock;
    bnNewBlock.SetCompact(nBits, NULL, &fOverflow);
    if (fOverflow)
        return false;

    const uint256 &bnLimit = Params().ProofOfWorkLimit();
    // Testnet has min-difficulty blocks
    // after Params().TargetSpacing()*2 time between blocks:
    if (Params().AllowMinDifficultyBlocks() && deltaTime > Params().TargetSpacing()*2)
        return bnNewBlock <= bnLimit;

    uint256 bnResult;
    bnResult.SetCompact(nBase);
    while (deltaTime > 0 && bnResult < bnLimit) {
        // Maximum 400% adjustment...
        bnResult *= 4;
        // ... in best-case exactly 4-times-normal target time
        deltaTime -= Params().TargetTimespan()*4;
    }
    if (bnResult > bnLimit)
        bnResult = bnLimit;

    return bnNewBlock <= bnResult;
}

void UpdateTime(CBlock* pblock, const CBlockIndex* pindexPrev)
{
    pblock->nTime = std::max(std::max(pindexPrev->GetMedianTimePast()+1, GetAdjustedTime()), pblock->GetBlockStartTime() + 1);

    // Updating time can change work required on testnet:
    if (Params().AllowMinDifficultyBlocks())
        pblock->nBits = GetNextWorkRequired(pindexPrev, pblock);
}

uint256 GetProofIncrement(unsigned int nBits)
{
    uint256 bnTarget;
    bool fNegative;
    bool fOverflow;
    bnTarget.SetCompact(nBits, &fNegative, &fOverflow);
    if (fNegative || fOverflow || bnTarget == 0)
        return 0;
    // We need to compute 2**256 / (bnTarget+1), but we can't represent 2**256
    // as it's too large for a uint256. However, as 2**256 is at least as large
    // as bnTarget+1, it is equal to ((2**256 - bnTarget - 1) / (bnTarget+1)) + 1,
    // or ~bnTarget / (nTarget+1) + 1.
    return (~bnTarget / (bnTarget + 1)) + 1;
}
