// Copyright 2026 The Paper Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/paper/paper_window_controller.h"

#include <algorithm>
#include <utility>

#include "base/auto_reset.h"
#include "base/functional/bind.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/supports_user_data.h"
#include "base/task/sequenced_task_runner.h"
#include "base/uuid.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/paper/paper_features.h"
#include "chrome/browser/ui/views/paper/paper_haptics.h"
#include "chrome/browser/ui/views/paper/paper_launcher_view.h"
#include "chrome/browser/ui/views/paper/paper_sidebar_view.h"
#include "chrome/common/webui_url_constants.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/animation/tween.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace paper {

namespace {

constexpr base::TimeDelta kSidebarSlideDuration = base::Milliseconds(240);

const void* const kHostedContentsKey = &kHostedContentsKey;

struct HostedContentsData : public base::SupportsUserData::Data {
  explicit HostedContentsData(base::WeakPtr<PaperWindowController> controller)
      : controller(std::move(controller)) {}
  base::WeakPtr<PaperWindowController> controller;
};

GURL TabUrl(const tabs::TabInterface* tab) {
  return tab && tab->GetContents() ? tab->GetContents()->GetVisibleURL()
                                   : GURL();
}

bool IsBlankLanding(const GURL& url) {
  return url.is_empty() || url.IsAboutBlank() ||
         url == GURL(chrome::kChromeUINewTabURL);
}

bool SameSite(const GURL& a, const GURL& b) {
  return a.is_valid() && b.is_valid() &&
         url::Origin::Create(a).IsSameOriginWith(url::Origin::Create(b));
}

}  // namespace

PaperWindowController::PaperWindowController(BrowserView* browser_view)
    : browser_view_(browser_view) {
  PrefService* prefs = browser()->GetProfile()->GetPrefs();
  sidebar_animation_.SetSlideDuration(kSidebarSlideDuration);
  sidebar_animation_.SetTweenType(gfx::Tween::ACCEL_20_DECEL_100);
  sidebar_animation_.Reset(prefs->GetBoolean(kSidebarOpenPref) ? 1.0 : 0.0);

  tab_strip()->AddObserver(this);
  if (bookmarks::BookmarkModel* model = bookmark_model()) {
    bookmark_observation_.Observe(model);
  }
  AdoptPinnedTabs();
}

PaperWindowController::~PaperWindowController() {
  launcher_.reset();
}

// static
PaperWindowController* PaperWindowController::From(
    BrowserWindowInterface* browser) {
  if (!IsPaperWindow(browser)) {
    return nullptr;
  }
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  if (!browser_view) {
    return nullptr;
  }
  auto* sidebar =
      views::AsViewClass<PaperSidebarView>(browser_view->paper_sidebar());
  return sidebar ? sidebar->controller() : nullptr;
}

// static
PaperWindowController* PaperWindowController::FromHostedContents(
    content::WebContents* contents) {
  auto* data = static_cast<HostedContentsData*>(
      contents->GetUserData(kHostedContentsKey));
  return data ? data->controller.get() : nullptr;
}

void PaperWindowController::AttachHostedContents(
    content::WebContents* contents) {
  contents->SetUserData(kHostedContentsKey,
                        std::make_unique<HostedContentsData>(
                            weak_factory_.GetWeakPtr()));
}

BrowserWindowInterface* PaperWindowController::browser() {
  return browser_view_->browser();
}

TabStripModel* PaperWindowController::tab_strip() {
  return browser()->GetTabStripModel();
}

bookmarks::BookmarkModel* PaperWindowController::bookmark_model() {
  return BookmarkModelFactory::GetForBrowserContext(browser()->GetProfile());
}

void PaperWindowController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PaperWindowController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

// Sidebar ---------------------------------------------------------------------

void PaperWindowController::ToggleSidebar() {
  const bool open = !IsSidebarOpen();
  if (open) {
    sidebar_animation_.Show();
  } else {
    sidebar_animation_.Hide();
  }
  browser()->GetProfile()->GetPrefs()->SetBoolean(kSidebarOpenPref, open);
  PerformHaptic(HapticPattern::kGeneric);
}

bool PaperWindowController::IsSidebarOpen() const {
  return sidebar_animation_.IsShowing();
}

int PaperWindowController::GetSidebarVisibleWidth() const {
  return base::ClampRound(GetSidebarTargetWidth() *
                          sidebar_animation_.GetCurrentValue());
}

int PaperWindowController::GetSidebarTargetWidth() const {
  const int width = const_cast<PaperWindowController*>(this)
                        ->browser()
                        ->GetProfile()
                        ->GetPrefs()
                        ->GetInteger(kSidebarWidthPref);
  return std::clamp(width, kMinSidebarWidth, kMaxSidebarWidth);
}

void PaperWindowController::SetSidebarWidth(int width) {
  browser()->GetProfile()->GetPrefs()->SetInteger(
      kSidebarWidthPref, std::clamp(width, kMinSidebarWidth, kMaxSidebarWidth));
  browser_view_->InvalidateLayout();
}

void PaperWindowController::AnimationProgressed(
    const gfx::Animation* animation) {
  browser_view_->InvalidateLayout();
}

void PaperWindowController::AnimationEnded(const gfx::Animation* animation) {
  browser_view_->InvalidateLayout();
}

// Launcher --------------------------------------------------------------------

void PaperWindowController::ShowLauncher(mojom::LauncherMode mode) {
  if (!launcher_) {
    launcher_ = std::make_unique<PaperLauncherView>(this);
  }
  launcher_->Show(mode);
}

void PaperWindowController::MaybeShowLauncherForBlankWindow() {
  if (tab_strip()->count() == 1 &&
      IsBlankLanding(TabUrl(tab_strip()->GetActiveTab())) &&
      browser_view_->GetWidget() && browser_view_->GetWidget()->IsVisible()) {
    ShowLauncher(mojom::LauncherMode::kNewTab);
  }
}

void PaperWindowController::HideLauncher() {
  if (launcher_) {
    launcher_->Hide();
  }
}

bool PaperWindowController::IsLauncherVisible() const {
  return launcher_ && launcher_->IsVisible();
}

std::string PaperWindowController::GetLauncherInitialText(
    mojom::LauncherMode mode) {
  if (mode != mojom::LauncherMode::kEditCurrent) {
    return std::string();
  }
  const GURL url = TabUrl(tab_strip()->GetActiveTab());
  return IsBlankLanding(url) ? std::string() : url.spec();
}

void PaperWindowController::OpenFromLauncher(mojom::LauncherMode mode,
                                             const GURL& url,
                                             int tab_id) {
  HideLauncher();
  if (tab_id > 0) {
    ActivateTab(tab_id);
    return;
  }
  if (!url.is_valid()) {
    return;
  }

  // A blank landing tab (left behind by Clear) is reused instead of piling a
  // new tab next to it.
  const bool reuse_current =
      mode == mojom::LauncherMode::kEditCurrent ||
      IsBlankLanding(TabUrl(tab_strip()->GetActiveTab()));

  NavigateParams params(browser(), url,
                        ui::PageTransitionFromInt(
                            ui::PAGE_TRANSITION_TYPED |
                            ui::PAGE_TRANSITION_FROM_ADDRESS_BAR));
  params.disposition = reuse_current
                           ? WindowOpenDisposition::CURRENT_TAB
                           : WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);
}

// Tabs ------------------------------------------------------------------------

std::vector<tabs::TabInterface*> PaperWindowController::GetListedTabs() {
  std::vector<tabs::TabInterface*> tabs;
  TabStripModel* model = tab_strip();
  for (int i = 0; i < model->count(); ++i) {
    tabs::TabInterface* tab = model->GetTabAtIndex(i);
    if (!IsEssentialTab(tab)) {
      tabs.push_back(tab);
    }
  }
  return tabs;
}

tabs::TabInterface* PaperWindowController::GetTabById(int tab_id) {
  TabStripModel* model = tab_strip();
  for (int i = 0; i < model->count(); ++i) {
    tabs::TabInterface* tab = model->GetTabAtIndex(i);
    if (tab->GetHandle().raw_value() == tab_id) {
      return tab;
    }
  }
  return nullptr;
}

void PaperWindowController::ActivateTab(int tab_id) {
  if (tabs::TabInterface* tab = GetTabById(tab_id)) {
    tab_strip()->ActivateTabAt(tab_strip()->GetIndexOfTab(tab));
  }
}

void PaperWindowController::CloseTab(int tab_id) {
  if (tabs::TabInterface* tab = GetTabById(tab_id)) {
    tab_strip()->CloseWebContentsAt(tab_strip()->GetIndexOfTab(tab),
                                    TabCloseTypes::CLOSE_USER_GESTURE);
  }
}

void PaperWindowController::MoveTab(int tab_id, int to_listed_index) {
  tabs::TabInterface* tab = GetTabById(tab_id);
  const std::vector<tabs::TabInterface*> listed = GetListedTabs();
  if (!tab || listed.empty()) {
    return;
  }
  to_listed_index =
      std::clamp(to_listed_index, 0, static_cast<int>(listed.size()) - 1);
  TabStripModel* model = tab_strip();
  model->MoveWebContentsAt(model->GetIndexOfTab(tab),
                           model->GetIndexOfTab(listed[to_listed_index]),
                           /*select_after_move=*/false);
}

void PaperWindowController::NewTab() {
  ShowLauncher(mojom::LauncherMode::kNewTab);
}

void PaperWindowController::ClearTabs() {
  TabStripModel* model = tab_strip();
  tabs::TabInterface* active = model->GetActiveTab();

  std::vector<content::WebContents*> others;
  for (tabs::TabInterface* tab : GetListedTabs()) {
    if (tab != active) {
      others.push_back(tab->GetContents());
    }
  }

  if (!clear_armed_ && !others.empty()) {
    // Phase one: everything but the tab you're looking at.
    model->CloseTabs(others, TabCloseTypes::CLOSE_USER_GESTURE);
    clear_armed_ = true;
  } else {
    // Phase two: the active tab goes too. Essentials are never cleared.
    clear_armed_ = false;
    if (active && !IsEssentialTab(active)) {
      // Never let Clear close the window: land on a blank tab and open the
      // launcher on it.
      const bool last_tab = model->count() == 1;
      if (last_tab) {
        model->delegate()->AddTabAt(GURL(url::kAboutBlankURL), -1,
                                    /*foreground=*/true);
      }
      model->CloseWebContentsAt(model->GetIndexOfTab(active),
                                TabCloseTypes::CLOSE_USER_GESTURE);
      if (last_tab) {
        ShowLauncher(mojom::LauncherMode::kNewTab);
      }
    }
  }
  PerformHaptic(HapticPattern::kLevelChange);
  ScheduleStateChanged();
}

// Essentials ------------------------------------------------------------------

std::vector<PaperWindowController::Essential>
PaperWindowController::GetEssentials() const {
  std::vector<Essential> essentials;
  const base::Value::List& list = const_cast<PaperWindowController*>(this)
                                      ->browser()
                                      ->GetProfile()
                                      ->GetPrefs()
                                      ->GetList(kEssentialsPref);
  for (const base::Value& value : list) {
    const base::Value::Dict* dict = value.GetIfDict();
    if (!dict) {
      continue;
    }
    const std::string* id = dict->FindString("id");
    const std::string* title = dict->FindString("title");
    const std::string* url = dict->FindString("url");
    if (id && url) {
      essentials.push_back({*id, title ? *title : std::string(), GURL(*url)});
    }
  }
  return essentials;
}

void PaperWindowController::WriteEssentials(
    const std::vector<Essential>& essentials) {
  base::Value::List list;
  for (const Essential& essential : essentials) {
    list.Append(base::Value::Dict()
                    .Set("id", essential.id)
                    .Set("title", essential.title)
                    .Set("url", essential.url.spec()));
  }
  browser()->GetProfile()->GetPrefs()->SetList(kEssentialsPref,
                                               std::move(list));
  ScheduleStateChanged();
}

tabs::TabInterface* PaperWindowController::GetEssentialTab(
    const std::string& id) {
  auto it = essential_tabs_.find(id);
  if (it == essential_tabs_.end()) {
    return nullptr;
  }
  tabs::TabInterface* tab = it->second.Get();
  if (!tab || tab_strip()->GetIndexOfTab(tab) == TabStripModel::kNoTab) {
    essential_tabs_.erase(it);
    return nullptr;
  }
  return tab;
}

bool PaperWindowController::IsEssentialTab(
    const tabs::TabInterface* tab) const {
  return tab && std::ranges::any_of(essential_tabs_, [tab](const auto& entry) {
           return entry.second.Get() == tab;
         });
}

void PaperWindowController::OpenEssential(const std::string& id) {
  const std::vector<Essential> essentials = GetEssentials();
  auto it = std::ranges::find(essentials, id, &Essential::id);
  if (it == essentials.end()) {
    return;
  }

  TabStripModel* model = tab_strip();
  if (tabs::TabInterface* tab = GetEssentialTab(id)) {
    if (tab == model->GetActiveTab() && !SameSite(TabUrl(tab), it->url)) {
      // Clicking the active essential after wandering off takes you home.
      NavigateParams params(browser(), it->url,
                            ui::PAGE_TRANSITION_AUTO_BOOKMARK);
      params.disposition = WindowOpenDisposition::CURRENT_TAB;
      Navigate(&params);
    } else {
      model->ActivateTabAt(model->GetIndexOfTab(tab));
    }
    return;
  }

  // Lazily create the backing tab, pinned so it sits before the listed tabs.
  model->delegate()->AddTabAt(it->url, -1, /*foreground=*/true);
  tabs::TabInterface* tab = model->GetActiveTab();
  essential_tabs_[id] = tab->GetHandle();
  model->SetTabPinned(model->GetIndexOfTab(tab), true);
  ScheduleStateChanged();
}

void PaperWindowController::AddEssentialFromTab(int tab_id, int index) {
  tabs::TabInterface* tab = GetTabById(tab_id);
  if (!tab || IsEssentialTab(tab)) {
    return;
  }
  content::WebContents* contents = tab->GetContents();
  Essential essential{base::Uuid::GenerateRandomV4().AsLowercaseString(),
                      base::UTF16ToUTF8(contents->GetTitle()),
                      contents->GetLastCommittedURL()};
  if (!essential.url.is_valid()) {
    return;
  }

  std::vector<Essential> essentials = GetEssentials();
  index = std::clamp(index, 0, static_cast<int>(essentials.size()));
  essentials.insert(essentials.begin() + index, essential);

  essential_tabs_[essential.id] = tab->GetHandle();
  tab_strip()->SetTabPinned(tab_strip()->GetIndexOfTab(tab), true);
  WriteEssentials(essentials);
}

void PaperWindowController::RemoveEssential(const std::string& id) {
  std::vector<Essential> essentials = GetEssentials();
  std::erase_if(essentials, [&id](const Essential& e) { return e.id == id; });

  // The tab survives as an ordinary listed tab.
  if (tabs::TabInterface* tab = GetEssentialTab(id)) {
    essential_tabs_.erase(id);
    tab_strip()->SetTabPinned(tab_strip()->GetIndexOfTab(tab), false);
  }
  WriteEssentials(essentials);
}

void PaperWindowController::MoveEssential(const std::string& id,
                                          int to_index) {
  std::vector<Essential> essentials = GetEssentials();
  auto it = std::ranges::find(essentials, id, &Essential::id);
  if (it == essentials.end()) {
    return;
  }
  Essential moved = *it;
  essentials.erase(it);
  to_index = std::clamp(to_index, 0, static_cast<int>(essentials.size()));
  essentials.insert(essentials.begin() + to_index, std::move(moved));
  WriteEssentials(essentials);
}

void PaperWindowController::AdoptPinnedTabs() {
  if (adopting_) {
    return;
  }
  base::AutoReset<bool> reset(&adopting_, true);

  const std::vector<Essential> essentials = GetEssentials();
  TabStripModel* model = tab_strip();
  for (int i = 0; i < model->count(); ++i) {
    tabs::TabInterface* tab = model->GetTabAtIndex(i);
    if (!model->IsTabPinned(i) || IsEssentialTab(tab)) {
      continue;
    }
    auto match = std::ranges::find_if(essentials, [&](const Essential& e) {
      return !GetEssentialTab(e.id) && SameSite(TabUrl(tab), e.url);
    });
    if (match != essentials.end()) {
      essential_tabs_[match->id] = tab->GetHandle();
    } else {
      // Pinned tabs from a stock profile have no place in Paper; list them.
      model->SetTabPinned(i, false);
    }
  }
}

// Bookmarks -------------------------------------------------------------------

void PaperWindowController::OpenBookmark(int64_t id) {
  const bookmarks::BookmarkNode* node =
      bookmarks::GetBookmarkNodeByID(bookmark_model(), id);
  if (!node || !node->is_url()) {
    return;
  }
  OpenFromLauncher(mojom::LauncherMode::kNewTab, node->url(), /*tab_id=*/0);
}

void PaperWindowController::BookmarkTab(int tab_id,
                                        int64_t parent_id,
                                        int index) {
  tabs::TabInterface* tab = GetTabById(tab_id);
  bookmarks::BookmarkModel* model = bookmark_model();
  const bookmarks::BookmarkNode* parent =
      parent_id ? bookmarks::GetBookmarkNodeByID(model, parent_id)
                : model->bookmark_bar_node();
  if (!tab || !parent || !parent->is_folder()) {
    return;
  }
  index = std::clamp(index, 0, static_cast<int>(parent->children().size()));
  model->AddNewURL(parent, index, tab->GetContents()->GetTitle(),
                   tab->GetContents()->GetLastCommittedURL());
}

// Observers -------------------------------------------------------------------

void PaperWindowController::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  // Closing tabs doesn't disarm Clear (phase one closes tabs itself); opening
  // or switching tabs means the user moved on.
  if (change.type() == TabStripModelChange::kInserted ||
      selection.active_tab_changed()) {
    clear_armed_ = false;
  }
  if (change.type() == TabStripModelChange::kInserted) {
    AdoptPinnedTabs();
  }
  ScheduleStateChanged();
}

void PaperWindowController::OnTabChangedAt(tabs::TabInterface* tab,
                                           TabChangeType change_type) {
  ScheduleStateChanged();
}

void PaperWindowController::OnTabPinnedStateChanged(tabs::TabInterface* tab,
                                                    int index) {
  AdoptPinnedTabs();
  ScheduleStateChanged();
}

void PaperWindowController::BookmarkModelChanged() {
  ScheduleStateChanged();
}

void PaperWindowController::ScheduleStateChanged() {
  if (state_change_pending_) {
    return;
  }
  state_change_pending_ = true;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&PaperWindowController::NotifyStateChanged,
                                weak_factory_.GetWeakPtr()));
}

void PaperWindowController::NotifyStateChanged() {
  state_change_pending_ = false;
  for (Observer& observer : observers_) {
    observer.OnPaperStateChanged();
  }
}

}  // namespace paper
