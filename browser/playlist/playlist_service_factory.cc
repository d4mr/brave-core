/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/browser/playlist/playlist_service_factory.h"

#include <memory>

#include "base/feature_list.h"
#include "brave/browser/extensions/api/playlist/playlist_event_router_factory.h"
#include "brave/components/playlist/features.h"
#include "brave/components/playlist/playlist_service.h"
#include "brave/components/playlist/playlist_download_request_manager.h"
#include "brave/components/playlist/playlist_youtubedown_component_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_isolated_world_ids.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace playlist {

// static
PlaylistServiceFactory* PlaylistServiceFactory::GetInstance() {
  return base::Singleton<PlaylistServiceFactory>::get();
}

// static
PlaylistService* PlaylistServiceFactory::GetForProfile(Profile* profile) {
  if (base::FeatureList::IsEnabled(playlist::features::kPlaylist)) {
    // Create EventRouter here because it broadcasts PlaylistService's event.
    PlaylistEventRouterFactory::GetInstance()->GetForProfile(profile);

    return static_cast<PlaylistService*>(
        GetInstance()->GetServiceForBrowserContext(profile, true));
  }

  return nullptr;
}

PlaylistServiceFactory::PlaylistServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "PlaylistService",
          BrowserContextDependencyManager::GetInstance()) {
  PlaylistDownloadRequestManager::SetPlaylistJavaScriptWorldId(
      ISOLATED_WORLD_ID_CHROME_INTERNAL);
  playlist_youtubedown_component_manager_.reset(
      new PlaylistYoutubeDownComponentManager(
          g_browser_process->component_updater()));
}

PlaylistServiceFactory::~PlaylistServiceFactory() {}

KeyedService* PlaylistServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new PlaylistService(context,
                             playlist_youtubedown_component_manager_.get());
}

}  // namespace playlist
