/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bat/ads/internal/database/tables/categories_database_table.h"

#include <utility>

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "bat/ads/internal/ads_client_helper.h"
#include "bat/ads/internal/database/database_statement_util.h"
#include "bat/ads/internal/database/database_table_util.h"
#include "bat/ads/internal/database/database_util.h"
#include "bat/ads/internal/logging.h"

namespace ads {
namespace database {
namespace table {

namespace {
const char kTableName[] = "categories";
}  // namespace

Categories::Categories() = default;

Categories::~Categories() = default;

void Categories::InsertOrUpdate(
    DBTransaction* transaction,
    const CreativeAdList& creative_ads) {
  DCHECK(transaction);

  if (creative_ads.empty()) {
    return;
  }

  DBCommandPtr command = DBCommand::New();
  command->type = DBCommand::Type::RUN;
  command->command = BuildInsertOrUpdateQuery(command.get(), creative_ads);

  transaction->commands.push_back(std::move(command));
}

void Categories::Delete(
    ResultCallback callback) {
  DBTransactionPtr transaction = DBTransaction::New();

  util::Delete(transaction.get(), get_table_name());

  AdsClientHelper::Get()->RunDBTransaction(std::move(transaction),
      std::bind(&OnResultCallback, std::placeholders::_1, callback));
}

std::string Categories::get_table_name() const {
  return kTableName;
}

void Categories::Migrate(
    DBTransaction* transaction,
    const int to_version) {
  DCHECK(transaction);

  switch (to_version) {
    case 1: {
      MigrateToV1(transaction);
      break;
    }

    case 3: {
      MigrateToV3(transaction);
      break;
    }

    default: {
      break;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

int Categories::BindParameters(
    DBCommand* command,
    const CreativeAdList& creative_ads) {
  DCHECK(command);

  int count = 0;

  int index = 0;
  for (const auto& creative_ad : creative_ads) {
    BindString(command, index++, creative_ad.creative_set_id);
    BindString(command, index++, base::ToLowerASCII(creative_ad.category));

    count++;
  }

  return count;
}

std::string Categories::BuildInsertOrUpdateQuery(
    DBCommand* command,
    const CreativeAdList& creative_ads) {
  const int count = BindParameters(command, creative_ads);

  return base::StringPrintf(
      "INSERT OR REPLACE INTO %s "
          "(creative_set_id, "
          "category) VALUES %s",
      get_table_name().c_str(),
      BuildBindingParameterPlaceholders(2, count).c_str());
}

void Categories::CreateTableV1(
    DBTransaction* transaction) {
  DCHECK(transaction);

  const std::string query = base::StringPrintf(
      "CREATE TABLE %s "
          "(creative_instance_id TEXT NOT NULL, "
          "category TEXT NOT NULL, "
          "UNIQUE(creative_instance_id, category) ON CONFLICT REPLACE, "
          "CONSTRAINT fk_creative_instance_id "
              "FOREIGN KEY (creative_instance_id) "
              "REFERENCES creative_ad_notifications (creative_instance_id) "
              "ON DELETE CASCADE)",
      get_table_name().c_str());

  DBCommandPtr command = DBCommand::New();
  command->type = DBCommand::Type::EXECUTE;
  command->command = query;

  transaction->commands.push_back(std::move(command));
}

void Categories::CreateIndexV1(
    DBTransaction* transaction) {
  DCHECK(transaction);

  util::CreateIndex(transaction, get_table_name(), "category");
}

void Categories::MigrateToV1(
    DBTransaction* transaction) {
  DCHECK(transaction);

  util::Drop(transaction, get_table_name());

  CreateTableV1(transaction);
  CreateIndexV1(transaction);
}

void Categories::CreateTableV3(
    DBTransaction* transaction) {
  DCHECK(transaction);

  const std::string query = base::StringPrintf(
      "CREATE TABLE %s "
          "(creative_set_id TEXT NOT NULL, "
          "category TEXT NOT NULL, "
          "PRIMARY KEY (creative_set_id, category), "
          "UNIQUE(creative_set_id, category) ON CONFLICT REPLACE)",
      get_table_name().c_str());

  DBCommandPtr command = DBCommand::New();
  command->type = DBCommand::Type::EXECUTE;
  command->command = query;

  transaction->commands.push_back(std::move(command));
}

void Categories::CreateIndexV3(
    DBTransaction* transaction) {
  DCHECK(transaction);

  util::CreateIndex(transaction, get_table_name(), "creative_set_id");
}

void Categories::MigrateToV3(
    DBTransaction* transaction) {
  DCHECK(transaction);

  util::Drop(transaction, get_table_name());

  CreateTableV3(transaction);
  CreateIndexV3(transaction);
}

}  // namespace table
}  // namespace database
}  // namespace ads
