// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Copyright (c) 2014-2016 The Gcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "primitives/transaction.h"

#include "wallet/crypter.h"
#include "hash.h"
#include "random.h"
#include "streams.h"
#include "tinyformat.h"
#include "utilstrencodings.h"

std::string COutPoint::ToString() const
{
    return strprintf("COutPoint(%s, %u)", hash.ToString().substr(0,10), n);
}

CTxIn::CTxIn(COutPoint prevoutIn, CScript scriptSigIn, uint32_t nSequenceIn)
{
    prevout = prevoutIn;
    scriptSig = scriptSigIn;
    nSequence = nSequenceIn;
}

CTxIn::CTxIn(uint256 hashPrevTx, uint32_t nOut, CScript scriptSigIn, uint32_t nSequenceIn)
{
    prevout = COutPoint(hashPrevTx, nOut);
    scriptSig = scriptSigIn;
    nSequence = nSequenceIn;
}

std::string CTxIn::ToString() const
{
    std::string str;
    str += "CTxIn(";
    str += prevout.ToString();
    if (prevout.IsNull())
        str += strprintf(", coinbase %s", HexStr(scriptSig));
    else
        str += strprintf(", scriptSig=%s", scriptSig.ToString().substr(0,24));
    if (nSequence != std::numeric_limits<unsigned int>::max())
        str += strprintf(", nSequence=%u", nSequence);
    str += ")";
    return str;
}
CTxOut::CTxOut(const CAmount& nValueIn, CScript scriptPubKeyIn, type_Color colorIn)
{
    nValue = nValueIn;
    scriptPubKey = scriptPubKeyIn;
    color = colorIn;
}

uint256 CTxOut::GetHash() const
{
    return SerializeHash(*this);
}

std::string CTxOut::ToString() const
{
    return strprintf("CTxOut(nValue=%d.%08d, scriptPubKey=%s, color=%d)", nValue / COIN, nValue % COIN, scriptPubKey.ToString().substr(0,30), color);
}

CMutableTransaction::CMutableTransaction() : nVersion(CTransaction::CURRENT_VERSION), nLockTime(0), type(NORMAL) {}
CMutableTransaction::CMutableTransaction(const CTransaction& tx) : nVersion(tx.nVersion), pubKeys(tx.pubKeys), encryptedKeys(tx.encryptedKeys), vin(tx.vin), vout(tx.vout), nLockTime(tx.nLockTime), type(tx.type), chex(tx.chex) {}

uint256 CMutableTransaction::GetHash() const
{
    return SerializeHash(*this);
}

void CTransaction::UpdateHash() const
{
    *const_cast<uint256*>(&hash) = SerializeHash(*this);
}

void CTransaction::UpdateHex(const std::string& hex) const
{
    *const_cast<std::string*>(&phex) = hex;
}

CTransaction::CTransaction() : nVersion(CTransaction::CURRENT_VERSION), pubKeys(), encryptedKeys(), vin(), vout(), nLockTime(0), type(NORMAL) {}
CTransaction::CTransaction(const CMutableTransaction &tx) : nVersion(tx.nVersion), pubKeys(tx.pubKeys), encryptedKeys(tx.encryptedKeys), vin(tx.vin), vout(tx.vout), nLockTime(tx.nLockTime), type(tx.type), chex(tx.chex)
{
    UpdateHash();
}

CTransaction& CTransaction::operator=(const CTransaction &tx) {
    *const_cast<int*>(&nVersion) = tx.nVersion;
    *const_cast<std::vector<CPubKey>*>(&pubKeys) = tx.pubKeys;
    *const_cast<std::vector<std::string>*>(&encryptedKeys) = tx.encryptedKeys;
    *const_cast<std::vector<CTxIn>*>(&vin) = tx.vin;
    *const_cast<std::vector<CTxOut>*>(&vout) = tx.vout;
    *const_cast<unsigned int*>(&nLockTime) = tx.nLockTime;
    *const_cast<uint256*>(&hash) = tx.hash;
    *const_cast<tx_type*>(&type) = tx.type;
    *const_cast<std::string*>(&chex) = tx.chex;
    return *this;
}

CAmount CTransaction::GetValueOut() const
{
    CAmount nValueOut = 0;
    colorAmount_t nValueOut_;
    for (std::vector<CTxOut>::const_iterator it(vout.begin()); it != vout.end(); ++it)
    {
        nValueOut += it->nValue;
        if (nValueOut_.find(it->color) != nValueOut_.end()) {
            nValueOut_[it->color] += it->nValue;
        } else {
            nValueOut_[it->color] = it->nValue;
        }
        if (!MoneyRange(it->nValue) || !MoneyRange(nValueOut_[it->color]))
            throw std::runtime_error("CTransaction::GetValueOut(): value out of range");
    }
    return nValueOut;
}

double CTransaction::ComputePriority(double dPriorityInputs, unsigned int nTxSize) const
{
    nTxSize = CalculateModifiedSize(nTxSize);
    if (nTxSize == 0) return 0.0;

    return dPriorityInputs / nTxSize;
}

unsigned int CTransaction::CalculateModifiedSize(unsigned int nTxSize) const
{
    // In order to avoid disincentivizing cleaning up the UTXO set we don't count
    // the constant overhead for each txin and up to 110 bytes of scriptSig (which
    // is enough to cover a compressed pubkey p2sh redemption) for priority.
    // Providing any more cleanup incentive than making additional inputs free would
    // risk encouraging people to create junk outputs to redeem later.
    if (nTxSize == 0)
        nTxSize = ::GetSerializeSize(*this, SER_NETWORK, PROTOCOL_VERSION);
    for (std::vector<CTxIn>::const_iterator it(vin.begin()); it != vin.end(); ++it)
    {
        unsigned int offset = 41U + std::min(110U, (unsigned int)it->scriptSig.size());
        if (nTxSize > offset)
            nTxSize -= offset;
    }
    return nTxSize;
}

std::string CTransaction::EncodeHexCryptedTx() const
{
    CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
    ssTx << *this;
    unsigned nSize = NONCRYPTED_TX_FIELD_SIZE;
    ssTx.ignore(nSize);
    return HexStr(ssTx.begin(), ssTx.end());
}

bool CTransaction::Decrypt(const unsigned int& index, const CKey& vchPrivKey)
{
    // Decrypt the key with given secp256k1 privkey
    std::string strKey;
    vchPrivKey.Decrypt(encryptedKeys[index], strKey);
    CKeyingMaterial vchKey(strKey.begin(), strKey.begin() + WALLET_CRYPTO_KEY_SIZE);
    std::vector<unsigned char> vchIV(strKey.begin() + WALLET_CRYPTO_KEY_SIZE, strKey.end());
    // Decrypt cs with the AES key and IV
    CCrypter cKeyCrypter;
    if (!cKeyCrypter.SetKey(vchKey, vchIV))
        return false;
    std::vector<unsigned char> vchCryptData(chex.begin(), chex.end());
    CKeyingMaterial vchPlainData;
    if (!cKeyCrypter.Decrypt(vchCryptData, vchPlainData))
        return false;
    std::string hex(vchPlainData.begin(), vchPlainData.end());
    *const_cast<std::string*>(&phex) = hex;
    // Deserialize stream cs
    DecodeHexCryptedTx();
    *const_cast<std::string*>(&phex) = "";
    UpdateHash();

    return true;
}

bool CTransaction::DecodeHexCryptedTx()
{
    if (!IsHex(phex))
        return false;

    std::vector<unsigned char> txData(ParseHex(phex));
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << *this;
    unsigned nSize = NONCRYPTED_TX_FIELD_SIZE;
    ss.erase(ss.begin() + nSize, ss.end());
    CDataStream ssData(txData, SER_NETWORK, PROTOCOL_VERSION);
    ss += ssData;
    try {
        ss >> *this;
    }
    catch (const std::exception&) {
        return false;
    }

    return true;
}

std::string CTransaction::ToString() const
{
    std::string str;
    str += strprintf("CTransaction(hash=%s, ver=%d, encrypted=%s, vin.size=%u, vout.size=%u, nLockTime=%u, type=%s)\n",
        GetHash().ToString().substr(0,10),
        nVersion,
        encryptedKeys.size() > 0? "true": "false",
        vin.size(),
        vout.size(),
        nLockTime,
        GetTypeName(type));
    for (unsigned int i = 0; i < vin.size(); i++)
        str += "    " + vin[i].ToString() + "\n";
    for (unsigned int i = 0; i < vout.size(); i++)
        str += "    " + vout[i].ToString() + "\n";
    for (unsigned int i = 0; i < encryptedKeys.size(); i++)
        str += "    " + HexStr(encryptedKeys[i]) + "\n";
    return str;
}

std::string GetTypeName(tx_type type)
{
    switch (type) {
        case NORMAL:
            return "NORMAL";
        case MINT:
            return "MINT";
        case VOTE:
            return "VOTE";
        case LICENSE:
            return "LICENSE";
        case MINER:
            return "MINER";
        case DEMINER:
            return "DEMINER";
        default:
            return "UNKNOWN";
    }
}

bool CMutableTransaction::Encrypt(const std::vector<CPubKey>& vchPubKeys)
{
    if (vchPubKeys.empty())
        return false;
    if (!chex.empty())
        return true;
    pubKeys = vchPubKeys;
    // Fetch the data hex
    std::string strData = EncodeHexCryptedTx();

    // Random create AES key and IV
    CCrypter cKeyCrypter;
    RandAddSeedPerfmon();
    CKeyingMaterial vchKey;
    vchKey.resize(WALLET_CRYPTO_KEY_SIZE);
    GetRandBytes(&vchKey[0], WALLET_CRYPTO_KEY_SIZE);
    std::vector<unsigned char> vchIV;
    vchIV.resize(WALLET_CRYPTO_KEY_SIZE);
    GetRandBytes(&vchIV[0], WALLET_CRYPTO_KEY_SIZE);

    // Encrypt the key with given secp256k1 pubkey
    std::string strKey(vchKey.begin(), vchKey.end());
    strKey += std::string(vchIV.begin(), vchIV.end());
    for (std::vector<CPubKey>::const_iterator it(vchPubKeys.begin()); it != vchPubKeys.end(); it++) {
        std::string strCryptedKey;
        it->Encrypt(strKey, strCryptedKey);
        encryptedKeys.push_back(strCryptedKey);
    }

    // Encrypt the data hex
    if (!cKeyCrypter.SetKey(vchKey, vchIV))
        return false;
    CKeyingMaterial vchPlainData(strData.begin(), strData.end());
    std::vector<unsigned char> vchCryptData;
    if (!cKeyCrypter.Encrypt(vchPlainData, vchCryptData))
        return false;
    chex = std::string(vchCryptData.begin(), vchCryptData.end());

    return true;
}

std::string CMutableTransaction::EncodeHexCryptedTx()
{
    CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
    ssTx << *this;
    unsigned nSize = NONCRYPTED_TX_FIELD_SIZE;
    ssTx.ignore(nSize);
    return HexStr(ssTx.str());
}

std::string CMutableTransaction::ToString() const
{
    std::string str;
    str += strprintf("CMutableTransaction(hash=%s, ver=%d, encrypted=%s, vin.size=%u, vout.size=%u, nLockTime=%u, type=%s)\n",
        GetHash().ToString().substr(0,10),
        nVersion,
        encryptedKeys.size() > 0? "true": "false",
        vin.size(),
        vout.size(),
        nLockTime,
        GetTypeName(type));
    for (unsigned int i = 0; i < vin.size(); i++)
        str += "    " + vin[i].ToString() + "\n";
    for (unsigned int i = 0; i < vout.size(); i++)
        str += "    " + vout[i].ToString() + "\n";
    for (unsigned int i = 0; i < encryptedKeys.size(); i++)
        str += "    " + HexStr(encryptedKeys[i]) + "\n";
    return str;
}
