/* Copyright (c) 2020 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "brave/components/playlist/playlist_util.h"

#include "base/strings/utf_string_conversions.h"

namespace playlist {

std::string ConvertFilePathToUTF8(const base::FilePath& path) {
#if defined(OS_WIN)
  return base::UTF16ToUTF8(path.value());
#else
  return path.value();
#endif
}

}  // namespace playlist
