// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2013 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rpcclient.h"

#include <stdint.h>

#include <set>
#include <stdexcept>
#include <string>
#include <utility>

#include "json/json_spirit_reader_template.h"

class CRPCConvertParam
{
public:
    std::string methodName;            // method whose params want conversion
    int paramIdx;                      // 0-based idx of param to convert
};

static const CRPCConvertParam vRPCConvertParams[] =
{
    { "stop", 0 },
    { "getaddednodeinfo", 0 },
    { "setgenerate", 0 },
    { "setgenerate", 1 },
    { "getnetworkhashps", 0 },
    { "getnetworkhashps", 1 },
    { "getrtts", 0 },
    { "getrtts", 1 },
    { "getrtts", 2 },
    { "gettotalbandwidth", 0 },
    { "gettotalbandwidth", 1 },
    { "gettotalbandwidth", 2 },
    { "sendtoaddress", 1 },
    { "sendtoaddress", 2 },
    { "settxfee", 0 },
    { "getreceivedbyaddress", 1 },
    { "getreceivedbyaccount", 1 },
    { "listreceivedbyaddress", 0 },
    { "listreceivedbyaddress", 1 },
    { "listreceivedbyaddress", 2 },
    { "listreceivedbyaccount", 0 },
    { "listreceivedbyaccount", 1 },
    { "listreceivedbyaccount", 2 },
    { "getbalance", 1 },
    { "getbalance", 2 },
    { "getaddressbalance", 1 },
    { "getcolorbalance", 0 },
    { "getcolorbalance", 2 },
    { "getcolorbalance", 3 },
    { "getblockhash", 0 },
    { "move", 2 },
    { "move", 3 },
    { "sendfrom", 2 },
    { "sendfrom", 3 },
    { "listtransactions", 1 },
    { "listtransactions", 2 },
    { "listtransactions", 3 },
    { "listwalletaddress", 1 },
    { "listonewalletaddress", 0 },
    { "listaccounts", 0 },
    { "listaccounts", 1 },
    { "walletpassphrase", 1 },
    { "getblocktemplate", 0 },
    { "listsinceblock", 1 },
    { "listsinceblock", 2 },
    { "sendvotetoaddress", 1 },
    { "sendlicensetoaddress", 1 },
    { "sendmany", 2 },
    { "sendmany", 3 },
    { "mint", 0 },
    { "mint", 1 },
    { "addmultisigaddress", 0 },
    { "addmultisigaddress", 1 },
    { "createmultisig", 0 },
    { "createmultisig", 1 },
    { "listunspent", 0 },
    { "listunspent", 1 },
    { "listunspent", 2 },
    { "getblock", 1 },
    { "getrawtransaction", 1 },
    { "createrawtransaction", 0 },
    { "createrawtransaction", 1 },
    { "createrawtransaction", 2 },
    { "signrawtransaction", 1 },
    { "signrawtransaction", 2 },
    { "sendrawtransaction", 1 },
    { "gettxout", 1 },
    { "gettxout", 2 },
    { "lockunspent", 0 },
    { "lockunspent", 1 },
    { "importprivkey", 2 },
    { "importaddress", 2 },
    { "verifychain", 0 },
    { "verifychain", 1 },
    { "keypoolrefill", 0 },
    { "getrawmempool", 0 },
    { "estimatefee", 0 },
    { "estimatepriority", 0 },
    { "prioritisetransaction", 1 },
    { "prioritisetransaction", 2 },
};

class CRPCConvertTable
{
private:
    std::set<std::pair<std::string, int> > members;

public:
    CRPCConvertTable();

    bool convert(const std::string& method, int idx) {
        return (members.count(std::make_pair(method, idx)) > 0);
    }
};

CRPCConvertTable::CRPCConvertTable()
{
    const unsigned int n_elem =
        (sizeof(vRPCConvertParams) / sizeof(vRPCConvertParams[0]));

    for (unsigned int i = 0; i < n_elem; i++) {
        members.insert(std::make_pair(vRPCConvertParams[i].methodName,
                                      vRPCConvertParams[i].paramIdx));
    }
}

static CRPCConvertTable rpcCvtTable;

// Convert strings to command-specific RPC representation
json_spirit::Array RPCConvertValues(const std::string &strMethod, const std::vector<std::string> &strParams)
{
    json_spirit::Array params;

    for (unsigned int idx = 0; idx < strParams.size(); idx++) {
        const std::string& strVal = strParams[idx];

        // insert string value directly
        if (!rpcCvtTable.convert(strMethod, idx))
            params.push_back(strVal);
        // parse string as JSON, insert bool/number/object/etc. value
        else {
            json_spirit::Value jVal;
            if (!read_string(strVal, jVal))
                throw std::runtime_error(std::string("Error parsing JSON:") + strVal);
            params.push_back(jVal);
        }
    }

    return params;
}
