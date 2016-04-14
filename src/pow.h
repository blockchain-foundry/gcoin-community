// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Copyright (c) 2014-2016 The Gcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_POW_H
#define BITCOIN_POW_H

#include "consensus/params.h"

#include <stdint.h>

class CBlockHeader;
class CBlockIndex;
class CBlock;
class uint256;
class arith_uint256;

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params&);
unsigned int CalculateNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params&);

/** Check whether a block hash satisfies the proof-of-work requirement specified by nBits */
arith_uint256 GetBlockProof(const CBlockIndex& block);

bool CheckProofOfWork(uint256 hash, unsigned int nBits, const Consensus::Params& params, unsigned int nSameMiner);
/** Check the work is more than the minimum a received block needs, without knowing its direct parent */
bool CheckMinWork(unsigned int nBits, unsigned int nBase, int64_t deltaTime);

void UpdateTime(CBlock* block, const CBlockIndex* pindexPrev);

/** Return the time it would take to redo the work difference between from and to, assuming the current hashrate corresponds to the difficulty at tip, in seconds. */
int64_t GetBlockProofEquivalentTime(const CBlockIndex& to, const CBlockIndex& from, const CBlockIndex& tip, const Consensus::Params&);

#endif // BITCOIN_POW_H
