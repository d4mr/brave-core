/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <limits>

#include "chrome/browser/ui/singleton_tabs.h"

namespace {

const char kBraveStubSessionTag[] = "brave_stub_more_session_tag";
const char kBraveSyncedTabsUrl[] = "brave://history/syncedTabs";
const int kStubTabId = std::numeric_limits<int32_t>::max() - 1;

}  //  namespace

#define BRAVE_EXECUTE_COMMAND                                                \
  if (item.session_tag == kBraveStubSessionTag) {                            \
    ShowSingletonTabOverwritingNTP(                                          \
        browser_,                                                            \
        GetSingletonTabNavigateParams(browser_, GURL(kBraveSyncedTabsUrl))); \
    return;                                                                  \
  }

#define BRAVE_BUILD_TABS_FROM_OTHER_DEVICES                                 \
  if (tabs_in_session.size() > kMaxTabsPerSessionToShow) {                  \
    /* Not all the tabs are shown in menu */                                \
    if (!stub_tab_.get()) {                                                 \
      stub_tab_.reset(new sessions::SessionTab());                          \
      sessions::SerializedNavigationEntry stub_nav_entry;                   \
      stub_nav_entry.set_title(                                             \
          l10n_util::GetStringUTF16(IDS_OPEN_MORE_OTHER_DEVICES_SESSIONS)); \
      stub_nav_entry.set_virtual_url(GURL(kBraveSyncedTabsUrl));            \
      stub_tab_->navigations.push_back(stub_nav_entry);                     \
      stub_tab_->tab_id = SessionID::FromSerializedValue(kStubTabId);       \
    }                                                                       \
    tabs_in_session[kMaxTabsPerSessionToShow] = stub_tab_.get();            \
    BuildOtherDevicesTabItem(kBraveStubSessionTag,                          \
                             *tabs_in_session[kMaxTabsPerSessionToShow]);   \
  }

#include "../../../../../../chrome/browser/ui/toolbar/recent_tabs_sub_menu_model.cc"

#undef BRAVE_BUILD_TABS_FROM_OTHER_DEVICES
#undef BRAVE_EXECUTE_COMMAND
