/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ads/internal/database/tables/creative_new_tab_page_ads_database_table.h"

#include "net/http/http_status_code.h"
#include "bat/ads/internal/unittest_base.h"
#include "bat/ads/internal/unittest_util.h"

// npm run test -- brave_unit_tests --filter=BatAds*

namespace ads {

class BatAdsCreativeNewTabPageAdsDatabaseTableTest : public UnitTestBase {
 protected:
  BatAdsCreativeNewTabPageAdsDatabaseTableTest() = default;

  ~BatAdsCreativeNewTabPageAdsDatabaseTableTest() override = default;

  void SetUp() override {
    UnitTestBase::SetUpForTesting(/* end_to_end_testing */ true);
  }
};

TEST_F(BatAdsCreativeNewTabPageAdsDatabaseTableTest,
    GetCreativeNewTabPageAdsFromCatalogEndpoint) {
  // Arrange
  const URLEndpoints endpoints = {
    {
      "/v5/catalog", {
        {
          net::HTTP_OK, "/catalog.json"
        }
      }
    }
  };

  MockUrlRequest(ads_client_mock_, endpoints);

  InitializeAds();

  // Act

  // Assert
  const std::vector<std::string> categories = {
    "Technology & Computing"
  };

  database::table::CreativeNewTabPageAds creative_new_tab_page_ads;
  creative_new_tab_page_ads.GetForCategories(categories, [](
      const Result result,
      const CategoryList& categories,
      const CreativeNewTabPageAdList& creative_new_tab_page_ads) {
    EXPECT_EQ(Result::SUCCESS, result);
    EXPECT_EQ(1UL, creative_new_tab_page_ads.size());
  });
}

}  // namespace ads
