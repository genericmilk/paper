// Copyright 2026 The Paper Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAPER_PAPER_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PAPER_PAPER_WINDOW_CONTROLLER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/webui/paper/paper.mojom.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/slide_animation.h"
#include "url/gurl.h"

class BrowserView;
class BrowserWindowInterface;
class TabStripModel;

namespace bookmarks {
class BookmarkModel;
}

namespace content {
class WebContents;
}

namespace paper {

class PaperLauncherView;

// The brain of one Paper window: sidebar visibility, essentials and the tabs
// that back them, the two-phase Clear, and the launcher. Owned by the window's
// PaperSidebarView, so it lives exactly as long as the BrowserView's children.
class PaperWindowController : public TabStripModelObserver,
                              public bookmarks::BaseBookmarkModelObserver,
                              public gfx::AnimationDelegate {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Tabs, essentials, bookmarks or the Clear state changed. Coalesced into
    // at most one call per message loop turn.
    virtual void OnPaperStateChanged() {}
  };

  struct Essential {
    std::string id;
    std::string title;
    GURL url;
  };

  explicit PaperWindowController(BrowserView* browser_view);
  PaperWindowController(const PaperWindowController&) = delete;
  PaperWindowController& operator=(const PaperWindowController&) = delete;
  ~PaperWindowController() override;

  // Null for non-Paper windows (popups, apps) and during teardown.
  static PaperWindowController* From(BrowserWindowInterface* browser);

  // For WebUI hosted by Paper (sidebar, launcher): finds the controller of the
  // window that owns `contents`.
  static PaperWindowController* FromHostedContents(
      content::WebContents* contents);
  void AttachHostedContents(content::WebContents* contents);

  base::WeakPtr<PaperWindowController> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  BrowserView* browser_view() { return browser_view_; }
  BrowserWindowInterface* browser();
  TabStripModel* tab_strip();
  bookmarks::BookmarkModel* bookmark_model();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Sidebar ------------------------------------------------------------------

  void ToggleSidebar();
  bool IsSidebarOpen() const;
  // Current pushed width, animated between 0 and the preferred width.
  int GetSidebarVisibleWidth() const;
  int GetSidebarTargetWidth() const;
  void SetSidebarWidth(int width);

  // Launcher -----------------------------------------------------------------

  void ShowLauncher(mojom::LauncherMode mode);
  // Opens the launcher if the window's only tab is a blank/new tab page.
  void MaybeShowLauncherForBlankWindow();
  void HideLauncher();
  bool IsLauncherVisible() const;
  // Text the launcher starts with: the full URL in kEditCurrent, else empty.
  std::string GetLauncherInitialText(mojom::LauncherMode mode);

  // Opens `url` per the launcher mode, or switches to `tab_id` if positive.
  void OpenFromLauncher(mojom::LauncherMode mode, const GURL& url, int tab_id);

  // Tabs ---------------------------------------------------------------------

  // Tabs that are not backing an essential, in strip order.
  std::vector<tabs::TabInterface*> GetListedTabs();
  tabs::TabInterface* GetTabById(int tab_id);
  void ActivateTab(int tab_id);
  void CloseTab(int tab_id);
  void MoveTab(int tab_id, int to_listed_index);
  void NewTab();

  // First call closes every listed tab except the active one and arms. While
  // armed, the next call closes the active tab too.
  void ClearTabs();
  bool clear_armed() const { return clear_armed_; }

  // Essentials ---------------------------------------------------------------

  std::vector<Essential> GetEssentials() const;
  tabs::TabInterface* GetEssentialTab(const std::string& id);
  bool IsEssentialTab(const tabs::TabInterface* tab) const;
  void OpenEssential(const std::string& id);
  void AddEssentialFromTab(int tab_id, int index);
  void RemoveEssential(const std::string& id);
  void MoveEssential(const std::string& id, int to_index);

  // Bookmarks ----------------------------------------------------------------

  void OpenBookmark(int64_t id);
  void BookmarkTab(int tab_id, int64_t parent_id, int index);

  // TabStripModelObserver:
  void OnTabStripModelChanged(TabStripModel* tab_strip_model,
                              const TabStripModelChange& change,
                              const TabStripSelectionChange& selection) override;
  void OnTabChangedAt(tabs::TabInterface* tab,
                      TabChangeType change_type) override;
  void OnTabPinnedStateChanged(tabs::TabInterface* tab, int index) override;

  // bookmarks::BaseBookmarkModelObserver:
  void BookmarkModelChanged() override;

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

 private:
  void WriteEssentials(const std::vector<Essential>& essentials);

  // Matches tabs to essentials after session restore or a new window: a
  // pinned tab on an essential's origin backs it; other pinned tabs are
  // unpinned so they show in the list.
  void AdoptPinnedTabs();
  bool adopting_ = false;

  void ScheduleStateChanged();
  void NotifyStateChanged();

  const raw_ptr<BrowserView> browser_view_;

  std::map<std::string, tabs::TabHandle> essential_tabs_;

  bool clear_armed_ = false;
  bool state_change_pending_ = false;

  gfx::SlideAnimation sidebar_animation_{this};

  std::unique_ptr<PaperLauncherView> launcher_;

  base::ObserverList<Observer> observers_;

  base::ScopedObservation<bookmarks::BookmarkModel,
                          bookmarks::BookmarkModelObserver>
      bookmark_observation_{this};

  base::WeakPtrFactory<PaperWindowController> weak_factory_{this};
};

}  // namespace paper

#endif  // CHROME_BROWSER_UI_VIEWS_PAPER_PAPER_WINDOW_CONTROLLER_H_
