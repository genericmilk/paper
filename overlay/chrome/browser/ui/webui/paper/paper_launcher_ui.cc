// Copyright 2026 The Paper Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/paper/paper_launcher_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/paper/paper_launcher_handler.h"
#include "chrome/browser/ui/webui/paper/paper_sidebar_ui.h"
#include "chrome/grit/paper_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/url_constants.h"

namespace paper {

PaperLauncherUIConfig::PaperLauncherUIConfig()
    : DefaultTopChromeWebUIConfig(content::kChromeUIScheme,
                                  kPaperLauncherHost) {}

bool PaperLauncherUIConfig::ShouldAutoResizeHost() {
  return true;
}

PaperLauncherUI::PaperLauncherUI(content::WebUI* web_ui)
    : TopChromeWebUIController(web_ui) {
  SetUpPaperDataSource(web_ui, kPaperLauncherHost, IDR_PAPER_LAUNCHER_HTML);
}

PaperLauncherUI::~PaperLauncherUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(PaperLauncherUI)

void PaperLauncherUI::BindInterface(
    mojo::PendingReceiver<mojom::LauncherPageHandlerFactory> receiver) {
  factory_receiver_.reset();
  factory_receiver_.Bind(std::move(receiver));
}

void PaperLauncherUI::NotifyShown(mojom::LauncherMode mode,
                                  const std::string& initial_text) {
  if (handler_) {
    handler_->NotifyShown(mode, initial_text);
  } else {
    pending_show_ = PendingShow{mode, initial_text};
  }
}

void PaperLauncherUI::CreatePageHandler(
    mojo::PendingRemote<mojom::LauncherPage> page,
    mojo::PendingReceiver<mojom::LauncherPageHandler> receiver) {
  handler_ = std::make_unique<PaperLauncherHandler>(
      web_ui()->GetWebContents(), Profile::FromWebUI(web_ui()),
      std::move(page), std::move(receiver));
  if (pending_show_) {
    handler_->NotifyShown(pending_show_->mode, pending_show_->initial_text);
    pending_show_.reset();
  }
  // The page is live: let the host put it on screen.
  if (embedder()) {
    embedder()->ShowUI();
  }
}

}  // namespace paper
