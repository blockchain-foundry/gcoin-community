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
block_miner::BlockMiner *pminer = NULL;
activate_addr::ActivateAddr *pactivate = NULL;
order_list::OrderList *porder = NULL;

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
    pcontainer_->push_front(make_pair(addr, palliance->NumOfMembers()));
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

// Namespace for cache of activated addresses.
namespace activate_addr
{

bool ActivateAddr::Activate(const type_Color &color, const std::string &addr)
{
    // Use a counter to justify whether the transaction is the one that activates the receiver.
    if (IsActivated(color, addr))
        (*pcontainer_)[color][addr]++;
    else
        Add(make_pair(color, addr));
    return true;
}

bool ActivateAddr::Deactivate(const type_Color &color, const std::string &addr)
{
    if (!IsActivated(color, addr))
        return false;
    (*pcontainer_)[color][addr]--;
    if ((*pcontainer_)[color][addr] == 0)
        Remove(make_pair(color, addr));
    else
        return false;
    return true;
}

}

// Namespace for cache of orders
namespace order_list
{

bool OrderList::Remove(const Te_t &txinfo)
{
    pair<type_Color, type_Color> order_color(txinfo.GetTxOutColorOfIndex(1), txinfo.GetTxOutColorOfIndex(0));

    if (pcontainer_->find(order_color) == pcontainer_->end())
        return true;

    struct order_info_ order_info;
    order_info.hash = txinfo.GetTxHash();
    order_info.address = txinfo.GetTxOutAddressOfIndex(1);
    order_info.buy_amount = txinfo.GetTxOutValueOfIndex(1);
    order_info.sell_amount = txinfo.GetTxOutValueOfIndex(0);

    Tc_t::iterator it_tmp = pcontainer_->find(order_color);
    if (it_tmp == pcontainer_->end())
        return true;
    vector<order_info_> InfoVec = it_tmp->second;
    for (vector<order_info_>::iterator it = InfoVec.begin(); it != InfoVec.end(); it++) {
        if (*it != order_info)
            continue;
        InfoVec.erase(it);
        break;
    }
    if (InfoVec.size() == 0)
        pcontainer_->erase(order_color);
    return true;
}

void OrderList::AddOrder(const TxInfo &txinfo)
{
    pair<type_Color, type_Color> order_color(txinfo.GetTxOutColorOfIndex(1), txinfo.GetTxOutColorOfIndex(0));

    struct order_info_ order_info;
    order_info.hash = txinfo.GetTxHash();
    order_info.address = txinfo.GetTxOutAddressOfIndex(1);
    order_info.buy_amount = txinfo.GetTxOutValueOfIndex(1);
    order_info.sell_amount = txinfo.GetTxOutValueOfIndex(0);

    (*pcontainer_)[order_color].push_back(order_info);
}

bool OrderList::IsExist(const TxInfo &txinfo) const
{
    pair<type_Color, type_Color> order_color(txinfo.GetTxOutColorOfIndex(1), txinfo.GetTxOutColorOfIndex(0));

    struct order_info_ order_info;
    order_info.hash = txinfo.GetTxHash();
    order_info.address = txinfo.GetTxOutAddressOfIndex(1);
    order_info.buy_amount = txinfo.GetTxOutValueOfIndex(1);
    order_info.sell_amount = txinfo.GetTxOutValueOfIndex(0);

    if (pcontainer_->find(order_color) == pcontainer_->end())
        return false;

    for (vector<order_info_>::iterator it = (*pcontainer_)[order_color].begin(); it != (*pcontainer_)[order_color].end(); it++) {
        if (*it == order_info)
            return true;
    }

    return false;
}

vector<string> OrderList::GetList() const
{
    vector<string> List;
    for (Tc_t::const_iterator it = pcontainer_->begin();
         it != pcontainer_->end(); it++) {
        for (vector<order_info_>::const_iterator itvec = it->second.begin(); itvec != it->second.end(); itvec++) {
            std::stringstream out;
            out << "hash: " << itvec->hash.ToString() << " color:" << it->first.second << " amount:" << itvec->sell_amount << " for color:" << it->first.first <<" amount:"<< itvec->buy_amount;
            List.push_back(out.str());
        }
    }
    return List;

}

}
