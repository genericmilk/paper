// Copyright 2026 The Paper Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/paper/paper_sidebar_view.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/paper/paper_features.h"
#include "chrome/browser/ui/views/paper/paper_hooks.h"
#include "chrome/browser/ui/views/paper/paper_window_controller.h"
#include "chrome/browser/ui/webui/paper/paper_sidebar_ui.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/resize_area.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view_class_properties.h"

namespace paper {

namespace {
constexpr int kResizeAreaWidth = 6;
}  // namespace

PaperSidebarView::PaperSidebarView(BrowserView* browser_view)
    : controller_(std::make_unique<PaperWindowController>(browser_view)) {
  web_view_ = AddChildView(
      std::make_unique<views::WebView>(browser_view->GetProfile()));
  resize_area_ = AddChildView(std::make_unique<views::ResizeArea>(this));
}

PaperSidebarView::~PaperSidebarView() {
  // The controller must outlive the WebUI that talks to it.
  web_view_->SetWebContents(nullptr);
  contents_wrapper_.reset();
}

void PaperSidebarView::AddedToWidget() {
  if (contents_wrapper_) {
    return;
  }
  contents_wrapper_ = std::make_unique<WebUIContentsWrapperT<PaperSidebarUI>>(
      GURL(kPaperSidebarURL), controller_->browser()->GetProfile(),
      // TODO(paper): add a "Paper Sidebar" task manager string.
      IDS_TASK_MANAGER_TAB_SEARCH, /*esc_closes_ui=*/false);
  contents_wrapper_->SetHost(weak_factory_.GetWeakPtr());

  content::WebContents* contents = contents_wrapper_->web_contents();
  controller_->AttachHostedContents(contents);
  // Let the window's glass show through the page.
  contents->SetPageBaseBackgroundColor(SK_ColorTRANSPARENT);
  web_view_->SetWebContents(contents);

  // A fresh window has nowhere to be yet: greet it with the launcher once the
  // window is on screen.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PaperWindowController::MaybeShowLauncherForBlankWindow,
                     controller_->GetWeakPtr()),
      base::Milliseconds(350));
}

void PaperSidebarView::Layout(PassKey) {
  // The web view keeps its full width while the column animates, so the
  // content slides in from the left edge instead of reflowing every frame.
  const int full_width = controller_->GetSidebarTargetWidth();
  web_view_->SetBounds(width() - full_width, 0, full_width, height());
  resize_area_->SetBounds(width() - kResizeAreaWidth, 0, kResizeAreaWidth,
                          height());
}

void PaperSidebarView::OnResize(int resize_amount, bool done_resizing) {
  if (resize_start_width_ == 0) {
    resize_start_width_ = controller_->GetSidebarTargetWidth();
  }
  controller_->SetSidebarWidth(resize_start_width_ + resize_amount);
  if (done_resizing) {
    resize_start_width_ = 0;
  }
}

void PaperSidebarView::ShowUI() {}

void PaperSidebarView::CloseUI() {}

BEGIN_METADATA(PaperSidebarView)
END_METADATA

std::unique_ptr<views::View> CreatePaperSidebar(BrowserView* browser_view) {
  if (!IsPaperWindow(browser_view->browser())) {
    return nullptr;
  }
  return std::make_unique<PaperSidebarView>(browser_view);
}

int GetPaperSidebarVisibleWidth(const views::View* sidebar) {
  const auto* view = views::AsViewClass<PaperSidebarView>(sidebar);
  return view ? const_cast<PaperSidebarView*>(view)
                    ->controller()
                    ->GetSidebarVisibleWidth()
              : 0;
}

}  // namespace paper
