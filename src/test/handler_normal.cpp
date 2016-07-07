// Copyright (c) 2014-2016 The Gcoin Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <stdint.h>

#include <map>
#include <string>
#include <iostream>

#include "test_bitcoin.h"
#include "policy/licenseinfo.h"

using namespace std;

BOOST_AUTO_TEST_SUITE(handler_normal_test)

struct NormalHandlerFixture : public TestingSetup
{
    NormalHandlerFixture()
    {
        handler = type_transaction_handler::GetHandler(NORMAL);
    }
    ~NormalHandlerFixture()
    {
        handler = NULL;
    }
};

struct NormalHandlerCheckValidFixture : public NormalHandlerFixture
{
    NormalHandlerCheckValidFixture()
    {
        in_hash = ArithToUint256(arith_uint256(1));
        member_hash = ArithToUint256(arith_uint256(3));
        out_hash = ArithToUint256(arith_uint256(4));

        color = 5;
        pinfo = new CLicenseInfo();
        pinfo->fMemberControl = true;
        plicense->SetOwner(color, CreateAddress(), pinfo);

        member = CreateAddress();
        receiver = CreateAddress();
        pactivate->Activate(color, receiver);
        CreateTransaction(in_hash, MINT);
        CreateTransaction(member_hash, NORMAL);
        CreateTransaction(out_hash, NORMAL);
        ConnectTransactions(in_hash, member_hash, COIN, member, color);
        ConnectTransactions(member_hash, out_hash, COIN, receiver, color);
    }

    ~NormalHandlerCheckValidFixture()
    {
        delete pinfo;
    }

    void CheckFalse(int ndos, const std::string &msg)
    {
        CValidationState state;
        bool ret;
        int v;

        ret = handler->CheckValid(
                CTransaction(transactions[member_hash]), state, NULL);
        ret &= handler->CheckFormat(
                CTransaction(transactions[member_hash]), state, NULL);

        BOOST_CHECK_MESSAGE(
                ret == false && state.IsInvalid(v) && v == ndos, msg);
    }

    uint256 in_hash, member_hash, out_hash;
    std::string member, receiver;
    type_Color color;
    CValidationState state;
    CLicenseInfo *pinfo;
};


BOOST_FIXTURE_TEST_CASE(NormalHandlerCheckValidPass, NormalHandlerCheckValidFixture)
{
    BOOST_CHECK(handler->CheckValid(
                CTransaction(transactions[member_hash]), state, NULL) == true);
}


BOOST_FIXTURE_TEST_CASE(NormalHandlerCheckValidInactivatedMember, NormalHandlerCheckValidFixture)
{
    pactivate->Deactivate(color, receiver);
    CheckFalse(10, __func__);
}


BOOST_FIXTURE_TEST_CASE(NormalHandlerCheckValidInactivatedColor, NormalHandlerCheckValidFixture)
{
    pactivate->RemoveColor(color);
    CheckFalse(100, __func__);
}


BOOST_FIXTURE_TEST_CASE(NormalHandlerApplyNoMemberOnly, NormalHandlerFixture)
{
    type_Color color = 5;
    uint256 hash1 = ArithToUint256(arith_uint256(1));
    uint256 hash2 = ArithToUint256(arith_uint256(2));
    uint256 hash3 = ArithToUint256(arith_uint256(3));
    std::string issuer = CreateAddress();
    std::string member = CreateAddress();
    CreateTransaction(hash1, MINT);
    CreateTransaction(hash2, NORMAL);
    CreateTransaction(hash3, NORMAL);
    ConnectTransactions(hash1, hash2, COIN, issuer, color);
    ConnectTransactions(hash2, hash3, COIN, member, color);

    CLicenseInfo *pinfo = new CLicenseInfo();
    BOOST_CHECK(plicense->SetOwner(color, issuer, pinfo));
    BOOST_CHECK(handler->Apply(CTransaction(transactions[hash2]), NULL));
    BOOST_CHECK(!pactivate->IsActivated(color, member));
    delete pinfo;
}


BOOST_FIXTURE_TEST_CASE(NormalHandlerApplyMemberOnly, NormalHandlerFixture)
{
    type_Color color = 5;
    uint256 hash1 = ArithToUint256(arith_uint256(1));
    uint256 hash2 = ArithToUint256(arith_uint256(2));
    uint256 hash3 = ArithToUint256(arith_uint256(3));
    std::string issuer = CreateAddress();
    std::string member = CreateAddress();
    CreateTransaction(hash1, MINT);
    CreateTransaction(hash2, NORMAL);
    CreateTransaction(hash3, NORMAL);
    ConnectTransactions(hash1, hash2, COIN, issuer, color);
    ConnectTransactions(hash2, hash3, COIN, member, color);

    CLicenseInfo *pinfo = new CLicenseInfo();
    pinfo->fMemberControl = true;
    BOOST_CHECK(plicense->SetOwner(color, issuer, pinfo));
    BOOST_CHECK(handler->Apply(CTransaction(transactions[hash2]), NULL));
    BOOST_CHECK(pactivate->IsActivated(color, member));
    delete pinfo;
}


BOOST_FIXTURE_TEST_CASE(NormalHandlerUndo, NormalHandlerFixture)
{
    CBlock tmp;
    std::string issuer = CreateAddress();
    std::string member = CreateAddress();
    type_Color color = 5;
    CLicenseInfo *pinfo = new CLicenseInfo();
    pinfo->fMemberControl = true;
    plicense->SetOwner(color, issuer, pinfo);
    pactivate->Activate(color, member);
    pactivate->Activate(color, member);
    uint256 hash1 = ArithToUint256(arith_uint256(1));
    uint256 hash2 = ArithToUint256(arith_uint256(2));
    uint256 hash3 = ArithToUint256(arith_uint256(3));

    CreateTransaction(hash1, MINT);
    CreateTransaction(hash2, NORMAL);
    CreateTransaction(hash3, NORMAL);
    ConnectTransactions(hash1, hash2, COIN, issuer, color);
    ConnectTransactions(hash2, hash3, COIN, member, color);
    BOOST_CHECK(handler->Undo(CTransaction(transactions[hash2]), &tmp) == true);
    BOOST_CHECK(pactivate->IsActivated(color, member));
    BOOST_CHECK(handler->Undo(CTransaction(transactions[hash2]), &tmp) == true);
    BOOST_CHECK(pactivate->IsActivated(color, member) == false);
    delete pinfo;
}

BOOST_AUTO_TEST_SUITE_END();
