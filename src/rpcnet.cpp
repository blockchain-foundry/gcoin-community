// Copyright (c) 2009-2014 The Bitcoin Core developers
// Copyright (c) 2014-2016 The Gcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rpcserver.h"

#include "cache.h"
#include "clientversion.h"
#include "main.h"
#include "net.h"
#include "netbase.h"
#include "protocol.h"
#include "sync.h"
#include "timedata.h"
#include "util.h"
#include "version.h"

#include <vector>
#include <sstream>
#include <time.h>
#include <boost/foreach.hpp>

#include "json/json_spirit_value.h"

using namespace json_spirit;
using namespace std;
using alliance_member::AllianceMember;

Value getconnectioncount(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw std::runtime_error(
            _(__func__) + "\n"
            "\nReturns the number of connections to other nodes.\n"
            "\nbResult:\n"
            "n          (numeric) The connection count\n"
            "\nExamples:\n"
            + HelpExampleCli("getconnectioncount", "")
            + HelpExampleRpc("getconnectioncount", "")
        );

    LOCK2(cs_main, cs_vNodes);

    return (int)vNodes.size();
}

Value ping(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw std::runtime_error(
            _(__func__) + "\n"
            "\nRequests that a ping be sent to all other nodes, to measure ping time.\n"
            "Results provided in getpeerinfo, pingtime and pingwait fields are decimal seconds.\n"
            "Ping command is handled in queue with all other commands, so it measures processing backlog, not just network ping.\n"
            "\nExamples:\n"
            + HelpExampleCli("ping", "")
            + HelpExampleRpc("ping", "")
        );

    // Request that each node send a ping during next message processing pass
    LOCK2(cs_main, cs_vNodes);

    BOOST_FOREACH(CNode* pNode, vNodes) {
        pNode->fPingQueued = true;
    }

    return Value::null;
}

static void CopyNodeStats(std::vector<CNodeStats>& vstats)
{
    vstats.clear();

    LOCK(cs_vNodes);
    vstats.reserve(vNodes.size());
    BOOST_FOREACH(CNode* pnode, vNodes) {
        CNodeStats stats;
        pnode->copyStats(stats);
        vstats.push_back(stats);
    }
}

Value getpeerinfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw std::runtime_error(
            _(__func__) + "\n"
            "\nReturns data about each connected network node as a json array of objects.\n"
            "\nbResult:\n"
            "[\n"
            "  {\n"
            "    \"id\": n,                   (numeric) Peer index\n"
            "    \"addr\":\"host:port\",      (string) The ip address and port of the peer\n"
            "    \"addrlocal\":\"ip:port\",   (string) local address\n"
            "    \"services\":\"xxxxxxxxxxxxxxxx\",   (string) The services offered\n"
            "    \"lastsend\": ttt,           (numeric) The time in seconds since epoch (Jan 1 1970 GMT) of the last send\n"
            "    \"lastrecv\": ttt,           (numeric) The time in seconds since epoch (Jan 1 1970 GMT) of the last receive\n"
            "    \"bytessent\": n,            (numeric) The total bytes sent\n"
            "    \"bytesrecv\": n,            (numeric) The total bytes received\n"
            "    \"conntime\": ttt,           (numeric) The connection time in seconds since epoch (Jan 1 1970 GMT)\n"
            "    \"timeoffset\": ttt,         (numeric) The time offset in seconds\n"
            "    \"pingtime\": n,             (numeric) ping time\n"
            "    \"pingwait\": n,             (numeric) ping wait\n"
            "    \"version\": v,              (numeric) The peer version, such as 7001\n"
            "    \"subver\": \"/Satoshi:0.8.5/\",  (string) The string version\n"
            "    \"inbound\": true|false,     (boolean) Inbound (true) or Outbound (false)\n"
            "    \"startingheight\": n,       (numeric) The starting height (block) of the peer\n"
            "    \"banscore\": n,             (numeric) The ban score\n"
            "    \"synced_headers\": n,       (numeric) The last header we have in common with this peer\n"
            "    \"synced_blocks\": n,        (numeric) The last block we have in common with this peer\n"
            "    \"inflight\": [\n"
            "       n,                        (numeric) The heights of blocks we're currently asking from this peer\n"
            "       ...\n"
            "    ]\n"
            "  }\n"
            "  ,...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("getpeerinfo", "")
            + HelpExampleRpc("getpeerinfo", "")
        );

    LOCK(cs_main);

    vector<CNodeStats> vstats;
    CopyNodeStats(vstats);

    Array ret;

    BOOST_FOREACH(const CNodeStats& stats, vstats) {
        Object obj;
        CNodeStateStats statestats;
        bool fStateStats = GetNodeStateStats(stats.nodeid, statestats);
        obj.push_back(Pair("id", stats.nodeid));
        obj.push_back(Pair("addr", stats.addrName));
        if (!(stats.addrLocal.empty()))
            obj.push_back(Pair("addrlocal", stats.addrLocal));
        obj.push_back(Pair("services", strprintf("%016x", stats.nServices)));
        obj.push_back(Pair("lastsend", stats.nLastSend));
        obj.push_back(Pair("lastrecv", stats.nLastRecv));
        obj.push_back(Pair("bytessent", stats.nSendBytes));
        obj.push_back(Pair("bytesrecv", stats.nRecvBytes));
        obj.push_back(Pair("conntime", stats.nTimeConnected));
        obj.push_back(Pair("timeoffset", stats.nTimeOffset));
        obj.push_back(Pair("pingtime", stats.dPingTime));
        if (stats.dPingWait > 0.0)
            obj.push_back(Pair("pingwait", stats.dPingWait));
        obj.push_back(Pair("version", stats.nVersion));
        // Use the sanitized form of subver here, to avoid tricksy remote peers from
        // corrupting or modifiying the JSON output by putting special characters in
        // their ver message.
        obj.push_back(Pair("subver", stats.cleanSubVer));
        obj.push_back(Pair("inbound", stats.fInbound));
        obj.push_back(Pair("startingheight", stats.nStartingHeight));
        if (fStateStats) {
            obj.push_back(Pair("banscore", statestats.nMisbehavior));
            obj.push_back(Pair("synced_headers", statestats.nSyncHeight));
            obj.push_back(Pair("synced_blocks", statestats.nCommonHeight));
            Array heights;
            BOOST_FOREACH(int height, statestats.vHeightInFlight) {
                heights.push_back(height);
            }
            obj.push_back(Pair("inflight", heights));
        }
        obj.push_back(Pair("whitelisted", stats.fWhitelisted));

        ret.push_back(obj);
    }

    return ret;
}

Value bannode(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw std::runtime_error(
            _(__func__) + " \"nodeid\" \n"
            "\nAttempts ban a node by node id.\n"
            "\nArguments:\n"
            "1. \"nodeid\"     (numeric, required) The node's index number in the local node database (see getpeerinfo for nodes)\n"
            "2. \"nodeid\"     (optional, required) The ban score you want to increase or decrease\n"
            "\nExamples:\n"
            + HelpExampleCli("bannode", "\"192.168.0.6:8333\"")
            + HelpExampleCli("bannode", "\"192.168.0.6:8333, 100\"")
        );

    NodeId nodeid = params[0].get_int();

    int howmuch = GetArg("-banscore", 100); 
    if (params.size() > 1)
        howmuch = params[1].get_int();

    Misbehaving(nodeid, howmuch);

    return Value::null;
}

Value permitnode(const Array& params, bool fHelp)
{
    std::string strCommand;
    if (fHelp || params.size() != 1)
        throw std::runtime_error(
            _(__func__) + " \"node\" \"add|remove|onetry\"\n"
            "\nAttempts remove a node from the bannode list.\n"
            "\nArguments:\n"
            "1. \"node\"     (string, required) The node (see getpeerinfo for nodes)\n"
            "\nExamples:\n"
            + HelpExampleCli("permitnode", "\"192.168.0.6:8333\"")
        );

    std::string strNode = params[0].get_str();

    CNetAddr addr(strNode);
    if (!CNode::RemoveFromBannedList(addr))
        throw JSONRPCError(RPC_CLIENT_NODE_NOT_BANNED, "Error: Node not exist in banlist");

    return Value::null;
}

Value addnode(const Array& params, bool fHelp)
{
    std::string strCommand;
    if (params.size() == 2)
        strCommand = params[1].get_str();
    if (fHelp || params.size() != 2 ||
        (strCommand != "onetry" && strCommand != "add" && strCommand != "remove"))
        throw std::runtime_error(
            _(__func__) + " \"node\" \"add|remove|onetry\"\n"
            "\nAttempts add or remove a node from the addnode list.\n"
            "Or try a connection to a node once.\n"
            "\nArguments:\n"
            "1. \"node\"     (string, required) The node (see getpeerinfo for nodes)\n"
            "2. \"command\"  (string, required) 'add' to add a node to the list, 'remove' to remove a node from the list, 'onetry' to try a connection to the node once\n"
            "\nExamples:\n"
            + HelpExampleCli("addnode", "\"192.168.0.6:8333\" \"onetry\"")
            + HelpExampleRpc("addnode", "\"192.168.0.6:8333\", \"onetry\"")
        );

    std::string strNode = params[0].get_str();

    if (strCommand == "onetry") {
        CAddress addr;
        OpenNetworkConnection(addr, NULL, strNode.c_str());
        return Value::null;
    }

    LOCK(cs_vAddedNodes);
    std::vector<std::string>::iterator it = vAddedNodes.begin();
    for(; it != vAddedNodes.end(); it++)
        if (strNode == *it)
            break;

    if (strCommand == "add") {
        if (it != vAddedNodes.end())
            throw JSONRPCError(RPC_CLIENT_NODE_ALREADY_ADDED, "Error: Node already added");
        vAddedNodes.push_back(strNode);
    } else if(strCommand == "remove") {
        if (it == vAddedNodes.end())
            throw JSONRPCError(RPC_CLIENT_NODE_NOT_ADDED, "Error: Node has not been added.");
        vAddedNodes.erase(it);
    }

    return Value::null;
}

Value disconnectnode(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 1)
        throw runtime_error(
            "disconnectnode \"node\" \n"
            "\nImmediately disconnects from the specified node.\n"
            "\nArguments:\n"
            "1. \"node\"     (string, required) The node (see getpeerinfo for nodes)\n"
            "\nExamples:\n"
            + HelpExampleCli("disconnectnode", "\"192.168.0.6:8333\"")
            + HelpExampleRpc("disconnectnode", "\"192.168.0.6:8333\"")
        );

    CNode* pNode = FindNode(params[0].get_str());
    if (pNode == NULL)
        throw JSONRPCError(RPC_CLIENT_NODE_NOT_CONNECTED, "Node not found in connected nodes");

    pNode->CloseSocketDisconnect();

    return Value::null;
}

Value getaddednodeinfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() < 1 || params.size() > 2)
        throw std::runtime_error(
            _(__func__) + " dns ( \"node\" )\n"
            "\nReturns information about the given added node, or all added nodes\n"
            "(note that onetry addnodes are not listed here)\n"
            "If dns is false, only a list of added nodes will be provided,\n"
            "otherwise connected information will also be available.\n"
            "\nArguments:\n"
            "1. dns        (boolean, required) If false, only a list of added nodes will be provided, otherwise connected information will also be available.\n"
            "2. \"node\"   (string, optional) If provided, return information about this specific node, otherwise all nodes are returned.\n"
            "\nResult:\n"
            "[\n"
            "  {\n"
            "    \"addednode\" : \"192.168.0.201\",   (string) The node ip address\n"
            "    \"connected\" : true|false,          (boolean) If connected\n"
            "    \"addresses\" : [\n"
            "       {\n"
            "         \"address\" : \"192.168.0.201:8333\",  (string) The gcoin server host and port\n"
            "         \"connected\" : \"outbound\"           (string) connection, inbound or outbound\n"
            "       }\n"
            "       ,...\n"
            "     ]\n"
            "  }\n"
            "  ,...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("getaddednodeinfo", "true")
            + HelpExampleCli("getaddednodeinfo", "true \"192.168.0.201\"")
            + HelpExampleRpc("getaddednodeinfo", "true, \"192.168.0.201\"")
        );

    bool fDns = params[0].get_bool();

    std::list<std::string> laddedNodes(0);
    if (params.size() == 1) {
        LOCK(cs_vAddedNodes);
        BOOST_FOREACH(std::string& strAddNode, vAddedNodes)
            laddedNodes.push_back(strAddNode);
    } else {
        std::string strNode = params[1].get_str();
        LOCK(cs_vAddedNodes);
        BOOST_FOREACH(std::string& strAddNode, vAddedNodes)
            if (strAddNode == strNode) {
                laddedNodes.push_back(strAddNode);
                break;
            }
        if (laddedNodes.size() == 0)
            throw JSONRPCError(RPC_CLIENT_NODE_NOT_ADDED, "Error: Node has not been added.");
    }

    Array ret;
    if (!fDns) {
        BOOST_FOREACH(std::string& strAddNode, laddedNodes) {
            Object obj;
            obj.push_back(Pair("addednode", strAddNode));
            ret.push_back(obj);
        }
        return ret;
    }

    std::list<std::pair<std::string, std::vector<CService> > > laddedAddreses(0);
    BOOST_FOREACH(std::string& strAddNode, laddedNodes) {
        std::vector<CService> vservNode(0);
        if(Lookup(strAddNode.c_str(), vservNode, Params().GetDefaultPort(), fNameLookup, 0))
            laddedAddreses.push_back(make_pair(strAddNode, vservNode));
        else {
            Object obj;
            obj.push_back(Pair("addednode", strAddNode));
            obj.push_back(Pair("connected", false));
            Array addresses;
            obj.push_back(Pair("addresses", addresses));
        }
    }

    LOCK(cs_vNodes);
    for (std::list<std::pair<std::string, std::vector<CService> > >::iterator it = laddedAddreses.begin();
        it != laddedAddreses.end(); it++) {
        Object obj;
        obj.push_back(Pair("addednode", it->first));

        Array addresses;
        bool fConnected = false;
        BOOST_FOREACH(CService& addrNode, it->second) {
            bool fFound = false;
            Object node;
            node.push_back(Pair("address", addrNode.ToString()));
            BOOST_FOREACH(CNode* pnode, vNodes)
                if (pnode->addr == addrNode) {
                    fFound = true;
                    fConnected = true;
                    node.push_back(Pair("connected", pnode->fInbound ? "inbound" : "outbound"));
                    break;
                }
            if (!fFound)
                node.push_back(Pair("connected", "false"));
            addresses.push_back(node);
        }
        obj.push_back(Pair("connected", fConnected));
        obj.push_back(Pair("addresses", addresses));
        ret.push_back(obj);
    }

    return ret;
}

Value getnettotals(const Array& params, bool fHelp)
{
    if (fHelp || params.size() > 0)
        throw std::runtime_error(
            _(__func__) + "\n"
            "\nReturns information about network traffic, including bytes in, bytes out,\n"
            "and current time.\n"
            "\nResult:\n"
            "{\n"
            "  \"totalbytesrecv\": n,   (numeric) Total bytes received\n"
            "  \"totalbytessent\": n,   (numeric) Total bytes sent\n"
            "  \"timemillis\": t        (numeric) Total cpu time\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getnettotals", "")
            + HelpExampleRpc("getnettotals", "")
       );

    Object obj;
    obj.push_back(Pair("totalbytesrecv", CNode::GetTotalBytesRecv()));
    obj.push_back(Pair("totalbytessent", CNode::GetTotalBytesSent()));
    obj.push_back(Pair("timemillis", GetTimeMillis()));
    return obj;
}

// list the member list.
Value getmemberlist(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw std::runtime_error(
            _(__func__) + "\n"
            "\nGet the alliance members' address in the network.\n"
            "\nResult:\n"
            "\n"
            "{\n"
            "  \"memberlist\": [        (array) Member addresses\n"
            "       \"address\":str,    (string) an address of a member\n"
            "       ...\n"
            "   ]\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getmemberlist", "")
            + HelpExampleRpc("getmemberlist", "")
       );

    LOCK(cs_main);

    Object obj;
    Array a;
    for (AllianceMember::CIterator it = palliance->IteratorBegin();
         it != palliance->IteratorEnd(); ++it)
        a.push_back((*it));
    obj.push_back(Pair("member_list", a));
    return obj;
}

Value getrtts(const Array& params, bool fHelp)
{
    do {
        if (fHelp || (params.size() != 4 && params.size() != 3))
            break;
        int64_t startTime = params[0].get_int64();
        int64_t endTime = params[1].get_int64();
        int64_t period = params[2].get_int64();
        std::string add =
            params.size() == 4 ? params[3].get_str() : std::string("");
        if (startTime < 0 || endTime < 0 || startTime >= endTime || period <= 0)
            break;
        return CNetRecorder::QueryRTT(add, startTime, endTime, period, 0.001);
    } while (false);
    // TODO(???): The document should not be at here!!
    //         Currently the json object to return is not created here, and we
    //         should not seperately the document and the creater :(
    //         (Currently the json object to return is created by CNetRecorder,
    //         but I think it's a bad idea...)
    std::string err_str;
    err_str += __func__;
    err_str += "\n\n";
    err_str += "Returns the average RTT of a specified node in a period of time.\n";
    err_str += "\n";
    err_str += "Result format:\n";
    err_str += "{\n";
    err_str += "  \"since\": n,                  (numeric) start time\n";
    err_str += "  \"until\": n,                  (numeric) end time\n";
    err_str += "  \"unit\": n,                   (numeric) unit\n";
    err_str += "  \"node_num\": n,               (numeric) number of nodes detected\n";
    err_str += "  \"node_id\": str,              (optional, string) address of the specified node\n";
    err_str += "  \"rtts\": [                    (array) average rtt of each period\n";
    err_str += "    {\n";
    err_str += "      \"time\": n,               (numeric) start time of this period\n";
    err_str += "      \"rtt\": f                 (floating) average rtt\n";
    err_str += "    },\n";
    err_str += "    ...\n";
    err_str += "  ]\n";
    err_str += "}\n";
    err_str += HelpExampleCli("getrtts", " <time_since> <time_until> <time_unit> [ <node_address> ]");
    err_str += HelpExampleRpc("getrtts", " <time_since> <time_until> <time_unit> [ <node_address> ]");
    throw std::runtime_error(err_str);
}

Value gettotalbandwidth(const Array& params, bool fHelp)
{
    do {
        if (fHelp || (params.size() != 3 && params.size() != 4))
            break;

        int64_t startTime = params[0].get_int64();
        int64_t endTime = params[1].get_int64();
        int64_t period = params[2].get_int64();
        std::string add =
                params.size() == 4 ? params[3].get_str() : std::string("");
        if (startTime < 0 || endTime < 0 || startTime >= endTime || period <= 0)
            break;

        return CNetRecorder::QueryBandwidth(add, startTime, endTime, period, 1.0);
    } while (false);
    // TODO(???): The document should not be at here!!
    //         Currently the json object to return is not created here, and we
    //         should not seperately the document and the creater :(
    //         (Currently the json object to return is created by CNetRecorder,
    //         but I think it's a bad idea...)
    std::string err_str;
    err_str += __func__;
    err_str += "\n\n";
    err_str += "Returns the total bandwidth of a specified node in a period of time.\n";
    err_str += "\n";
    err_str += "Result format:\n";
    err_str += "{\n";
    err_str += "  \"since\": n,                  (numeric) start time\n";
    err_str += "  \"until\": n,                  (numeric) end time\n";
    err_str += "  \"unit\": n,                   (numeric) unit\n";
    err_str += "  \"node_num\": n,               (numeric) number of nodes detected\n";
    err_str += "  \"node_id\": str,              (optional, string) address of the specified node\n";
    err_str += "  \"bandwidth\": [               (array) average rtt of each period\n";
    err_str += "    {\n";
    err_str += "      \"time\": n,               (numeric) start time of this period\n";
    err_str += "      \"bandwidth\": f           (floating) total bandwidth\n";
    err_str += "    },\n";
    err_str += "    ...\n";
    err_str += "  ]\n";
    err_str += "}\n";
    err_str += HelpExampleCli("gettotalbandwidth", " <time_since> <time_until> <time_unit> [ <node_address> ]");
    err_str += HelpExampleRpc("gettotalbandwidth", " <time_since> <time_until> <time_unit> [ <node_address> ]");
    throw std::runtime_error(err_str);
}

Value getorderlist(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw std::runtime_error(
            _(__func__) + "\n"
            "\nGet all order transaction's hash in the network.\n"
            "\nResult:\n"
            "\n"
            "{\n"
            "  \"order_list\":str,    (string) an information of an order\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getorderlist", "")
            + HelpExampleRpc("getorderlist", "")
       );

    Object obj;
    std::vector<std::string> orders = porder->GetList();
    for (unsigned int i = 0; i < orders.size(); i++)
        obj.push_back(Pair("order_list", orders[i]));

    return obj;
}


static Array GetNetworksInfo()
{
    Array networks;
    for(int n=0; n<NET_MAX; ++n) {
        enum Network network = static_cast<enum Network>(n);
        if(network == NET_UNROUTABLE)
            continue;
        proxyType proxy;
        Object obj;
        GetProxy(network, proxy);
        obj.push_back(Pair("name", GetNetworkName(network)));
        obj.push_back(Pair("limited", IsLimited(network)));
        obj.push_back(Pair("reachable", IsReachable(network)));
        obj.push_back(Pair("proxy", proxy.IsValid() ? proxy.proxy.ToStringIPPort() : string()));
        obj.push_back(Pair("proxy_randomize_credentials", proxy.randomize_credentials));
        networks.push_back(obj);
    }
    return networks;
}

Value getnetworkinfo(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw std::runtime_error(
            _(__func__) + "\n"
            "Returns an object containing various state info regarding P2P networking.\n"
            "\nResult:\n"
            "{\n"
            "  \"version\": xxxxx,                      (numeric) the server version\n"
            "  \"subversion\": \"/Satoshi:x.x.x/\",     (string) the server subversion string\n"
            "  \"protocolversion\": xxxxx,              (numeric) the protocol version\n"
            "  \"localservices\": \"xxxxxxxxxxxxxxxx\", (string) the services we offer to the network\n"
            "  \"timeoffset\": xxxxx,                   (numeric) the time offset\n"
            "  \"connections\": xxxxx,                  (numeric) the number of connections\n"
            "  \"networks\": [                          (array) information per network\n"
            "  {\n"
            "    \"name\": \"xxx\",                     (string) network (ipv4, ipv6 or onion)\n"
            "    \"limited\": true|false,               (boolean) is the network limited using -onlynet?\n"
            "    \"reachable\": true|false,             (boolean) is the network reachable?\n"
            "    \"proxy\": \"host:port\"               (string) the proxy that is used for this network, or empty if none\n"
            "  }\n"
            "  ,...\n"
            "  ],\n"
            "  \"relayfee\": x.xxxxxxxx,                (numeric) minimum relay fee for non-free transactions in btc/kb\n"
            "  \"localaddresses\": [                    (array) list of local addresses\n"
            "  {\n"
            "    \"address\": \"xxxx\",                 (string) network address\n"
            "    \"port\": xxx,                         (numeric) network port\n"
            "    \"score\": xxx                         (numeric) relative score\n"
            "  }\n"
            "  ,...\n"
            "  ]\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("getnetworkinfo", "")
            + HelpExampleRpc("getnetworkinfo", "")
        );

    LOCK(cs_main);

    Object obj;
    obj.push_back(Pair("version",       CLIENT_VERSION));
    obj.push_back(Pair("subversion",
        FormatSubVersion(CLIENT_NAME, CLIENT_VERSION, std::vector<string>())));
    obj.push_back(Pair("protocolversion",PROTOCOL_VERSION));
    obj.push_back(Pair("localservices",       strprintf("%016x", nLocalServices)));
    obj.push_back(Pair("timeoffset",    GetTimeOffset()));
    obj.push_back(Pair("connections",   (int)vNodes.size()));
    obj.push_back(Pair("networks",      GetNetworksInfo()));
    obj.push_back(Pair("relayfee",      ValueFromAmount(::minRelayTxFee.GetFeePerK())));
    Array localAddresses;
    {
        LOCK(cs_mapLocalHost);
        BOOST_FOREACH(const PAIRTYPE(CNetAddr, LocalServiceInfo) &item, mapLocalHost) {
            Object rec;
            rec.push_back(Pair("address", item.first.ToString()));
            rec.push_back(Pair("port", item.second.nPort));
            rec.push_back(Pair("score", item.second.nScore));
            localAddresses.push_back(rec);
        }
    }
    obj.push_back(Pair("localaddresses", localAddresses));
    return obj;
}

Value getbanlist(const Array& params, bool fHelp)
{
    if (fHelp || params.size() != 0)
        throw std::runtime_error(
            _(__func__) + "\n"
            "\nReturns ip and ban time of banned list as a json array of objects.\n"
            "\nbResult:\n"
            "[\n"
            "  {\n"
            "    \"addr\":\"ip\",      (string) The ip address of the banned node\n"
            "    \"time\": n,              (numeric) The ban time\n"
            "  }\n"
            "  ,...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("getbanlist", "")
            + HelpExampleRpc("getbanlist", "")
        );


    Array ret;
    std::vector<std::pair<CNetAddr, int64_t> > bannedlist = CNode::GetBannedList();
    BOOST_FOREACH (const PAIRTYPE(CNetAddr, int64_t) &banned, bannedlist) {
        Object obj;
        obj.push_back(Pair("addr", banned.first.ToString()));
        obj.push_back(Pair("ban time", banned.second));

        ret.push_back(obj);
    }

    return ret;
}
