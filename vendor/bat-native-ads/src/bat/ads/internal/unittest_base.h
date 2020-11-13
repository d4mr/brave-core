/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BAT_ADS_INTERNAL_UNITTEST_BASE_H_
#define BAT_ADS_INTERNAL_UNITTEST_BASE_H_

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "brave/components/l10n/browser/locale_helper_mock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "bat/ads/database.h"
#include "bat/ads/internal/ads/ad_notifications/ad_notifications.h"
#include "bat/ads/internal/ads_client_helper.h"
#include "bat/ads/internal/ads_client_mock.h"
#include "bat/ads/internal/ads_impl.h"
#include "bat/ads/internal/client/client.h"
#include "bat/ads/internal/confirmations/confirmations_state.h"
#include "bat/ads/internal/database/database_initialize.h"
#include "bat/ads/internal/platform/platform_helper_mock.h"
#include "bat/ads/internal/tab_manager/tab_manager.h"
#include "bat/ads/internal/user_activity/user_activity.h"

namespace ads {

class UnitTestBase : public testing::Test {
 public:
  UnitTestBase();

  ~UnitTestBase() override;

  UnitTestBase(const UnitTestBase&) = delete;
  UnitTestBase& operator=(const UnitTestBase&) = delete;

  // If |end_to_end_testing| is true test the functionality and performance
  // under product-like circumstances with data to replicate live settings to
  // simulate what a real user scenario looks like from start to finish. You
  // must call |InitializeAds| manually after setting up your mocks
  void SetUpForTesting(
      const bool end_to_end_testing);

  void InitializeAds();

  // testing::Test implementation
  void SetUp() override;
  void TearDown() override;

 protected:
  base::test::TaskEnvironment task_environment_;

  base::ScopedTempDir temp_dir_;

  std::unique_ptr<AdsClientMock> ads_client_mock_;
  std::unique_ptr<AdsClientHelper> ads_client_helper_;
  std::unique_ptr<AdsImpl> ads_;
  std::unique_ptr<AdNotifications> ad_notifications_;
  std::unique_ptr<Client> client_;
  std::unique_ptr<ConfirmationsState> confirmations_state_;
  std::unique_ptr<brave_l10n::LocaleHelperMock> locale_helper_mock_;
  std::unique_ptr<Database> database_;
  std::unique_ptr<PlatformHelperMock> platform_helper_mock_;
  std::unique_ptr<TabManager> tab_manager_;
  std::unique_ptr<UserActivity> user_activity_;

  // Fast-forwards virtual time by |time_delta|, causing all tasks on the main
  // thread and thread pool with a remaining delay less than or equal to
  // |time_delta| to be executed in their natural order before this returns
  void FastForwardClockBy(
      const base::TimeDelta& time_delta);

  // Unlike |FastForwardBy| AdvanceClock does not run tasks
  void AdvanceClockToMidnightUTC();
  void AdvanceClock(
      const base::TimeDelta& time_delta);

 private:
  bool setup_called_ = false;
  bool teardown_called_ = false;

  bool end_to_end_testing_ = false;

  std::unique_ptr<database::Initialize> database_initialize_;

  void Initialize();
};

}  // namespace ads

#endif  // BAT_ADS_INTERNAL_UNITTEST_BASE_H_
