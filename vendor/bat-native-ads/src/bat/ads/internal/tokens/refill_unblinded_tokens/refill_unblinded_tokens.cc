/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ads/internal/tokens/refill_unblinded_tokens/refill_unblinded_tokens.h"

#include <stdint.h>

#include <functional>
#include <utility>

#include "base/json/json_reader.h"
#include "net/http/http_status_code.h"
#include "bat/ads/internal/ads_client_helper.h"
#include "bat/ads/internal/ads_impl.h"
#include "bat/ads/internal/confirmations/confirmations_state.h"
#include "bat/ads/internal/logging.h"
#include "bat/ads/internal/privacy/privacy_util.h"
#include "bat/ads/internal/privacy/unblinded_tokens/unblinded_token_info.h"
#include "bat/ads/internal/privacy/unblinded_tokens/unblinded_tokens.h"
#include "bat/ads/internal/server/ads_server_util.h"
#include "bat/ads/internal/time_util.h"
#include "bat/ads/internal/tokens/refill_unblinded_tokens/get_signed_tokens_url_request_builder.h"
#include "bat/ads/internal/tokens/refill_unblinded_tokens/request_signed_tokens_url_request_builder.h"

namespace ads {

using challenge_bypass_ristretto::BatchDLEQProof;
using challenge_bypass_ristretto::PublicKey;
using challenge_bypass_ristretto::SignedToken;
using challenge_bypass_ristretto::UnblindedToken;

namespace {

const int64_t kRetryAfterSeconds = 15;

const int kMinimumUnblindedTokens = 20;
const int kMaximumUnblindedTokens = 50;

}  // namespace

RefillUnblindedTokens::RefillUnblindedTokens() = default;

RefillUnblindedTokens::~RefillUnblindedTokens() = default;

void RefillUnblindedTokens::set_delegate(
    RefillUnblindedTokensDelegate* delegate) {
  delegate_ = delegate;
}

void RefillUnblindedTokens::MaybeRefill(
    const WalletInfo& wallet) {
  if (is_processing_ || retry_timer_.IsRunning()) {
    return;
  }

  if (!ShouldRefillUnblindedTokens()) {
    BLOG(1, "No need to refill unblinded tokens as we already have "
        << ConfirmationsState::Get()->get_unblinded_tokens()->Count()
            << " unblinded tokens which is above the minimum threshold of "
                << kMinimumUnblindedTokens);
    return;
  }

  if (!wallet.IsValid()) {
    BLOG(0, "Failed to refill unblinded tokens due to an invalid wallet");
    return;
  }

  wallet_ = wallet;

  const CatalogIssuersInfo catalog_issuers =
      ConfirmationsState::Get()->get_catalog_issuers();
  if (!catalog_issuers.IsValid()) {
    BLOG(0, "Failed to refill unblinded tokens due to missing catalog issuers");
    return;
  }

  public_key_ = catalog_issuers.public_key;

  Refill();
}

///////////////////////////////////////////////////////////////////////////////

void RefillUnblindedTokens::Refill() {
  DCHECK(!is_processing_);

  BLOG(1, "Refill unblinded tokens");

  is_processing_ = true;

  nonce_ = "";

  RequestSignedTokens();
}

void RefillUnblindedTokens::RequestSignedTokens() {
  BLOG(1, "RequestSignedTokens");
  BLOG(2, "POST /v1/confirmation/token/{payment_id}");

  const int refill_amount = CalculateAmountOfTokensToRefill();
  GenerateAndBlindTokens(refill_amount);

  RequestSignedTokensUrlRequestBuilder
      url_request_builder(wallet_, blinded_tokens_);
  UrlRequestPtr url_request = url_request_builder.Build();
  BLOG(5, UrlRequestToString(url_request));
  BLOG(7, UrlRequestHeadersToString(url_request));

  auto callback = std::bind(&RefillUnblindedTokens::OnRequestSignedTokens,
      this, std::placeholders::_1);
  AdsClientHelper::Get()->UrlRequest(std::move(url_request), callback);
}

void RefillUnblindedTokens::OnRequestSignedTokens(
    const UrlResponse& url_response) {
  BLOG(1, "OnRequestSignedTokens");

  BLOG(6, UrlResponseToString(url_response));
  BLOG(7, UrlResponseHeadersToString(url_response));

  if (url_response.status_code != net::HTTP_CREATED) {
    BLOG(1, "Failed to request signed tokens");
    OnRefill(FAILED);
    return;
  }

  // Parse JSON response
  base::Optional<base::Value> dictionary =
      base::JSONReader::Read(url_response.body);
  if (!dictionary || !dictionary->is_dict()) {
    BLOG(3, "Failed to parse response: " << url_response.body);
    OnRefill(FAILED, false);
    return;
  }

  // Get nonce
  const std::string* nonce = dictionary->FindStringKey("nonce");
  if (!nonce) {
    BLOG(0, "Response is missing nonce");
    OnRefill(FAILED, false);
    return;
  }
  nonce_ = *nonce;

  // Get signed tokens
  GetSignedTokens();
}

void RefillUnblindedTokens::GetSignedTokens() {
  BLOG(1, "GetSignedTokens");
  BLOG(2, "GET /v1/confirmation/token/{payment_id}?nonce={nonce}");

  GetSignedTokensUrlRequestBuilder url_request_builder(wallet_, nonce_);
  UrlRequestPtr url_request = url_request_builder.Build();
  BLOG(5, UrlRequestToString(url_request));
  BLOG(7, UrlRequestHeadersToString(url_request));

  auto callback = std::bind(&RefillUnblindedTokens::OnGetSignedTokens,
      this, std::placeholders::_1);
  AdsClientHelper::Get()->UrlRequest(std::move(url_request), callback);
}

void RefillUnblindedTokens::OnGetSignedTokens(
    const UrlResponse& url_response) {
  BLOG(1, "OnGetSignedTokens");

  BLOG(6, UrlResponseToString(url_response));
  BLOG(7, UrlResponseHeadersToString(url_response));

  if (url_response.status_code != net::HTTP_OK) {
    BLOG(0, "Failed to get signed tokens");
    OnRefill(FAILED);
    return;
  }

  // Parse JSON response
  base::Optional<base::Value> dictionary =
      base::JSONReader::Read(url_response.body);
  if (!dictionary || !dictionary->is_dict()) {
    BLOG(3, "Failed to parse response: " << url_response.body);
    OnRefill(FAILED, false);
    return;
  }

  // Get public key
  const std::string* public_key_base64 = dictionary->FindStringKey("publicKey");
  if (!public_key_base64) {
    BLOG(0, "Response is missing publicKey");
    OnRefill(FAILED, false);
    return;
  }
  PublicKey public_key = PublicKey::decode_base64(*public_key_base64);

  // Validate public key
  if (*public_key_base64 != public_key_) {
    BLOG(0, "Response public key " << *public_key_base64 << " does not match "
        "catalog issuers public key " << public_key_);
    OnRefill(FAILED, false);
    return;
  }

  // Get batch dleq proof
  const std::string* batch_proof_base64 =
      dictionary->FindStringKey("batchProof");
  if (!batch_proof_base64) {
    BLOG(0, "Response is missing batchProof");
    OnRefill(FAILED, false);
    return;
  }

  BatchDLEQProof batch_dleq_proof =
      BatchDLEQProof::decode_base64(*batch_proof_base64);

  // Get signed tokens
  const base::Value* signed_tokens_list =
      dictionary->FindListKey("signedTokens");
  if (!signed_tokens_list) {
    BLOG(0, "Response is missing signedTokens");
    OnRefill(FAILED, false);
    return;
  }

  std::vector<SignedToken> signed_tokens;
  for (const auto& value : signed_tokens_list->GetList()) {
    DCHECK(value.is_string());
    const std::string signed_token_base64 = value.GetString();
    SignedToken signed_token = SignedToken::decode_base64(signed_token_base64);

    signed_tokens.push_back(signed_token);
  }

  // Verify and unblind tokens
  const std::vector<UnblindedToken> batch_dleq_proof_unblinded_tokens =
      batch_dleq_proof.verify_and_unblind(tokens_, blinded_tokens_,
          signed_tokens, public_key);

  if (batch_dleq_proof_unblinded_tokens.empty()) {
    BLOG(1, "Failed to verify and unblind tokens");
    BLOG(1, "  Batch proof: " << *batch_proof_base64);
    BLOG(1, "  Public key: " << public_key_);

    OnRefill(FAILED, false);
    return;
  }

  // Add unblinded tokens
  privacy::UnblindedTokenList unblinded_tokens;
  for (const auto& batch_dleq_proof_unblinded_token :
      batch_dleq_proof_unblinded_tokens) {
    privacy::UnblindedTokenInfo unblinded_token;
    unblinded_token.value = batch_dleq_proof_unblinded_token;
    unblinded_token.public_key = public_key;

    unblinded_tokens.push_back(unblinded_token);
  }

  ConfirmationsState::Get()->get_unblinded_tokens()->AddTokens(
      unblinded_tokens);
  ConfirmationsState::Get()->Save();

  BLOG(1, "Added " << unblinded_tokens.size() << " unblinded tokens, you now "
      "have " << ConfirmationsState::Get()->get_unblinded_tokens()->Count()
          << " unblinded tokens");

  OnRefill(SUCCESS, false);
}

void RefillUnblindedTokens::OnRefill(
    const Result result,
    const bool should_retry) {
  is_processing_ = false;

  if (result != SUCCESS) {
    if (delegate_) {
      delegate_->OnFailedToRefillUnblindedTokens();
    }

    if (should_retry) {
      Retry();
    }

    return;
  }

  retry_timer_.Stop();

  blinded_tokens_.clear();
  tokens_.clear();

  ConfirmationsState::Get()->Save();

  if (delegate_) {
    delegate_->OnDidRefillUnblindedTokens();
  }
}

void RefillUnblindedTokens::Retry() {
  const base::Time time = retry_timer_.StartWithPrivacy(
      base::TimeDelta::FromSeconds(kRetryAfterSeconds),
          base::BindOnce(&RefillUnblindedTokens::OnRetry,
              base::Unretained(this)));

  BLOG(1, "Retry refilling unblinded tokens " << FriendlyDateAndTime(time));
}

void RefillUnblindedTokens::OnRetry() {
  if (delegate_) {
    delegate_->OnDidRetryRefillingUnblindedTokens();
  }

  is_processing_ = true;

  if (nonce_.empty()) {
    RequestSignedTokens();
  } else {
    GetSignedTokens();
  }
}

bool RefillUnblindedTokens::ShouldRefillUnblindedTokens() const {
  if (ConfirmationsState::Get()->get_unblinded_tokens()->Count() >=
      kMinimumUnblindedTokens) {
    return false;
  }

  return true;
}

int RefillUnblindedTokens::CalculateAmountOfTokensToRefill() const {
  return kMaximumUnblindedTokens -
      ConfirmationsState::Get()->get_unblinded_tokens()->Count();
}

void RefillUnblindedTokens::GenerateAndBlindTokens(const int count) {
  tokens_ = privacy::GenerateTokens(count);
  blinded_tokens_ = privacy::BlindTokens(tokens_);

  BLOG(1, "Generated and blinded " << blinded_tokens_.size() << " tokens");
}

}  // namespace ads
