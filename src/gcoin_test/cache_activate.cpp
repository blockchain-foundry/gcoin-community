// Copyright (c) 2014-2016 The Gcoin Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <stdint.h>

#include <map>
#include <string>

#include "test_util.h"

BOOST_AUTO_TEST_SUITE(test_cache_activate)

BOOST_FIXTURE_TEST_CASE(CacheTestActivateAddress, CacheSetupFixture)
{
    std::string addr1 = CreateAddress();
    std::string addr2 = CreateAddress();
    std::string addr3 = CreateAddress();
    type_Color color1 = 5;
    type_Color color2 = 6;
    BOOST_CHECK(pactivate->IsActivated(color1, addr1) == false);
    pactivate->Activate(color1, addr1);
    pactivate->Activate(color2, addr2);
    BOOST_CHECK(pactivate->IsActivated(color1, addr1));
    BOOST_CHECK(pactivate->IsActivated(color2, addr1) == false);
    pactivate->Activate(color1, addr1);
    pactivate->Deactivate(color1, addr1);
    pactivate->Deactivate(color2, addr2);
    BOOST_CHECK(pactivate->IsActivated(color1, addr1));
    BOOST_CHECK(pactivate->IsActivated(color2, addr2) == false);
    pactivate->RemoveAll();
    BOOST_CHECK(pactivate->IsActivated(color2, addr2) == false);
}

BOOST_AUTO_TEST_SUITE_END()
