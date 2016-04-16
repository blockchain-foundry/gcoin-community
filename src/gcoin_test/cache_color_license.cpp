// Copyright (c) 2014-2016 The Gcoin Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <stdint.h>

#include <map>
#include <string>

#include "test_util.h"
#include "policy/licenseinfo.h"

using std::string;

BOOST_AUTO_TEST_SUITE(test_cache_color_license)

BOOST_FIXTURE_TEST_CASE(CacheTestColorLicense, CacheSetupFixture)
{
    CLicenseInfo *pinfo = new CLicenseInfo();
    string issuer = "issuer";
    BOOST_CHECK(plicense->IsColorExist(3) == false);
    plicense->SetOwner(3, issuer, pinfo);
    BOOST_CHECK(plicense->IsColorExist(3));
    BOOST_CHECK(plicense->HasColorOwner(3));
    BOOST_CHECK(plicense->IsColorOwner(3, issuer));
    plicense->AddNumOfCoins(3, 100);
    BOOST_CHECK(plicense->NumOfCoins(3) == 100);
    plicense->RemoveOwner(3);
    BOOST_CHECK(plicense->IsColorExist(3));
    BOOST_CHECK(plicense->IsColorOwner(3, issuer) == false);
    plicense->RemoveColor(3);

    BOOST_CHECK(plicense->IsColorExist(3) == false);

    CLicenseInfo infoIn, infoOut;
    plicense->SetOwner(3, issuer);
    BOOST_CHECK(plicense->GetLicenseInfo(3, infoOut) == false);
    plicense->RemoveColor(3);
    BOOST_CHECK(plicense->IsColorExist(3) == false);

    plicense->SetOwner(3, "", pinfo);
    plicense->SetOwner(4, "", pinfo);
    BOOST_CHECK(plicense->IsColorExist(3));
    BOOST_CHECK(plicense->IsColorExist(4));
    plicense->RemoveColor(3);
    BOOST_CHECK(plicense->IsColorExist(3) == false);
    BOOST_CHECK(plicense->IsColorExist(4));
    plicense->RemoveAll();
    BOOST_CHECK(plicense->IsColorExist(4) == false);
}

BOOST_AUTO_TEST_SUITE_END()
