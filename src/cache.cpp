// Copyright (c) 2014-2016 The Gcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "cache.h"
#include "main.h"
#include "policy/licenseinfo.h"
#include "util.h"
#include "utilerror.h"

using std::set;
using std::string;
using std::map;
using std::pair;
using std::list;
using std::exception;
using std::vector;

alliance_member::AllianceMember *palliance = NULL;
color_license::ColorLicense *plicense = NULL;
block_miner::BlockMiner *pblkminer = NULL;
miner::Miner *pminer = NULL;

// Namespace for cache of license structure.
namespace color_license
{

bool ColorLicense::SetOwner(const type_Color &color, const string addr, const CLicenseInfo *pinfo)
{
    // If the owner is assigned for the first time, license info should be assigned at the same time.
    // If the license already exists, license info should not be assigned.
    if (IsColorExist(color)) {
        if (pinfo)
            return false;
    } else {
        if (pinfo)
            (*pcontainer_)[color].info_ = *pinfo;
        else
            return false;
    }
    (*pcontainer_)[color].address_ = addr;
    return true;
}

string ColorLicense::GetOwner(const type_Color &color) const
{
    return (*pcontainer_)[color].address_;
}

bool ColorLicense::IsColorExist(const type_Color &color) const
{
    if (color == DEFAULT_ADMIN_COLOR)
        return true;
    return (pcontainer_->find(color) != pcontainer_->end());
}

int64_t ColorLicense::NumOfCoins(const type_Color &color) const
{
    Tc_t::iterator it = pcontainer_->find(color);
    if (it == pcontainer_->end()) {
        return 0;
    }
    return it->second.num_of_coins_;
}

map<type_Color, pair<string, int64_t> > ColorLicense::ListLicense() const
{
    map<type_Color, pair<string, int64_t> > list;
    for (Tc_t::const_iterator it = pcontainer_->begin(); it != pcontainer_->end(); it++) {
        list[it->first] = make_pair(it->second.address_, it->second.num_of_coins_);
    }
    return list;
}

bool ColorLicense::GetLicenseInfo(const type_Color &color, CLicenseInfo &info) const
{
    Tc_t::iterator it = pcontainer_->find(color);
    if (it != pcontainer_->end()) {
        info = it->second.info_;
        return true;
    } else
        return false;
}

}

// Namespace for cache of block miners.
namespace block_miner
{
bool BlockMiner::Add(const string &addr)
{
    while (pcontainer_->size() >= 100) pcontainer_->pop_back();
    pcontainer_->push_front(make_pair(addr, pminer->NumOfMiners()));
    return true;
}

unsigned int BlockMiner::NumOfMined(string addr, unsigned int nAlliance) const
{
    unsigned int count = 1, nSameMiner = 0;
    for (list<pair<string, unsigned int> >::iterator it = pcontainer_->begin();
         count <= Params().DynamicMiner() && count < nAlliance && it != pcontainer_->end(); it++) {
        if (it->first == addr) nSameMiner++;
        count++;
    }
    return nSameMiner;
}
}
