// Copyright 2026 The Paper Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/paper/paper_launcher_handler.h"

#include <utility>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_scheme_classifier.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/paper/paper_window_controller.h"
#include "chrome/browser/ui/webui/paper/paper_sidebar_handler.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_controller_config.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/autocomplete_result.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

namespace paper {

namespace {

constexpr size_t kMaxTabMatchesWhenEmpty = 6;
constexpr size_t kMaxTabMatchesWhenTyping = 3;
constexpr size_t kMaxAutocompleteMatches = 7;

mojom::MatchKind KindForMatch(const AutocompleteMatch& match) {
  if (AutocompleteMatch::IsSearchType(match.type)) {
    return mojom::MatchKind::kSearch;
  }
  if (match.type == AutocompleteMatch::Type::kUrlWhatYouTyped) {
    return mojom::MatchKind::kUrl;
  }
  return mojom::MatchKind::kHistory;
}

}  // namespace

PaperLauncherHandler::PaperLauncherHandler(
    content::WebContents* webui_contents,
    Profile* profile,
    mojo::PendingRemote<mojom::LauncherPage> page,
    mojo::PendingReceiver<mojom::LauncherPageHandler> receiver)
    : webui_contents_(webui_contents),
      profile_(profile),
      receiver_(this, std::move(receiver)),
      page_(std::move(page)) {
  AutocompleteControllerConfig config;
  config.provider_types = AutocompleteClassifier::DefaultOmniboxProviders();
  // Tabs are added by us with better titles; don't double them up.
  config.unscoped_open_tab_suggestions = false;
  autocomplete_ = std::make_unique<AutocompleteController>(
      std::make_unique<ChromeAutocompleteProviderClient>(profile_), config);
  autocomplete_observation_.Observe(autocomplete_.get());
}

PaperLauncherHandler::~PaperLauncherHandler() = default;

PaperWindowController* PaperLauncherHandler::controller() {
  return PaperWindowController::FromHostedContents(webui_contents_);
}

void PaperLauncherHandler::NotifyShown(mojom::LauncherMode mode,
                                       const std::string& initial_text) {
  mode_ = mode;
  page_->Shown(mode, initial_text);
}

void PaperLauncherHandler::Query(const std::string& text) {
  query_ = text;
  const std::u16string query = base::UTF8ToUTF16(text);

  if (base::TrimWhitespace(query, base::TRIM_ALL).empty()) {
    autocomplete_->Stop(AutocompleteStopReason::kClobbered);
    matches_.clear();
    AppendTabMatches(std::u16string(), kMaxTabMatchesWhenEmpty);
    SendResults();
    return;
  }

  AutocompleteInput input(query, metrics::OmniboxEventProto::OTHER,
                          ChromeAutocompleteSchemeClassifier(profile_));
  input.set_prevent_inline_autocomplete(true);
  autocomplete_->Start(input);
}

void PaperLauncherHandler::OnResultChanged(AutocompleteController* controller,
                                           bool default_match_changed) {
  matches_.clear();
  AppendTabMatches(base::UTF8ToUTF16(query_), kMaxTabMatchesWhenTyping);

  size_t added = 0;
  for (const AutocompleteMatch& match : controller->result()) {
    if (added == kMaxAutocompleteMatches) {
      break;
    }
    if (!match.destination_url.is_valid()) {
      continue;
    }
    auto item = mojom::LauncherMatch::New();
    item->kind = KindForMatch(match);
    if (item->kind == mojom::MatchKind::kHistory &&
        !match.description.empty()) {
      item->title = base::UTF16ToUTF8(match.description);
      item->subtitle = base::UTF16ToUTF8(match.contents);
    } else {
      item->title = base::UTF16ToUTF8(match.contents);
      item->subtitle = item->kind == mojom::MatchKind::kSearch
                           ? base::UTF16ToUTF8(match.description)
                           : std::string();
    }
    item->url = match.destination_url;
    matches_.push_back(std::move(item));
    ++added;
  }
  SendResults();
}

void PaperLauncherHandler::AppendTabMatches(const std::u16string& query,
                                            size_t limit) {
  PaperWindowController* c = controller();
  if (!c) {
    return;
  }
  const std::u16string needle = base::ToLowerASCII(query);
  tabs::TabInterface* active = c->tab_strip()->GetActiveTab();

  size_t added = 0;
  for (tabs::TabInterface* tab : c->GetListedTabs()) {
    if (added == limit) {
      break;
    }
    if (tab == active) {
      continue;
    }
    content::WebContents* contents = tab->GetContents();
    const std::u16string title = contents->GetTitle();
    const GURL& url = contents->GetVisibleURL();
    if (!needle.empty() &&
        base::ToLowerASCII(title).find(needle) == std::u16string::npos &&
        base::ToLowerASCII(base::UTF8ToUTF16(url.spec())).find(needle) ==
            std::u16string::npos) {
      continue;
    }
    auto item = mojom::LauncherMatch::New();
    item->kind = mojom::MatchKind::kTab;
    item->title = base::UTF16ToUTF8(title);
    item->subtitle = url.host();
    item->url = url;
    item->tab_id = tab->GetHandle().raw_value();
    matches_.push_back(std::move(item));
    ++added;
  }
}

void PaperLauncherHandler::SendResults() {
  std::vector<mojom::LauncherMatchPtr> copy;
  copy.reserve(matches_.size());
  for (const auto& match : matches_) {
    copy.push_back(match.Clone());
  }
  page_->ResultsChanged(query_, std::move(copy));
}

void PaperLauncherHandler::OpenMatch(int32_t index) {
  PaperWindowController* c = controller();
  if (!c || index < 0 || static_cast<size_t>(index) >= matches_.size()) {
    return;
  }
  const mojom::LauncherMatchPtr& match = matches_[index];
  c->OpenFromLauncher(mode_, match->url,
                      match->kind == mojom::MatchKind::kTab ? match->tab_id
                                                            : 0);
}

void PaperLauncherHandler::Dismiss() {
  if (PaperWindowController* c = controller()) {
    c->HideLauncher();
  }
}

void PaperLauncherHandler::PerformHaptic(mojom::Haptic pattern) {
  PerformMojomHaptic(pattern);
}

}  // namespace paper
