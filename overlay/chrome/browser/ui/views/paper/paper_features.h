// Copyright 2026 The Paper Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAPER_PAPER_FEATURES_H_
#define CHROME_BROWSER_UI_VIEWS_PAPER_PAPER_FEATURES_H_

#include "base/feature_list.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

class BrowserWindowInterface;

namespace paper {

// Replaces the tab strip and toolbar with the Paper chrome. On by default;
// --disable-features=PaperUi gives back stock Chromium for debugging.
BASE_DECLARE_FEATURE(kPaperUi);

// Ordered list of {id, title, url} dictionaries.
inline constexpr char kEssentialsPref[] = "paper.essentials";
inline constexpr char kSidebarOpenPref[] = "paper.sidebar.open";
inline constexpr char kSidebarWidthPref[] = "paper.sidebar.width";

inline constexpr int kDefaultSidebarWidth = 260;
inline constexpr int kMinSidebarWidth = 200;
inline constexpr int kMaxSidebarWidth = 420;

bool IsPaperUiEnabled();

// Paper chrome only applies to normal tabbed windows, not popups or apps.
bool IsPaperWindow(const BrowserWindowInterface* browser);

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

}  // namespace paper

#endif  // CHROME_BROWSER_UI_VIEWS_PAPER_PAPER_FEATURES_H_
