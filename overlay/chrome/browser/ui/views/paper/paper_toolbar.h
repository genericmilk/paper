// Copyright 2026 The Paper Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAPER_PAPER_TOOLBAR_H_
#define CHROME_BROWSER_UI_VIEWS_PAPER_PAPER_TOOLBAR_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/throb_animation.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/layout/layout_manager.h"

class BrowserView;
class GURL;
class ToolbarView;

namespace views {
class Label;
}

namespace paper {

class PaperGlassPill;

// Bare registrable domain for the pill: "reddit.com" for
// https://old.reddit.com/r/foo. chrome:// pages show their host ("settings"),
// blank and new-tab pages show "paper".
std::u16string GetDisplayDomain(const GURL& url);

// The centre pill. Shows the active tab's domain, shimmers while loading, and
// opens the launcher on the full URL when clicked.
class PaperDomainPill : public views::Button,
                        public TabStripModelObserver,
                        public content::WebContentsObserver,
                        public gfx::AnimationDelegate {
  METADATA_HEADER(PaperDomainPill, views::Button)

 public:
  explicit PaperDomainPill(BrowserView* browser_view);
  PaperDomainPill(const PaperDomainPill&) = delete;
  PaperDomainPill& operator=(const PaperDomainPill&) = delete;
  ~PaperDomainPill() override;

  // views::Button:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void Layout(PassKey) override;
  void OnPaintBackground(gfx::Canvas* canvas) override;
  void PaintButtonContents(gfx::Canvas* canvas) override;
  void StateChanged(ButtonState old_state) override;
  void OnThemeChanged() override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(TabStripModel* tab_strip_model,
                              const TabStripModelChange& change,
                              const TabStripSelectionChange& selection) override;

  // content::WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;
  void DidStartLoading() override;
  void DidStopLoading() override;

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;

 private:
  void UpdateText();
  void UpdateLoading();

  const raw_ptr<BrowserView> browser_view_;
  raw_ptr<views::Label> label_ = nullptr;
  gfx::ThrobAnimation shimmer_{this};
};

// Lays out the Paper top bar inside the stock ToolbarView. Stock children the
// layout doesn't know about get empty bounds, so they stay alive (lots of
// code anchors on them) but never take up space.
class PaperToolbarLayout : public views::LayoutManager {
 public:
  PaperToolbarLayout(views::View* left_pill,
                     views::View* domain_pill,
                     views::View* right_pill,
                     std::vector<views::View*> right_items);
  PaperToolbarLayout(const PaperToolbarLayout&) = delete;
  PaperToolbarLayout& operator=(const PaperToolbarLayout&) = delete;
  ~PaperToolbarLayout() override;

  // views::LayoutManager:
  void Layout(views::View* host) override;
  gfx::Size GetPreferredSize(const views::View* host) const override;
  gfx::Size GetPreferredSize(
      const views::View* host,
      const views::SizeBounds& available_size) const override;

 private:
  const raw_ptr<views::View> left_pill_;
  const raw_ptr<views::View> domain_pill_;
  const raw_ptr<views::View> right_pill_;
  const std::vector<raw_ptr<views::View>> right_items_;
};

}  // namespace paper

#endif  // CHROME_BROWSER_UI_VIEWS_PAPER_PAPER_TOOLBAR_H_
