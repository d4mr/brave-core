/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/components/playlist/browser/playlist_media_file_download_manager.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/values.h"
#include "brave/components/playlist/browser/playlist_constants.h"

namespace playlist {

PlaylistMediaFileDownloadManager::PlaylistMediaFileDownloadManager(
    content::BrowserContext* context,
    Delegate* delegate,
    const base::FilePath& base_dir)
    : base_dir_(base_dir),
      delegate_(delegate) {
  // TODO(pilgrim) dynamically set file extensions based on format
  // (may require changes to youtubedown parser)
  video_media_file_downloader_.reset(new PlaylistMediaFileDownloader(
      this, context, FILE_PATH_LITERAL("video_source_files"),
      FILE_PATH_LITERAL("video_file.mp4"), kPlaylistVideoMediaFilePathKey,
      kPlaylistCreateParamsVideoMediaFilesPathKey));
  audio_media_file_downloader_.reset(new PlaylistMediaFileDownloader(
      this, context, FILE_PATH_LITERAL("audio_source_files"),
      FILE_PATH_LITERAL("audio_file.m4a"), kPlaylistAudioMediaFilePathKey,
      kPlaylistCreateParamsAudioMediaFilesPathKey));
}

PlaylistMediaFileDownloadManager::~PlaylistMediaFileDownloadManager() = default;

void PlaylistMediaFileDownloadManager::GenerateMediaFileForPlaylistItem(
    const base::Value& playlist_item) {
  pending_media_file_creation_jobs_.push(playlist_item.Clone());

  // If either media file controller is generating a playlist media file,
  // delay the next playlist generation. It will be triggered when the current
  // one is finished.
  if (!video_media_file_downloader_->in_progress() &&
      !audio_media_file_downloader_->in_progress()) {
    GenerateMediaFiles();
  }
}

void PlaylistMediaFileDownloadManager::CancelDownloadRequest(
    const std::string& id) {
  // Cancel if currently downloading item is id.
  if (video_media_file_downloader_->current_playlist_id() == id) {
    video_media_file_downloader_->RequestCancelCurrentPlaylistGeneration();
    audio_media_file_downloader_->RequestCancelCurrentPlaylistGeneration();
    return;
  }

  // TODO(simonhong): Remove from queue.
}

void PlaylistMediaFileDownloadManager::CancelAllDownloadRequests() {
  video_media_file_downloader_->RequestCancelCurrentPlaylistGeneration();
  audio_media_file_downloader_->RequestCancelCurrentPlaylistGeneration();
  pending_media_file_creation_jobs_ = base::queue<base::Value>();
}

void PlaylistMediaFileDownloadManager::GenerateMediaFiles() {
  DCHECK(!video_media_file_downloader_->in_progress() &&
         !audio_media_file_downloader_->in_progress());
  DCHECK(!pending_media_file_creation_jobs_.empty());

  base::Value video_value(std::move(pending_media_file_creation_jobs_.front()));
  base::Value audio_value = video_value.Clone();
  pending_media_file_creation_jobs_.pop();
  VLOG(2) << __func__ << ": "
          << *video_value.FindStringKey(kPlaylistPlaylistNameKey);

  video_media_file_downloader_->GenerateSingleMediaFile(std::move(video_value),
                                                        base_dir_);
  audio_media_file_downloader_->GenerateSingleMediaFile(std::move(audio_value),
                                                        base_dir_);
}

void PlaylistMediaFileDownloadManager::OnMediaFileReady(
    base::Value playlist_value,
    bool partial) {
  // TODO(simonhong): Revisit how |partial| handles.
  if (video_media_file_downloader_->in_progress() ||
      audio_media_file_downloader_->in_progress())
    partial = true;
  VLOG(2) << __func__ << ": "
          << *playlist_value.FindStringKey(kPlaylistPlaylistNameKey) << " "
          << partial;

  delegate_->OnMediaFileReady(std::move(playlist_value), partial);

  // TODO(simonhong): Revisit how |partial| handles.
  // If partial is true here, one of video or audio media files are not ready.
  if (partial)
    return;

  if (!pending_media_file_creation_jobs_.empty())
    GenerateMediaFiles();
}

void PlaylistMediaFileDownloadManager::OnMediaFileGenerationFailed(
    base::Value playlist_value) {
  VLOG(2) << __func__ << ": "
          << *playlist_value.FindStringKey(kPlaylistPlaylistNameKey);

  video_media_file_downloader_->RequestCancelCurrentPlaylistGeneration();
  audio_media_file_downloader_->RequestCancelCurrentPlaylistGeneration();

  delegate_->OnMediaFileGenerationFailed(std::move(playlist_value));
 //  UpdatePlaylistValue(playlist_id, std::move(playlist_value));

 //  NotifyPlaylistChanged(
 //      {PlaylistChangeParams::ChangeType::CHANGE_TYPE_ABORTED, playlist_id});

  if (!pending_media_file_creation_jobs_.empty())
    GenerateMediaFiles();
}

}  // namespace playlist
