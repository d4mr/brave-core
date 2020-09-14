/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_COMPONENTS_PLAYLIST_BROWSER_PLAYLIST_MEDIA_FILE_DOWNLOAD_MANAGER_H_
#define BRAVE_COMPONENTS_PLAYLIST_BROWSER_PLAYLIST_MEDIA_FILE_DOWNLOAD_MANAGER_H_

#include <memory>

#include "base/containers/queue.h"
#include "brave/components/playlist/browser/playlist_media_file_downloader.h"

namespace base {
class Value;
}  // namespace bas<p></p>e

namespace content {
class BrowserContext;
}  // namespace content

namespace playlist {

class PlaylistMediaFileDownloadManager :
    public PlaylistMediaFileDownloader::Delegate {
 public:
  class Delegate {
   public:
    // Called when target media file generation succeed.
    virtual void OnMediaFileReady(base::Value playlist_value,
                                  bool partial) = 0;
    // Called when target media file generation failed.
    virtual void OnMediaFileGenerationFailed(base::Value playlist_value) = 0;

   protected:
    virtual ~Delegate() {}
  };

  PlaylistMediaFileDownloadManager(content::BrowserContext* context,
                                   Delegate* delegate,
                                   const base::FilePath& base_dir);
  ~PlaylistMediaFileDownloadManager() override;

  PlaylistMediaFileDownloadManager(
      const PlaylistMediaFileDownloadManager&) = delete;
  PlaylistMediaFileDownloadManager& operator=(
      const PlaylistMediaFileDownloadManager&) = delete;

  void GenerateMediaFileForPlaylistItem(const base::Value& playlist_item);
  void CancelDownloadRequest(const std::string& id);
  void CancelAllDownloadRequests();

 private:
  // PlaylistMediaFileController::Delegate overrides:
  void OnMediaFileReady(base::Value playlist_value, bool partial) override;
  void OnMediaFileGenerationFailed(base::Value playlist_value) override;

  void GenerateMediaFiles();

  const base::FilePath base_dir_;
  Delegate* delegate_;
  base::queue<base::Value> pending_media_file_creation_jobs_;
  std::unique_ptr<PlaylistMediaFileDownloader> video_media_file_downloader_;
  std::unique_ptr<PlaylistMediaFileDownloader> audio_media_file_downloader_;
};

}  // namespace playlist

#endif  // BRAVE_COMPONENTS_PLAYLIST_BROWSER_PLAYLIST_MEDIA_FILE_DOWNLOADE_MANAGER_H_
