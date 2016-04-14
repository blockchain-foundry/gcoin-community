// Copyright (c) 2014-2016 The Gcoin Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "licenseinfo.h"


CLicenseInfo::CLicenseInfo() : nVersion(1), name(""), description(""), issuer(""), fDivisibility(true),
    feeType(FIXED), nFeeRate(0), feeCollectorAddr(""), nLimit(0), mintSchedule(FREE), fMemberControl(false),
    metadataLink(""), metadataHash(ArithToUint256(arith_uint256(0)))
{
}

// Get LicenseInfo version from block height if the format changed
int CLicenseInfo::GetVersionFromHeight(int height)
{
    return 1;
}

std::string CLicenseInfo::EncodeInfo() const
{
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << *this;
    return HexStr(ss.begin(), ss.end());
}

bool CLicenseInfo::DecodeInfo(const std::string& hexStr)
{
    if (!IsHex(hexStr))
        return false;

    std::vector<unsigned char> Data(ParseHex(hexStr));
    CDataStream ssData(Data, SER_NETWORK, PROTOCOL_VERSION);
    try {
        ssData >> *this;
    } catch (std::exception &e) {
        return false;
    }

    return IsValid();
}

bool CLicenseInfo::IsValid()
{
    if (name.size() > NAME_LEN)
        return false;
    else if (description.size() > DESCRIPTION_LEN)
        return false;
    else
        return true;
}
