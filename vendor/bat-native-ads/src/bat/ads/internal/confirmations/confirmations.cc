/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ads/internal/confirmations/confirmations.h"

#include <stdint.h>

#include <functional>

#include "bat/ads/internal/ads_impl.h"
#include "bat/ads/internal/logging.h"
#include "bat/ads/internal/privacy/unblinded_tokens/unblinded_tokens.h"
#include "bat/ads/internal/tokens/redeem_unblinded_token/redeem_unblinded_token.h"

namespace ads {

using std::placeholders::_1;
using std::placeholders::_2;

namespace {

const char kConfirmationsFilename[] = "confirmations.json";

const uint64_t kRetryAfterSeconds = 5 * base::Time::kSecondsPerMinute;

}  // namespace

Confirmations::Confirmations(
    AdsImpl* ads)
    : ads_(ads),
      state_(std::make_unique<ConfirmationsState>(ads_)) {
  DCHECK(ads_);
}

Confirmations::~Confirmations() = default;

void Confirmations::Initialize(
    InitializeCallback callback) {
  callback_ = callback;

  Load();
}

CatalogIssuersInfo Confirmations::GetCatalogIssuers() const {
  return state_->get_catalog_issuers();
}

void Confirmations::SetCatalogIssuers(
    const CatalogIssuersInfo& catalog_issuers) {
  BLOG(1, "SetCatalogIssuers:");
  BLOG(1, "  Public key: " << catalog_issuers.public_key);
  BLOG(1, "  Issuers:");

  for (const auto& issuer : catalog_issuers.issuers) {
    BLOG(1, "    Name: " << issuer.name);
    BLOG(1, "    Public key: " << issuer.public_key);
  }

  const CatalogIssuersInfo current_catalog_issuers =
      state_->get_catalog_issuers();

  const bool public_key_was_rotated =
      !current_catalog_issuers.public_key.empty() &&
          current_catalog_issuers.public_key != catalog_issuers.public_key;

  state_->set_catalog_issuers(catalog_issuers);

  if (public_key_was_rotated) {
    state_->get_unblinded_tokens()->RemoveAllTokens();
  }

  Save();
}

base::Time Confirmations::get_next_token_redemption_date() const {
  return state_->get_next_token_redemption_date();
}

void Confirmations::set_next_token_redemption_date(
    const base::Time& next_token_redemption_date) {
  state_->set_next_token_redemption_date(next_token_redemption_date);
  Save();
}

void Confirmations::ConfirmAd(
    const std::string& creative_instance_id,
    const ConfirmationType confirmation_type) {
  BLOG(1, "Confirming " << std::string(confirmation_type) << " ad for "
      "creative instance id " << creative_instance_id);

  ads_->get_redeem_unblinded_token()->Redeem(
      creative_instance_id, confirmation_type);
}

void Confirmations::RetryFailedConfirmationsAfterDelay() {
  if (failed_confirmations_timer_.IsRunning()) {
    return;
  }

  const base::Time time = failed_confirmations_timer_.StartWithPrivacy(
      base::TimeDelta::FromSeconds(kRetryAfterSeconds),
          base::BindOnce(&Confirmations::RetryFailedConfirmations,
              base::Unretained(this)));

  BLOG(1, "Retry failed confirmations " << FriendlyDateAndTime(time));
}

TransactionList Confirmations::get_transactions() const {
  return state_->get_transactions();
}

void Confirmations::AppendTransaction(
    const double estimated_redemption_value,
    const ConfirmationType confirmation_type) {
  TransactionInfo transaction;

  transaction.timestamp = static_cast<int64_t>(base::Time::Now().ToDoubleT());
  transaction.estimated_redemption_value = estimated_redemption_value;
  transaction.confirmation_type = std::string(confirmation_type);

  state_->append_transaction(transaction);
  Save();

  ads_->get_ads_client()->OnAdRewardsChanged();
}

void Confirmations::AppendConfirmationToRetryQueue(
    const ConfirmationInfo& confirmation) {
  state_->append_confirmation(confirmation);
  Save();

  BLOG(1, "Added confirmation id " << confirmation.id << ", creative instance "
      "id " << confirmation.creative_instance_id << " and "
          << std::string(confirmation.type) << " to the confirmations queue");

  RetryFailedConfirmationsAfterDelay();
}

privacy::UnblindedTokens* Confirmations::get_unblinded_tokens() {
  return state_->get_unblinded_tokens();
}

privacy::UnblindedTokens* Confirmations::get_unblinded_payment_tokens() {
  return state_->get_unblinded_payment_tokens();
}

void Confirmations::Save() {
  if (!is_initialized_) {
    return;
  }

  BLOG(9, "Saving confirmations state");

  const std::string json = state_->ToJson();
  auto callback = std::bind(&Confirmations::OnSaved, this, _1);
  ads_->get_ads_client()->Save(kConfirmationsFilename, json, callback);
}

///////////////////////////////////////////////////////////////////////////////

void Confirmations::RetryFailedConfirmations() {
  ConfirmationList confirmations = state_->get_confirmations();
  if (confirmations.empty()) {
    BLOG(1, "No failed confirmations to retry");
    return;
  }

  ConfirmationInfo confirmation(confirmations.front());
  RemoveConfirmationFromRetryQueue(confirmation);

  ads_->get_redeem_unblinded_token()->Redeem(confirmation);

  RetryFailedConfirmationsAfterDelay();
}

void Confirmations::RemoveConfirmationFromRetryQueue(
    const ConfirmationInfo& confirmation) {
  if (!state_->remove_confirmation(confirmation)) {
    BLOG(0, "Failed to remove confirmation id " << confirmation.id
        << ", creative instance id " << confirmation.creative_instance_id
            << " and " << std::string(confirmation.type) << " from "
                "the confirmations queue");

    return;
  }

  BLOG(1, "Removed confirmation id " << confirmation.id << ", creative "
      "instance id " << confirmation.creative_instance_id << " and " <<
          std::string(confirmation.type) << " from the confirmations queue");

  Save();
}

void Confirmations::OnSaved(
    const Result result) {
  if (result != SUCCESS) {
    BLOG(0, "Failed to save confirmations state");

    return;
  }

  BLOG(9, "Successfully saved confirmations state");
}

void Confirmations::Load() {
  BLOG(3, "Loading confirmations state");

  auto callback = std::bind(&Confirmations::OnLoaded, this, _1, _2);
  ads_->get_ads_client()->Load(kConfirmationsFilename, callback);
}

void Confirmations::OnLoaded(
    const Result result,
    const std::string& json) {
  if (result != SUCCESS) {
    BLOG(3, "Confirmations state does not exist, creating default state");

    is_initialized_ = true;

    state_.reset(new ConfirmationsState(ads_));
    Save();
  } else {
    if (!state_->FromJson(json)) {
      BLOG(0, "Failed to load confirmations state");

      BLOG(3, "Failed to parse confirmations state: " << json);

      callback_(FAILED);
      return;
    }

    BLOG(3, "Successfully loaded confirmations state");

    is_initialized_ = true;
  }

  callback_(SUCCESS);
}

}  // namespace ads
