// Copyright (c) 2014-2016 The Gcoin Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GCOIN_LICENSEINFO_H
#define GCOIN_LICENSEINFO_H

#include "script/standard.h"
#include "serialize.h"
#include "uint256.h"
#include "arith_uint256.h"
#include "streams.h"
#include "version.h"
#include "utilstrencodings.h"

#include <stdint.h>
#include <iomanip>

typedef uint32_t type_Color;
typedef uint32_t tx_type;


const int NAME_LEN = 32;
const int DESCRIPTION_LEN = 40;

enum FeeTypes {
    FIXED = 0,
    BYSIZE,
    BYAMOUNT
};

enum MintSchedule {
    FREE = 0,
    ONCE,
    LINEAR,
    HALFLIFE
};

using namespace std;

/*!
 * @brief   The structure of license information.
 */
class CLicenseInfo
{
public:
    // License version.
    int nVersion;
    // License name.
    string name;
    // License detail.
    string description;
    // License owner name.
    string issuer;
    // Divisibility (todo).
    bool fDivisibility;
    // Fee calculation policy (todo).
    int feeType;
    // Fee rate (todo).
    double nFeeRate;
    // Address of fee collector (todo).
    string feeCollectorAddr;
    // Upper limit of minting amount.
    int64_t nLimit;
    // Minting schedule policy (todo).
    int mintSchedule;
    // Member-only.
    bool fMemberControl;
    // Hyperlink for extra metadata.
    string metadataLink;
    // Finger print for metadata.
    uint256 metadataHash;

    CLicenseInfo();

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(nVersion);
        READWRITE(name);
        READWRITE(description);
        READWRITE(issuer);
        READWRITE(fDivisibility);
        READWRITE(feeType);
        READWRITE(nFeeRate);
        READWRITE(feeCollectorAddr);
        READWRITE(nLimit);
        READWRITE(mintSchedule);
        READWRITE(fMemberControl);
        READWRITE(metadataLink);
        READWRITE(metadataHash);
    }

    /*!
     * @brief   Give the version information for license information.
     * @param   height  The height to be checked.
     * @return  The version of the given height.
     */
    static int GetVersionFromHeight(int height);

    /*!
     * @brief   Encode the license information into a hex string.
     * @return  The hex string of the license information.
     */
    string EncodeInfo() const;

    /*!
     * @brief   Decode the given hex string and assign the value to current object.
     * @param   hexStr  The hex string to be decoded.
     * @return  True if the decoding process is successful.
     */
    bool DecodeInfo(const string& hexStr);

    /*!
     * @brief   Verify if the license information is valid.
     * @return  True if license information is valid.
     */
    bool IsValid();

    inline CLicenseInfo& operator=(const CLicenseInfo& rhs)
    {
        nVersion = rhs.nVersion;
        name = rhs.name;
        description = rhs.description;
        issuer = rhs.issuer;
        fDivisibility = rhs.fDivisibility;
        feeType = rhs.feeType;
        nFeeRate = rhs.nFeeRate;
        feeCollectorAddr = rhs.feeCollectorAddr;
        nLimit = rhs.nLimit;
        mintSchedule = rhs.mintSchedule;
        fMemberControl = rhs.fMemberControl;
        metadataLink = rhs.metadataLink;
        metadataHash = rhs.metadataHash;

        return *this;
    }
};

#endif
