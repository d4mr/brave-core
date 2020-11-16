/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ads/internal/account/ad_rewards/ad_rewards.h"

#include <functional>
#include <utility>

#include "net/http/http_status_code.h"
#include "bat/ads/transaction_info.h"
#include "bat/ads/internal/time_util.h"
#include "bat/ads/internal/logging.h"
#include "bat/ads/internal/confirmations/confirmations_state.h"
#include "bat/ads/internal/ads_client_helper.h"
#include "bat/ads/internal/account/transactions/transactions.h"
#include "bat/ads/internal/account/ad_rewards/payments/payments_url_request_builder.h"
#include "bat/ads/internal/account/ad_rewards/payments/payments.h"
#include "bat/ads/internal/account/ad_rewards/ad_grants/ad_grants_url_request_builder.h"
#include "bat/ads/internal/account/ad_rewards/ad_grants/ad_grants.h"

namespace ads {

namespace {

const int64_t kRetryAfterSeconds = 1 * base::Time::kSecondsPerMinute;

double CalculateEstimatedPendingRewardsForTransactions(
    const TransactionList& transactions) {
  double estimated_pending_rewards = 0.0;

  for (const auto& transaction : transactions) {
    estimated_pending_rewards += transaction.estimated_redemption_value;
  }

  return estimated_pending_rewards;
}

uint64_t CalculateAdNotificationsReceivedThisMonthForTransactions(
    const TransactionList& transactions) {
  uint64_t ad_notifications_received_this_month = 0;

  auto now = base::Time::Now();
  base::Time::Exploded now_exploded;
  now.UTCExplode(&now_exploded);

  for (const auto& transaction : transactions) {
    if (transaction.timestamp == 0) {
      // Workaround for Windows crash when passing 0 to UTCExplode
      continue;
    }

    auto transaction_timestamp =
        base::Time::FromDoubleT(transaction.timestamp);

    base::Time::Exploded transaction_timestamp_exploded;
    transaction_timestamp.UTCExplode(&transaction_timestamp_exploded);

    if (transaction_timestamp_exploded.year == now_exploded.year &&
        transaction_timestamp_exploded.month == now_exploded.month &&
        transaction.estimated_redemption_value > 0.0 &&
        ConfirmationType(transaction.confirmation_type) ==
            ConfirmationType::kViewed) {
      ad_notifications_received_this_month++;
    }
  }

  return ad_notifications_received_this_month;
}

}  // namespace

AdRewards::AdRewards()
    : ad_grants_(std::make_unique<AdGrants>()),
      payments_(std::make_unique<Payments>()) {
}

AdRewards::~AdRewards() = default;

void AdRewards::set_delegate(
    AdRewardsDelegate* delegate) {
  delegate_ = delegate;
}

void AdRewards::MaybeReconcile(
    const WalletInfo& wallet) {
  if (is_processing_ || retry_timer_.IsRunning()) {
    return;
  }

  if (!wallet.IsValid()) {
    BLOG(0, "Failed to reconcile ad rewards due to invalid wallet");
    return;
  }

  wallet_ = wallet;

  Reconcile();
}

double AdRewards::GetEstimatedPendingRewards() const {
  double estimated_pending_rewards = payments_->GetBalance();

  estimated_pending_rewards -= ad_grants_->GetBalance();

  const TransactionList uncleared_transactions = transactions::GetUncleared();
  const double uncleared_estimated_pending_rewards =
      CalculateEstimatedPendingRewardsForTransactions(uncleared_transactions);
  estimated_pending_rewards += uncleared_estimated_pending_rewards;

  estimated_pending_rewards += unreconciled_estimated_pending_rewards_;

  if (estimated_pending_rewards < 0.0) {
    estimated_pending_rewards = 0.0;
  }

  return estimated_pending_rewards;
}

uint64_t AdRewards::GetNextPaymentDateInSeconds() const {
  const base::Time now = base::Time::Now();

  const base::Time next_token_redemption_date =
      ConfirmationsState::Get()->get_next_token_redemption_date();

  const base::Time next_payment_date =
      payments_->CalculateNextPaymentDate(now, next_token_redemption_date);

  return static_cast<uint64_t>(next_payment_date.ToDoubleT());
}

uint64_t AdRewards::GetAdNotificationsReceivedThisMonth() const {
  const TransactionList transactions =
      ConfirmationsState::Get()->get_transactions();
  return CalculateAdNotificationsReceivedThisMonthForTransactions(transactions);
}

void AdRewards::SetUnreconciledTransactions(
    const TransactionList& unreconciled_transactions) {
  unreconciled_estimated_pending_rewards_ =
      CalculateEstimatedPendingRewardsForTransactions(
          unreconciled_transactions);

  ConfirmationsState::Get()->Save();
}

base::Value AdRewards::GetAsDictionary() {
  base::Value dictionary(base::Value::Type::DICTIONARY);

  const double grants_balance = ad_grants_->GetBalance();
  dictionary.SetKey("grants_balance", base::Value(grants_balance));

  base::Value payments = payments_->GetAsList();
  dictionary.SetKey("payments", base::Value(std::move(payments)));

  dictionary.SetKey("unreconciled_estimated_pending_rewards",
      base::Value(unreconciled_estimated_pending_rewards_));

  return dictionary;
}

bool AdRewards::SetFromDictionary(
    base::Value* dictionary) {
  DCHECK(dictionary);

  base::Value* ad_rewards = dictionary->FindDictKey("ads_rewards");
  if (!ad_rewards) {
    return false;
  }

  base::DictionaryValue* ads_rewards_dictionary;
  if (!ad_rewards->GetAsDictionary(&ads_rewards_dictionary)) {
    return false;
  }

  if (!ad_grants_->SetFromDictionary(ads_rewards_dictionary) ||
      !payments_->SetFromDictionary(ads_rewards_dictionary)) {
    return false;
  }

  const base::Optional<double> unreconciled_estimated_pending_rewards =
      ads_rewards_dictionary->FindDoubleKey(
          "unreconciled_estimated_pending_rewards");
  unreconciled_estimated_pending_rewards_ =
      unreconciled_estimated_pending_rewards.value_or(0.0);

  return true;
}

///////////////////////////////////////////////////////////////////////////////

void AdRewards::Reconcile() {
  DCHECK(!is_processing_);

  BLOG(1, "Reconcile ad rewards");

  is_processing_ = true;

  GetPayments();
}

void AdRewards::GetPayments() {
  BLOG(1, "GetPayments");
  BLOG(2, "GET /v1/confirmation/payment/{payment_id}");

  PaymentsUrlRequestBuilder url_request_builder(wallet_);
  UrlRequestPtr url_request = url_request_builder.Build();
  BLOG(5, UrlRequestToString(url_request));
  BLOG(7, UrlRequestHeadersToString(url_request));

  auto callback = std::bind(&AdRewards::OnGetPayments, this,
      std::placeholders::_1);
  AdsClientHelper::Get()->UrlRequest(std::move(url_request), callback);
}

void AdRewards::OnGetPayments(
    const UrlResponse& url_response) {
  BLOG(1, "OnGetPayments");

  BLOG(6, UrlResponseToString(url_response));
  BLOG(7, UrlResponseHeadersToString(url_response));

  if (url_response.status_code != net::HTTP_OK) {
    BLOG(1, "Failed to get payment balance");
    OnFailedToReconcileAdRewards();
    return;
  }

  if (!payments_->SetFromJson(url_response.body)) {
    BLOG(0, "Failed to parse payment balance: " << url_response.body);
    OnFailedToReconcileAdRewards();
    return;
  }

  GetAdGrants();
}

void AdRewards::GetAdGrants() {
  BLOG(1, "GetAdGrants");
  BLOG(2, "GET /v1/promotions/ads/grants/summary?paymentId={payment_id}");

  AdGrantsUrlRequestBuilder url_request_builder(wallet_);
  UrlRequestPtr url_request = url_request_builder.Build();
  BLOG(5, UrlRequestToString(url_request));
  BLOG(7, UrlRequestHeadersToString(url_request));

  auto callback = std::bind(&AdRewards::OnGetAdGrants, this,
      std::placeholders::_1);
  AdsClientHelper::Get()->UrlRequest(std::move(url_request), callback);
}

void AdRewards::OnGetAdGrants(
    const UrlResponse& url_response) {
  BLOG(1, "OnGetAdGrants");

  BLOG(6, UrlResponseToString(url_response));
  BLOG(7, UrlResponseHeadersToString(url_response));

  if (url_response.status_code == net::HTTP_NO_CONTENT) {
    ad_grants_ = std::make_unique<AdGrants>();
    OnDidReconcileAdRewards();
    return;
  }

  if (url_response.status_code != net::HTTP_OK) {
    BLOG(1, "Failed to get ad grants");
    OnFailedToReconcileAdRewards();
    return;
  }

  if (!ad_grants_->SetFromJson(url_response.body)) {
    BLOG(0, "Failed to parse ad grants: " << url_response.body);
    OnFailedToReconcileAdRewards();
    return;
  }

  OnDidReconcileAdRewards();
}

void AdRewards::OnDidReconcileAdRewards() {
  is_processing_ = false;

  BLOG(1, "Successfully reconciled ad rewards");

  retry_timer_.Stop();

  unreconciled_estimated_pending_rewards_ = 0.0;
  ConfirmationsState::Get()->Save();

  if (!delegate_) {
    return;
  }

  delegate_->OnDidReconcileAdRewards();
}

void AdRewards::OnFailedToReconcileAdRewards() {
  is_processing_ = false;

  BLOG(1, "Failed to reconcile ad rewards");

  Retry();

  if (!delegate_) {
    return;
  }

  delegate_->OnFailedToReconcileAdRewards();
}

void AdRewards::Retry() {
  const base::Time time = retry_timer_.StartWithPrivacy(
      base::TimeDelta::FromSeconds(kRetryAfterSeconds),
          base::BindOnce(&AdRewards::OnRetry, base::Unretained(this)));

  BLOG(1, "Retry reconciling ad rewards " << FriendlyDateAndTime(time));
}

void AdRewards::OnRetry() {
  BLOG(1, "Retry reconciling ad rewards");

  Reconcile();
}

}  // namespace ads
