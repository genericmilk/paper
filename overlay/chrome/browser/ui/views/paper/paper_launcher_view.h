// Copyright 2026 The Paper Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAPER_PAPER_LAUNCHER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAPER_PAPER_LAUNCHER_VIEW_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/webui/paper/paper.mojom.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class WebView;
class Widget;
}  // namespace views

namespace paper {

class PaperLauncherUI;
class PaperWindowController;

// The Cmd+T / "+" / domain-pill modal: a frameless, translucent child widget
// centred over the window, hosting chrome://paper-launcher.top-chrome. The
// WebContents is created once and kept warm, so reopening is instant.
class PaperLauncherView : public WebUIContentsWrapper::Host,
                          public views::WidgetObserver {
 public:
  explicit PaperLauncherView(PaperWindowController* controller);
  PaperLauncherView(const PaperLauncherView&) = delete;
  PaperLauncherView& operator=(const PaperLauncherView&) = delete;
  ~PaperLauncherView() override;

  void Show(mojom::LauncherMode mode);
  void Hide();
  bool IsVisible() const;

  mojom::LauncherMode mode() const { return mode_; }

  // WebUIContentsWrapper::Host:
  void ShowUI() override;
  void CloseUI() override;
  void ResizeDueToAutoResize(content::WebContents* source,
                             const gfx::Size& new_size) override;

  // views::WidgetObserver:
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  void OnWidgetDestroying(views::Widget* widget) override;

 private:
  void CreateWidget();
  void UpdateBounds();

  const raw_ptr<PaperWindowController> controller_;
  std::unique_ptr<WebUIContentsWrapperT<PaperLauncherUI>> contents_wrapper_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<views::WebView> web_view_ = nullptr;

  mojom::LauncherMode mode_ = mojom::LauncherMode::kNewTab;
  // Set by Show() until the page reports it is ready (first open only).
  bool show_pending_ = false;
  gfx::Size content_size_;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};
  base::WeakPtrFactory<WebUIContentsWrapper::Host> weak_factory_{this};
};

}  // namespace paper

#endif  // CHROME_BROWSER_UI_VIEWS_PAPER_PAPER_LAUNCHER_VIEW_H_
