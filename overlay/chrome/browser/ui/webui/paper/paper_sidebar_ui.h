// Copyright 2026 The Paper Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PAPER_PAPER_SIDEBAR_UI_H_
#define CHROME_BROWSER_UI_WEBUI_PAPER_PAPER_SIDEBAR_UI_H_

#include <memory>
#include <string_view>

#include "chrome/browser/ui/webui/paper/paper.mojom.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace paper {

inline constexpr char kPaperSidebarHost[] = "paper-sidebar.top-chrome";
inline constexpr char kPaperSidebarURL[] = "chrome://paper-sidebar.top-chrome/";

class PaperSidebarHandler;
class PaperSidebarUI;

class PaperSidebarUIConfig : public DefaultTopChromeWebUIConfig<PaperSidebarUI> {
 public:
  PaperSidebarUIConfig();
};

class PaperSidebarUI : public TopChromeWebUIController,
                       public mojom::SidebarPageHandlerFactory {
 public:
  explicit PaperSidebarUI(content::WebUI* web_ui);
  PaperSidebarUI(const PaperSidebarUI&) = delete;
  PaperSidebarUI& operator=(const PaperSidebarUI&) = delete;
  ~PaperSidebarUI() override;

  void BindInterface(
      mojo::PendingReceiver<mojom::SidebarPageHandlerFactory> receiver);

  static constexpr std::string_view GetWebUIName() { return "PaperSidebar"; }

 private:
  // mojom::SidebarPageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<mojom::SidebarPage> page,
      mojo::PendingReceiver<mojom::SidebarPageHandler> receiver) override;

  std::unique_ptr<PaperSidebarHandler> handler_;
  mojo::Receiver<mojom::SidebarPageHandlerFactory> factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

// Shared by the sidebar and launcher: registers chrome://paper-* resources,
// favicons and theme colors on a new data source for `host`.
void SetUpPaperDataSource(content::WebUI* web_ui,
                          std::string_view host,
                          int default_resource);

}  // namespace paper

#endif  // CHROME_BROWSER_UI_WEBUI_PAPER_PAPER_SIDEBAR_UI_H_
