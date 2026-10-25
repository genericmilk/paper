// Copyright 2026 The Paper Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAPER_PAPER_SIDEBAR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAPER_PAPER_SIDEBAR_VIEW_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/resize_area_delegate.h"
#include "ui/views/view.h"

class BrowserView;

namespace views {
class ResizeArea;
class WebView;
}  // namespace views

namespace paper {

class PaperSidebarUI;
class PaperWindowController;

// The left column that holds the sidebar WebUI (essentials, bookmarks, tabs).
// BrowserViewTabbedLayoutImpl gives it GetSidebarVisibleWidth() pixels below
// the top bar and insets the page by the same amount, so opening it pushes
// the page rather than covering it.
class PaperSidebarView : public views::View,
                         public views::ResizeAreaDelegate,
                         public WebUIContentsWrapper::Host {
  METADATA_HEADER(PaperSidebarView, views::View)

 public:
  explicit PaperSidebarView(BrowserView* browser_view);
  PaperSidebarView(const PaperSidebarView&) = delete;
  PaperSidebarView& operator=(const PaperSidebarView&) = delete;
  ~PaperSidebarView() override;

  PaperWindowController* controller() { return controller_.get(); }

  // views::View:
  void AddedToWidget() override;
  void Layout(PassKey) override;

  // views::ResizeAreaDelegate:
  void OnResize(int resize_amount, bool done_resizing) override;

  // WebUIContentsWrapper::Host:
  void ShowUI() override;
  void CloseUI() override;

 private:
  std::unique_ptr<PaperWindowController> controller_;
  std::unique_ptr<WebUIContentsWrapperT<PaperSidebarUI>> contents_wrapper_;
  raw_ptr<views::WebView> web_view_ = nullptr;
  raw_ptr<views::ResizeArea> resize_area_ = nullptr;

  // Width when the current resize drag started.
  int resize_start_width_ = 0;

  base::WeakPtrFactory<WebUIContentsWrapper::Host> weak_factory_{this};
};

}  // namespace paper

#endif  // CHROME_BROWSER_UI_VIEWS_PAPER_PAPER_SIDEBAR_VIEW_H_
