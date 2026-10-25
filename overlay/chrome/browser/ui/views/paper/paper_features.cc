// Copyright 2026 The Paper Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/paper/paper_features.h"

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/pref_registry/pref_registry_syncable.h"

namespace paper {

BASE_FEATURE(kPaperUi, base::FEATURE_ENABLED_BY_DEFAULT);

bool IsPaperUiEnabled() {
  return base::FeatureList::IsEnabled(kPaperUi);
}

bool IsPaperWindow(const BrowserWindowInterface* browser) {
  return IsPaperUiEnabled() && browser &&
         browser->GetType() == BrowserWindowInterface::Type::TYPE_NORMAL;
}

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(kEssentialsPref);
  registry->RegisterBooleanPref(kSidebarOpenPref, false);
  registry->RegisterIntegerPref(kSidebarWidthPref, kDefaultSidebarWidth);
}

}  // namespace paper
