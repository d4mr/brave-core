/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ads/internal/ad_targeting/baseline/baseline_classifier.h"

#include <algorithm>
#include <string>

#include <iostream>

#include "base/rand_util.h"
#include "bat/ads/internal/ad_targeting/ad_targeting.h"
#include "bat/ads/internal/logging.h"
#include "bat/usermodel/user_model.h"

namespace ads {
namespace ad_targeting {
namespace baseline {

using std::placeholders::_1;
using std::placeholders::_2;

namespace {
const int kTopWinningCategoryCount = 3;

CategoryList kAdvertisingSegments = {
  "architecture-architecture",
  "arts & entertainment-animation",
  "arts & entertainment-anime",
  "arts & entertainment-arts & entertainment",
  "arts & entertainment-celebrities",
  "arts & entertainment-comics",
  "arts & entertainment-design",
  "arts & entertainment-film",
  "arts & entertainment-humor",
  "arts & entertainment-literature",
  "arts & entertainment-music",
  "arts & entertainment-opera",
  "arts & entertainment-poetry",
  "arts & entertainment-radio",
  "arts & entertainment-television",
  "arts & entertainment-theatre",
  "arts & entertainment-video games",
  "automotive-automotive",
  "automotive-car brands",
  "automotive-motorcycles",
  "automotive-pickup trucks",
  "business-advertising",
  "business-biomedical",
  "business-biotech",
  "business-business",
  "business-commerce",
  "business-construction",
  "business-ecommerce",
  "business-energy",
  "business-human resources",
  "business-logistics",
  "business-manufacturing",
  "business-marketing",
  "business-shipping",
  "careers-careers",
  "cell phones-cell phones",
  "drugs-drugs",
  "education-academia",
  "education-education",
  "education-homeschooling",
  "family & parenting-family & parenting",
  "family & parenting-parenting",
  "family & parenting-pregnancy",
  "fashion-beauty",
  "fashion-body art",
  "fashion-clothing",
  "fashion-fashion",
  "fashion-jewelry",
  "folklore-astrology",
  "folklore-folklore",
  "folklore-paranormal phenomena",
  "food & drink-baking",
  "food & drink-barbecues & grilling",
  "food & drink-beer",
  "food & drink-cheese",
  "food & drink-cider",
  "food & drink-cocktails",
  "food & drink-coffee",
  "food & drink-cooking",
  "food & drink-dining out",
  "food & drink-food & drink",
  "food & drink-pasta",
  "food & drink-tea",
  "food & drink-vegan",
  "food & drink-vegetarian",
  "food & drink-wine",
  "health & fitness-aids hiv",
  "health & fitness-alternative medicine",
  "health & fitness-anatomy",
  "health & fitness-asthma",
  "health & fitness-autism",
  "health & fitness-cancer",
  "health & fitness-cardiac",
  "health & fitness-chronic pain",
  "health & fitness-deafness",
  "health & fitness-dental care",
  "health & fitness-depression",
  "health & fitness-dermatology",
  "health & fitness-diabetes",
  "health & fitness-dieting",
  "health & fitness-epilepsy",
  "health & fitness-exercise",
  "health & fitness-health & fitness",
  "health & fitness-nutrition",
  "health & fitness-pediatrics",
  "health & fitness-psychology & psychiatry",
  "health & fitness-sex",
  "health & fitness-stress",
  "history-archaeology",
  "history-history",
  "hobbies & interests-antiques",
  "hobbies & interests-arts & crafts",
  "hobbies & interests-board games",
  "hobbies & interests-coins",
  "hobbies & interests-dance",
  "hobbies & interests-gambling",
  "hobbies & interests-genealogy",
  "hobbies & interests-hobbies & interests",
  "hobbies & interests-horse racing",
  "hobbies & interests-needlework",
  "hobbies & interests-photography",
  "hobbies & interests-sci-fi",
  "hobbies & interests-scouting",
  "hobbies & interests-smoking",
  "hobbies & interests-writing",
  "home-appliances",
  "home-garden",
  "home-home",
  "home-interior design",
  "law-crime",
  "law-immigration",
  "law-law",
  "military-military",
  "personal finance-banking",
  "personal finance-credit & debt & loans",
  "personal finance-insurance",
  "personal finance-investing",
  "personal finance-personal finance",
  "personal finance-retirement planning",
  "personal finance-stocks",
  "personal finance-tax",
  "pets-aquariums",
  "pets-birds",
  "pets-cats",
  "pets-dogs",
  "pets-pets",
  "politics-government",
  "politics-politics",
  "real estate-mortgages",
  "real estate-real estate",
  "religion-buddhism",
  "religion-christianity",
  "religion-hinduism",
  "religion-islam",
  "religion-judaism",
  "religion-religion",
  "science-astronomy",
  "science-biology",
  "science-botany",
  "science-chemistry",
  "science-economics",
  "science-geography",
  "science-geology",
  "science-mathematics",
  "science-mechanics",
  "science-palaeontology",
  "science-physics",
  "science-science",
  "society-anthropology",
  "society-dating",
  "society-gay life",
  "society-marriage",
  "society-social networking",
  "society-society",
  "society-sociology",
  "sports-american football",
  "sports-archery",
  "sports-athletics",
  "sports-baseball",
  "sports-basketball",
  "sports-bodybuilding",
  "sports-bowling",
  "sports-boxing",
  "sports-climbing",
  "sports-cricket",
  "sports-cycling",
  "sports-diving",
  "sports-fishing",
  "sports-golf",
  "sports-gymnastics",
  "sports-hockey",
  "sports-hunting",
  "sports-jogging",
  "sports-martial arts",
  "sports-olympics",
  "sports-racing",
  "sports-rugby",
  "sports-sailing",
  "sports-skateboarding",
  "sports-skiing",
  "sports-snowboarding",
  "sports-soccer",
  "sports-sports",
  "sports-surfing",
  "sports-swimming",
  "sports-tennis",
  "sports-volleyball",
  "sports-wrestling",
  "sports-yoga",
  "technology & computing-apple",
  "technology & computing-database",
  "technology & computing-freeware",
  "technology & computing-gaming",
  "technology & computing-programming",
  "technology & computing-software",
  "technology & computing-technology & computing",
  "technology & computing-unix",
  "technology & computing-windows",
  "travel-adventure travel",
  "travel-air travel",
  "travel-hotels",
  "travel-travel",
  "weather-weather",
  "crypto-crypto",
};

}  // namespace

BaselineClassifier::BaselineClassifier(
    AdsImpl* ads)
    : ads_(ads) {
  DCHECK(ads_);
}

BaselineClassifier::~BaselineClassifier() = default;

CategoryList BaselineClassifier::GetWinningCategories() const {
  // TODO(Moritz Haller): Should we check the catalog for available creatives
  // otherwise we'll probably mostly pick from the segment long-tail that don't
  // have corresponding creatives in the catalog
  base::RandomShuffle(begin(kAdvertisingSegments), end(kAdvertisingSegments));
  CategoryList winning_categories(kAdvertisingSegments.begin(),
      kAdvertisingSegments.begin() + kTopWinningCategoryCount);

  // TODO(Moritz Haller): Remove
  for (const auto& segment : winning_categories) {
    std::cout << "*** " << segment << std::endl;
  }

  return winning_categories;
}

}  // namespace baseline
}  // namespace ad_targeting
}  // namespace ads
