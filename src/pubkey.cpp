// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "pubkey.h"

#include "eccryptoverify.h"

#include "ecwrapper.h"

#include <osrng.h>
#include <eccrypto.h>
#include <ecpoint.h>
#include <integer.h>
#include <oids.h>
using namespace CryptoPP;

bool CPubKey::Verify(const uint256 &hash, const std::vector<unsigned char>& vchSig) const {
    if (!IsValid())
        return false;
    CECKey key;
    if (!key.SetPubKey(begin(), size()))
        return false;
    if (!key.Verify(hash, vchSig))
        return false;
    return true;
}

bool CPubKey::Encrypt(const std::string& plainData, std::string& cryptData) const
{
    AutoSeededRandomPool prng;
    unsigned char vx[32];
    unsigned char vy[32];
    if (IsCompressed()) {
        if (!IsValid())
            return false;
        CECKey key;
        if (!key.SetPubKey(begin(), size()))
            return false;
        std::vector<unsigned char> pubkey;
        key.GetPubKey(pubkey, false);
        memcpy(vx, (unsigned char*)&pubkey[1], 32);
        memcpy(vy, (unsigned char*)&pubkey[33], 32);
    } else {
        memcpy(vx, vch + 1, 32);
        memcpy(vy, vch + 33, 32);
    }
    Integer x(vx, 32);
    Integer y(vy, 32);
    ECIES<ECP>::Encryptor e0(ASN1::secp256k1(), ECPPoint(x, y));
    e0.GetPublicKey().ThrowIfInvalid(prng, 3);

    // Encrypt message
    StringSource ss(plainData, true, new PK_EncryptorFilter(prng, e0, new StringSink(cryptData)));

    return true;
}
bool CPubKey::RecoverCompact(const uint256 &hash, const std::vector<unsigned char>& vchSig) {
    if (vchSig.size() != 65)
        return false;
    int recid = (vchSig[0] - 27) & 3;
    bool fComp = ((vchSig[0] - 27) & 4) != 0;
    CECKey key;
    if (!key.Recover(hash, &vchSig[1], recid))
        return false;
    std::vector<unsigned char> pubkey;
    key.GetPubKey(pubkey, fComp);
    Set(pubkey.begin(), pubkey.end());
    return true;
}

bool CPubKey::IsFullyValid() const {
    if (!IsValid())
        return false;
    CECKey key;
    if (!key.SetPubKey(begin(), size()))
        return false;
    return true;
}

bool CPubKey::Decompress() {
    if (!IsValid())
        return false;
    CECKey key;
    if (!key.SetPubKey(begin(), size()))
        return false;
    std::vector<unsigned char> pubkey;
    key.GetPubKey(pubkey, false);
    Set(pubkey.begin(), pubkey.end());
    return true;
}

bool CPubKey::Derive(CPubKey& pubkeyChild, ChainCode &ccChild, unsigned int nChild, const ChainCode& cc) const {
    assert(IsValid());
    assert((nChild >> 31) == 0);
    assert(begin() + 33 == end());
    unsigned char out[64];
    BIP32Hash(cc, nChild, *begin(), begin()+1, out);
    memcpy(ccChild.begin(), out+32, 32);
    CECKey key;
    bool ret = key.SetPubKey(begin(), size());
    ret &= key.TweakPublic(out);
    std::vector<unsigned char> pubkey;
    key.GetPubKey(pubkey, true);
    pubkeyChild.Set(pubkey.begin(), pubkey.end());
    return ret;
}

void CExtPubKey::Encode(unsigned char code[BIP32_EXTKEY_SIZE]) const {
    code[0] = nDepth;
    memcpy(code+1, vchFingerprint, 4);
    code[5] = (nChild >> 24) & 0xFF; code[6] = (nChild >> 16) & 0xFF;
    code[7] = (nChild >>  8) & 0xFF; code[8] = (nChild >>  0) & 0xFF;
    memcpy(code+9, chaincode.begin(), 32);
    assert(pubkey.size() == 33);
    memcpy(code+41, pubkey.begin(), 33);
}

void CExtPubKey::Decode(const unsigned char code[BIP32_EXTKEY_SIZE]) {
    nDepth = code[0];
    memcpy(vchFingerprint, code+1, 4);
    nChild = (code[5] << 24) | (code[6] << 16) | (code[7] << 8) | code[8];
    memcpy(chaincode.begin(), code+9, 32);
    pubkey.Set(code+41, code+BIP32_EXTKEY_SIZE);
}

bool CExtPubKey::Derive(CExtPubKey &out, unsigned int nChild) const {
    out.nDepth = nDepth + 1;
    CKeyID id = pubkey.GetID();
    memcpy(&out.vchFingerprint[0], &id, 4);
    out.nChild = nChild;
    return pubkey.Derive(out.pubkey, out.chaincode, nChild, chaincode);
}
