// Copyright (c) 2014-2016 The Gcoin Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef GCOIN_CACHE_H
#define GCOIN_CACHE_H

#include "chainparams.h"
#include "clientversion.h"
#include "hash.h"
#include "policy/licenseinfo.h"
#include "random.h"
#include "uint256.h"
#include "util.h"

#include <algorithm>
#include <exception>
#include <map>
#include <set>
#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/unordered_map.hpp>

class TxInfo;

/*!
 * @brief The interface for all kinds of cache.
 */
template <class Tc, class Te>
class CacheInterface
{
public:
    CacheInterface()
    {
        pcontainer_ = new Tc();
    }
    virtual ~CacheInterface()
    {
        delete pcontainer_;
    }

    /*!
     * @brief   Add the element corresponding to the type of cache.
     * @param   e   The element to be added into the cache.
     * @return  True if element is added normally.
     */
    virtual bool Add(const Te &e)
    {
        return true;
    }

    /*!
     * @brief   Remove the given element.
     * @param   e   The element to be removed from the cache.
     * @return  True if the element is removed normally.
     */
    virtual bool Remove(const Te &e)
    {
        return true;
    }

    /*!
     * @brief   Clear the cache.
     * @return  True if the cache is cleared normally.
     */
    virtual bool RemoveAll()
    {
        return true;
    }

    /*!
     * @brief   Return when the cache is backed up by block height.
     * @return  The value of backed up block height.
     */
    int BackupHeight() const
    {
        return backupheight_;
    }

    /*!
     * @brief   Write the current cache status into disk.
     * @param   height  The backing up height to be recorded.
     * @return  True if the writing process is successful.
     */
    bool WriteDisk(const int height)
    {
        boost::filesystem::path pathAddr = GetDataDir() / filename_;
        // Generate random temporary filename
        unsigned short randv = 0;
        GetRandBytes((unsigned char*)&randv, sizeof(randv));
        std::string tmpfn = strprintf("%s.%04x", filename_, randv);
        backupheight_ = height;

        // serialize addresses, checksum data up to that point, then append csum
        CDataStream ssPeers(SER_DISK, CLIENT_VERSION);
        ssPeers << FLATDATA(Params().MessageStart());
        ssPeers << backupheight_;
        ssPeers << *pcontainer_;
        uint256 hash = Hash(ssPeers.begin(), ssPeers.end());
        ssPeers << hash;

        // open temp output file, and associate with CAutoFile
        boost::filesystem::path pathTmp = GetDataDir() / tmpfn;
        FILE *file = fopen(pathTmp.string().c_str(), "wb");
        CAutoFile fileout(file, SER_DISK, CLIENT_VERSION);
        if (fileout.IsNull())
            //return error("%s: Failed to open file %s", __func__, pathTmp.string());
            return false;

        // Write and commit header, data
        try {
            fileout << ssPeers;
        } catch (const std::exception& e) {
            //return error("%s: Serialize or I/O error - %s", __func__, e.what());
            return false;
        }
        FileCommit(fileout.Get());
        fileout.fclose();

        // replace existing peers.dat, if any, with new peers.dat.XXXX
        if (!RenameOver(pathTmp, pathAddr))
            //return error("%s: Rename-into-place failed", __func__);i
            return false;

        return true;
    }

    /*!
     * @brief   Read the disk data into cache.
     * @return  True if the reading process is successful.
     */
    bool ReadDisk()
    {
        RemoveAll();
        boost::filesystem::path pathAddr = GetDataDir() / filename_;
        // open input file, and associate with CAutoFile
        FILE *file = fopen(pathAddr.string().c_str(), "rb");
        CAutoFile filein(file, SER_DISK, CLIENT_VERSION);
        if (filein.IsNull())
            //return error("%s: Failed to open file %s", __func__, pathAddr.string());
            return true;

        // use file size to size memory buffer
        int fileSize = boost::filesystem::file_size(pathAddr);
        int dataSize = fileSize - sizeof(uint256);
        // Don't try to resize to a negative number if file is small
        if (dataSize < 0)
            dataSize = 0;
        vector<unsigned char> vchData;
        vchData.resize(dataSize);
        uint256 hashIn;

        // read data and checksum from file
        try {
            filein.read((char *)&vchData[0], dataSize);
            filein >> hashIn;
        } catch (const std::exception& e) {
            //return error("%s: Deserialize or I/O error - %s", __func__, e.what());
            return false;
        }
        filein.fclose();

        CDataStream ssPeers(vchData, SER_DISK, CLIENT_VERSION);

        // verify stored checksum matches input data
        uint256 hashTmp = Hash(ssPeers.begin(), ssPeers.end());
        if (hashIn != hashTmp)
            //return error("%s: Checksum mismatch, data corrupted", __func__);
            return false;

        unsigned char pchMsgTmp[4];
        try {
            // de-serialize file header (network specific magic number) and ..
            ssPeers >> FLATDATA(pchMsgTmp);

            // ... verify the network matches ours
            if (memcmp(pchMsgTmp, Params().MessageStart(), sizeof(pchMsgTmp)))
                //return error("%s: Invalid network magic number", __func__);
                return false;

            // de-serialize address data into one CAddrMan object
            ssPeers >> backupheight_;
            ssPeers >> *pcontainer_;
        } catch (const std::exception& e) {
            //return error("%s: Deserialize or I/O error - %s", __func__, e.what());
            return false;
        }

        return true;
    }

    typedef typename Tc::const_iterator CIterator;

    inline CIterator IteratorBegin()
    {
        return pcontainer_->begin();
    }

    inline CIterator IteratorEnd()
    {
        return pcontainer_->end();
    }

protected:
    // The pointer to the container.
    Tc *pcontainer_;
    // Disk backed up height.
    int backupheight_;
    // Filename on the disk.
    std::string filename_;
};

// namespace for the cache of alliance member
namespace alliance_member
{

namespace
{
typedef std::set<std::string> Tc_t;
typedef std::string Te_t;
}

/*!
 * @brief   The cache structure for alliance member.
 */
class AllianceMember : public CacheInterface<Tc_t, Te_t>
{
public:
    AllianceMember()
    {
        filename_ = "member.dat";
    }

    ~AllianceMember()
    {
        filename_ = "";
    }

    inline bool Add(const Te_t &addr)
    {
        pcontainer_->insert(addr);
        return true;
    }

    inline bool Remove(const Te_t &addr)
    {
        pcontainer_->erase(addr);
        return true;
    }

    inline bool RemoveAll()
    {
        pcontainer_->clear();
        return true;
    }

    /*!
     * @brief   Check if the given address is an alliance member.
     * @param   addr    The address to be checked.
     * @return  True if the address is an alliance member.
     */
    inline bool IsMember(const std::string &addr) const
    {
        return (pcontainer_->find(addr) != pcontainer_->end());
    }

    /*!
     * @brief   Check the amount of alliance member.
     */
    inline size_t NumOfMembers() const
    {
        return pcontainer_->size();
    }
};
}

// Namespace for the license
namespace color_license
{
/*!
 * @brief   The structure for owner of each color.
 */
struct Owner_
{
    // Owner address for the color.
    std::string address_;
    // Minted amount for the color.
    int64_t num_of_coins_;
    // License information for the color.
    CLicenseInfo info_;
    Owner_() : address_(""), num_of_coins_(0) {}

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(address_);
        READWRITE(num_of_coins_);
        READWRITE(info_);
    }
};

namespace
{
typedef std::map<type_Color, Owner_> Tc_t;
typedef std::pair<type_Color, Owner_> Te_t;
}

/*!
 * @brief   The structure for license.
 */
class ColorLicense : public CacheInterface<Tc_t, Te_t>
{
public:
    ColorLicense()
    {
        filename_ = "license.dat";
    }

    ~ColorLicense()
    {
        filename_ = "";
    }

    /*!
     * @brief   Remove the color from the cache.
     * @param   color   The color to be removed.
     */
    inline bool RemoveColor(const type_Color &color)
    {
        pcontainer_->erase(color);
        return true;
    }

    /*!
     * @brief   Remove the owner of the given color.
     * @param   color   The color of owner to be removed.
     */
    inline bool RemoveOwner(const type_Color &color)
    {
        (*pcontainer_)[color].address_ = "";
        return true;
    }

    inline bool RemoveAll()
    {
        pcontainer_->clear();
        return true;
    }

    /*!
     * @brief   Set the owner information of the color.
     * @param   color   The color to be processed.
     * @param   addr    The address to be assigned as the owner.
     * @param   pinfo   The pointer of the license information to be assigned.
     * @return  True if the process is successfully completed.
     */
    bool SetOwner(const type_Color &color, const std::string addr, const CLicenseInfo *pinfo = NULL);

    /*!
     * @brief   Get the owner information of the color.
     * @param   color   The color to be processed.
     * @return  address
     */
    string GetOwner(const type_Color &color) const;

    /*!
     * @brief   Add the amount of minted coins for the given color.
     * @param   color           The color to be processed.
     * @param   num_of_coins    The amount to be added.
     * @return  True if the process is successfully completed.
     */
    inline void AddNumOfCoins(const type_Color &color, int64_t num_of_coins)
    {
        (*pcontainer_)[color].num_of_coins_ += num_of_coins;
    }

    /*!
     * @brief   Check if the color exists.
     * @param   color   The color to be checked.
     * @return  True if the color exists.
     */
    bool IsColorExist(const type_Color &color) const;

    /*!
     * @brief   Check if the color has an owner.
     * @param   color   The color to be checked.
     * @return  True if the color has owner.
     */
    inline bool HasColorOwner(const type_Color &color) const
    {
        Tc_t::iterator it = pcontainer_->find(color);
        return (it != pcontainer_->end() && it->second.address_ != "");
    }

    /*!
     * @brief   Check if the address is the owner of the given color.
     * @param   color   The color to be checked.
     * @param   addr    The address to be checked.
     * @return  True of the address is the owner of the given color.
     */
    inline bool IsColorOwner(const type_Color &color, std::string &addr) const
    {
        Tc_t::iterator it = pcontainer_->find(color);
        return (it != pcontainer_->end() && it->second.address_ == addr);
    }

    /*!
     * @brief   Return the minted amount of coin of the given color.
     * @param   The color to be checked.
     * @retrun  The minted amount of the given color.
     */
    int64_t NumOfCoins(const type_Color &color) const;

    /*!
     * @brief   Return the entire license information.
     * @return  The list of information including color, owner address and minted amount.
     */
    std::map<type_Color, std::pair<std::string, int64_t> > ListLicense() const;

    /*!
     * @brief   Get the license information for the given color.
     * @param   color   The color to be checked.
     * @param   info    The referenced license information.
     * @return  True if the license information is successfullt fetched.
     */
    bool GetLicenseInfo(const type_Color &color, CLicenseInfo &info) const;

    /*!
     * @brief   Check if the given color is member-only.
     * @param   color   The color to be checked.
     * @return  True if the color is member-only.
     */
    inline bool IsMemberOnly(const type_Color &color) const
    {
        return (*pcontainer_)[color].info_.fMemberControl;
    }

    /*!
     * @brief   Check the upper limit of minting amount of the given color.
     * @param   color   The color to be checked.
     * @return  The amount of upper limit.
     */
    inline int64_t GetUpperLimit(const type_Color &color) const
    {
        return (*pcontainer_)[color].info_.nLimit;
    }
};
}

// Namespace of the block miner.
namespace block_miner
{
namespace
{
typedef std::list<std::pair<std::string, unsigned int> > Tc_t;
typedef std::string Te_t;
}

/*!
 * @brief   The structure of block miner.
 */
class BlockMiner : public CacheInterface<Tc_t, Te_t>
{
public:
    BlockMiner()
    {
        filename_ = "miner.dat";
    }

    ~BlockMiner()
    {
        filename_ = "";
    }

    bool Add(const Te_t &e);

    inline bool Remove()
    {
        if (!pcontainer_->empty())
            pcontainer_->pop_front();
        return true;
    }

    inline bool RemoveAll()
    {
        pcontainer_->clear();
        return true;
    }

    /*!
     * @brief   Check how many blocks are mined by the given address.
     * @param   addr        The address to be checked.
     * @param   nAlliance   The amount of alliance.
     * @return  The amount of blocks mined by the given miner.
     */
    unsigned int NumOfMined(std::string addr, unsigned int nAlliance) const;

};
}

// Namespace of activated address.
namespace activate_addr
{

namespace
{
typedef std::map<type_Color, std::map<std::string, int64_t> > Tc_t;
typedef std::pair<type_Color, std::string> Te_t;
}

/*!
 * @brief   The structure of activated addresses
 */
class ActivateAddr : public CacheInterface<Tc_t, Te_t >
{
public:
    ActivateAddr()
    {
        filename_ = "activate.dat";
    }

    ~ActivateAddr()
    {
        filename_ = "";
    }

    inline bool Add(const Te_t &e)
    {
        (*pcontainer_)[e.first][e.second] = 1;
        return true;
    }

    inline bool Remove(const std::pair<type_Color, std::string> &e)
    {
        (*pcontainer_)[e.first].erase(e.second);
        return true;
    }

    inline bool RemoveAll()
    {
        pcontainer_->clear();
        return true;
    }

    /*!
     * @brief   Remove the activated member list of the given color.
     * @param   color   The color to be removed.
     * @return  True if the remove process is successful.
     */
    inline bool RemoveColor(const type_Color &color)
    {
        pcontainer_->erase(color);
        return true;
    }

    /*!
     * @brief   Activate the given address with specific color.
     * @param   color   The color to activate.
     * @param   addr    The address to be activated.
     * @return  True if the activation is successful.
     */
    bool Activate(const type_Color &color, const std::string &addr);

    /*!
     * @brief   Deactivate the given address which was activated with specific color.
     * @param   color   The color to deactivate.
     * @param   addr    The address to be deactivated.
     * @return  True if the deactivation is successful.
     */
    bool Deactivate(const type_Color &color, const std::string &addr);

    /*!
     * @brief   Check if the color exists in the activation list.
     * @param   color   The color to be checked.
     * @return  True if the given color exists.
     */
    inline bool IsColorExist(const type_Color &color) const
    {
        return (pcontainer_->find(color) != pcontainer_->end());
    }

    /*!
     * @brief   Check if the given address is activated by the given color.
     * @param   color   The color to be checked.
     * @param   addr    The address to be checked.
     * @return  True if the address is activated with the given color.
     */
    inline bool IsActivated(const type_Color &color, const std::string &addr) const
    {
        Tc_t::iterator it;
        it = pcontainer_->find(color);
        return (IsColorExist(color) &&
                it->second.find(addr) != it->second.end());
    }
};

}

// Namespace of the orders.
namespace order_list
{

/*!
 * @brief   The structure of order information.
 */
struct order_info_
{
    uint256 hash;
    std::string address;
    int64_t buy_amount, sell_amount;

    order_info_() : address(""), buy_amount(0), sell_amount(0) { hash.SetNull(); }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        READWRITE(hash);
        READWRITE(address);
        READWRITE(buy_amount);
        READWRITE(sell_amount);
    }

    bool operator!=(const order_info_ &b) const{
        if (this->hash != b.hash)
            return true;
        if (this->address != b.address)
            return true;
        if (this->buy_amount != b.buy_amount)
            return true;
        if (this->sell_amount != b.sell_amount)
            return true;
        return false;
    }

    bool operator==(const order_info_ &b) const{
        return !(*this != b);
    }
};

namespace
{
typedef map<pair<type_Color, type_Color>, vector<order_info_> > Tc_t;
typedef TxInfo Te_t;
}

class OrderList : public CacheInterface<Tc_t, Te_t>
{
public:
    OrderList()
    {
        filename_ = "order.dat";
    }

    ~OrderList()
    {
        filename_ = "";
    }

    bool Remove(const Te_t &txinfo);

    inline bool RemoveAll()
    {
        pcontainer_->clear();
        return true;
    }

    void AddOrder(const TxInfo &txinfo);

    bool IsExist(const TxInfo &txinfo) const;

    std::vector<std::string> GetList() const;

};
}

extern alliance_member::AllianceMember *palliance;
extern color_license::ColorLicense *plicense;
extern block_miner::BlockMiner *pminer;
extern activate_addr::ActivateAddr *pactivate;
extern order_list::OrderList *porder;

#endif // GCOIN_CACHE_H
