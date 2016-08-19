// Copyright (c) 2014-2016 The Gcoin Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <stdint.h>

#include <map>
#include <string>
#include <iostream>
#include "policy/licenseinfo.h"

#include "test_gcoin.h"

using namespace std;

BOOST_AUTO_TEST_SUITE(handler_license_test)

struct LicenseHandlerFixture : public TestingSetup
{
    LicenseHandlerFixture()
    {
        handler = type_transaction_handler::GetHandler(LICENSE);
    }
    ~LicenseHandlerFixture()
    {
        handler = NULL;
    }
};

struct CreateLicenseHandlerCheckValidFixture : public LicenseHandlerFixture
{
    CreateLicenseHandlerCheckValidFixture()
    {
        mint_admin_hash = ArithToUint256(arith_uint256(1));
        license_hash = ArithToUint256(arith_uint256(2));
        out_hash = ArithToUint256(arith_uint256(3));

        color = 5;

        member = CreateAddress();
        issuer = CreateAddress();

        palliance->Add(member);
        CreateTransaction(mint_admin_hash, MINT);
        CreateTransaction(license_hash, LICENSE);
        CreateTransaction(out_hash, LICENSE);
        ConnectTransactions(mint_admin_hash, license_hash, COIN, member, DEFAULT_ADMIN_COLOR);
        ConnectTransactions(license_hash, out_hash, COIN, issuer, color, info.EncodeInfo());
    }

    ~CreateLicenseHandlerCheckValidFixture()
    {
    }

    void CheckFalse(int ndos, const std::string &msg)
    {
        bool ret;
        int v;

        ret = handler->CheckValid(
                CTransaction(transactions[license_hash]), state, NULL);
        ret &= handler->CheckFormat(
                CTransaction(transactions[license_hash]), state, NULL);

        BOOST_CHECK_MESSAGE(
                ret == false && state.IsInvalid(v) && v == ndos, msg);
    }

    uint256 mint_admin_hash, license_hash, out_hash;
    std::string member, issuer;
    type_Color color;
    CValidationState state;
    CLicenseInfo info;
};


BOOST_FIXTURE_TEST_CASE(CreateLicenseHandlerCheckValidPass, CreateLicenseHandlerCheckValidFixture)
{
    BOOST_CHECK(handler->CheckValid(
                CTransaction(transactions[license_hash]), state, NULL) == true);
}


BOOST_FIXTURE_TEST_CASE(CreateLicenseHandlerCheckValidNotAlliance, CreateLicenseHandlerCheckValidFixture)
{
    palliance->Remove(member);
    CheckFalse(100, __func__);
}


BOOST_FIXTURE_TEST_CASE(CreateLicenseHandlerCheckValidExistedColor, CreateLicenseHandlerCheckValidFixture)
{
    CLicenseInfo *pinfo = new CLicenseInfo();
    plicense->SetOwner(color, CreateAddress(), pinfo);
    CheckFalse(100, __func__);
    delete pinfo;
}


BOOST_FIXTURE_TEST_CASE(CreateLicenseHandlerCheckValidInvalidInfo, CreateLicenseHandlerCheckValidFixture)
{
    ConnectTransactions(license_hash, out_hash, COIN, issuer, color, "fake_info");
    CheckFalse(100, __func__);
}

struct TransferLicenseHandlerCheckValidFixture : public LicenseHandlerFixture
{
    TransferLicenseHandlerCheckValidFixture()
    {
        license_hash = ArithToUint256(arith_uint256(1));
        transfer_hash = ArithToUint256(arith_uint256(2));
        out_hash = ArithToUint256(arith_uint256(3));

        color = 5;

        issuer = CreateAddress();
        user = CreateAddress();

        CreateTransaction(license_hash, LICENSE);
        plicense->SetOwner(color, issuer);
        CreateTransaction(transfer_hash, LICENSE);
        CreateTransaction(out_hash, NORMAL);
        ConnectTransactions(license_hash, transfer_hash, COIN, issuer, color);
        ConnectTransactions(transfer_hash, out_hash, COIN, user, color);
    }

    ~TransferLicenseHandlerCheckValidFixture()
    {
    }

    void CheckFalse(int ndos, const std::string &msg)
    {
        bool ret;
        int v;

        ret = handler->CheckValid(
                CTransaction(transactions[transfer_hash]), state, NULL);
        ret &= handler->CheckFormat(
                CTransaction(transactions[transfer_hash]), state, NULL);

        BOOST_CHECK_MESSAGE(
                ret == false && state.IsInvalid(v) && v == ndos, msg);
    }

    uint256 license_hash, transfer_hash, out_hash;
    std::string issuer, user;
    type_Color color;
    CValidationState state;
};

BOOST_FIXTURE_TEST_CASE(TransferLicenseHandlerCheckValidNonOwner, TransferLicenseHandlerCheckValidFixture)
{
    plicense->SetOwner(color, "somebody");
    CheckFalse(100, __func__);
}

BOOST_FIXTURE_TEST_CASE(TransferLicenseHandlerCheckValidNewLicnse, TransferLicenseHandlerCheckValidFixture)
{
    plicense->RemoveColor(color);
    CheckFalse(100, __func__);
}

BOOST_AUTO_TEST_SUITE_END();
