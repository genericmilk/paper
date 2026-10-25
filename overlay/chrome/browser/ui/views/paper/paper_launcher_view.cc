// Copyright 2026 The Paper Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/paper/paper_launcher_view.h"

#include <algorithm>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/paper/paper_window_controller.h"
#include "chrome/browser/ui/webui/paper/paper_launcher_ui.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"

namespace paper {

namespace {

// The page draws a card inset by a transparent margin that holds its shadow,
// so these sizes include that margin.
constexpr int kLauncherWidth = 700;
constexpr int kLauncherMinHeight = 120;
constexpr int kLauncherMaxHeight = 620;

// The card's top edge sits this far down the window: high enough to feel
// like a spotlight, low enough to clear the top bar.
constexpr float kTopFraction = 0.16f;
constexpr int kMinTopOffset = 72;

}  // namespace

PaperLauncherView::PaperLauncherView(PaperWindowController* controller)
    : controller_(controller),
      content_size_(kLauncherWidth, kLauncherMinHeight) {
  contents_wrapper_ = std::make_unique<WebUIContentsWrapperT<PaperLauncherUI>>(
      GURL(kPaperLauncherURL), controller_->browser()->GetProfile(),
      // TODO(paper): add a "Paper Launcher" task manager string.
      IDS_TASK_MANAGER_TAB_SEARCH, /*esc_closes_ui=*/true);
  contents_wrapper_->SetHost(weak_factory_.GetWeakPtr());
  content::WebContents* contents = contents_wrapper_->web_contents();
  controller_->AttachHostedContents(contents);
  contents->SetPageBaseBackgroundColor(SK_ColorTRANSPARENT);
}

PaperLauncherView::~PaperLauncherView() {
  widget_observation_.Reset();
  if (web_view_) {
    web_view_->SetWebContents(nullptr);
  }
  widget_.reset();
  contents_wrapper_.reset();
}

void PaperLauncherView::Show(mojom::LauncherMode mode) {
  mode_ = mode;
  if (!widget_) {
    CreateWidget();
  }
  if (PaperLauncherUI* ui = contents_wrapper_->GetWebUIController()) {
    ui->NotifyShown(mode, controller_->GetLauncherInitialText(mode));
  }
  if (!contents_wrapper_->is_ready_to_show()) {
    // First open: wait for the page to load, then ShowUI() finishes the job.
    show_pending_ = true;
    return;
  }
  UpdateBounds();
  widget_->Show();
  widget_->Activate();
  web_view_->RequestFocus();
}

void PaperLauncherView::Hide() {
  show_pending_ = false;
  if (widget_ && widget_->IsVisible()) {
    widget_->Hide();
    // Give keyboard focus back to the page underneath.
    controller_->browser_view()->GetWidget()->Activate();
  }
}

bool PaperLauncherView::IsVisible() const {
  return widget_ && widget_->IsVisible();
}

void PaperLauncherView::ShowUI() {
  if (show_pending_) {
    show_pending_ = false;
    Show(mode_);
  }
}

void PaperLauncherView::CloseUI() {
  Hide();
}

void PaperLauncherView::ResizeDueToAutoResize(content::WebContents* source,
                                              const gfx::Size& new_size) {
  content_size_ =
      gfx::Size(kLauncherWidth, std::clamp(new_size.height(),
                                           kLauncherMinHeight,
                                           kLauncherMaxHeight));
  if (IsVisible()) {
    UpdateBounds();
  }
}

void PaperLauncherView::OnWidgetActivationChanged(views::Widget* widget,
                                                  bool active) {
  // Clicking anywhere else dismisses it, like Spotlight.
  if (!active) {
    Hide();
  }
}

void PaperLauncherView::OnWidgetDestroying(views::Widget* widget) {
  widget_observation_.Reset();
  web_view_ = nullptr;
}

void PaperLauncherView::CreateWidget() {
  BrowserView* browser_view = controller_->browser_view();

  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_POPUP);
  params.parent = browser_view->GetWidget()->GetNativeView();
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.shadow_type = views::Widget::InitParams::ShadowType::kNone;
  params.activatable = views::Widget::InitParams::Activatable::kYes;
  params.name = "PaperLauncher";

  widget_ = std::make_unique<views::Widget>();
  widget_->Init(std::move(params));
  widget_observation_.Observe(widget_.get());

  auto web_view =
      std::make_unique<views::WebView>(controller_->browser()->GetProfile());
  web_view->SetWebContents(contents_wrapper_->web_contents());
  web_view->EnableSizingFromWebContents(
      gfx::Size(kLauncherWidth, kLauncherMinHeight),
      gfx::Size(kLauncherWidth, kLauncherMaxHeight));
  web_view_ = widget_->SetContentsView(std::move(web_view));
}

void PaperLauncherView::UpdateBounds() {
  const gfx::Rect window = controller_->browser_view()->GetBoundsInScreen();
  const int top = std::max(
      kMinTopOffset, static_cast<int>(window.height() * kTopFraction));
  widget_->SetBounds(gfx::Rect(
      window.CenterPoint().x() - content_size_.width() / 2, window.y() + top,
      content_size_.width(), content_size_.height()));
}

}  // namespace paper
