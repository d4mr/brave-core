/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BAT_ADS_INTERNAL_AD_TARGETING_BASELINE_BASELINE_CLASSIFIER_H_  // NOLINT
#define BAT_ADS_INTERNAL_AD_TARGETING_BASELINE_BASELINE_CLASSIFIER_H_  // NOLINT

#include "bat/ads/internal/ad_targeting/ad_targeting.h"

namespace ads {

class AdsImpl;

namespace ad_targeting {
namespace baseline {

class BaselineClassifier {
 public:
  BaselineClassifier(
     AdsImpl* ads);

  ~BaselineClassifier();

  CategoryList GetWinningCategories() const;

 private:
  AdsImpl* ads_;  // NOT OWNED
};

}  // namespace baseline
}  // namespace ad_targeting
}  // namespace ads

#endif  // BAT_ADS_INTERNAL_AD_TARGETING_BASELINE_BASELINE_CLASSIFIER_H_
