// Copyright 2026 The Paper Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PAPER_PAPER_LAUNCHER_UI_H_
#define CHROME_BROWSER_UI_WEBUI_PAPER_PAPER_LAUNCHER_UI_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "chrome/browser/ui/webui/paper/paper.mojom.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"
#include "chrome/browser/ui/webui/top_chrome/top_chrome_webui_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace paper {

inline constexpr char kPaperLauncherHost[] = "paper-launcher.top-chrome";
inline constexpr char kPaperLauncherURL[] =
    "chrome://paper-launcher.top-chrome/";

class PaperLauncherHandler;
class PaperLauncherUI;

class PaperLauncherUIConfig
    : public DefaultTopChromeWebUIConfig<PaperLauncherUI> {
 public:
  PaperLauncherUIConfig();

  // DefaultTopChromeWebUIConfig:
  // The card grows and shrinks with its results.
  bool ShouldAutoResizeHost() override;
};

class PaperLauncherUI : public TopChromeWebUIController,
                        public mojom::LauncherPageHandlerFactory {
 public:
  explicit PaperLauncherUI(content::WebUI* web_ui);
  PaperLauncherUI(const PaperLauncherUI&) = delete;
  PaperLauncherUI& operator=(const PaperLauncherUI&) = delete;
  ~PaperLauncherUI() override;

  void BindInterface(
      mojo::PendingReceiver<mojom::LauncherPageHandlerFactory> receiver);

  // Resets the page for a new open. Buffered until the page connects.
  void NotifyShown(mojom::LauncherMode mode, const std::string& initial_text);

  static constexpr std::string_view GetWebUIName() { return "PaperLauncher"; }

 private:
  struct PendingShow {
    mojom::LauncherMode mode;
    std::string initial_text;
  };

  // mojom::LauncherPageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<mojom::LauncherPage> page,
      mojo::PendingReceiver<mojom::LauncherPageHandler> receiver) override;

  std::unique_ptr<PaperLauncherHandler> handler_;
  std::optional<PendingShow> pending_show_;
  mojo::Receiver<mojom::LauncherPageHandlerFactory> factory_receiver_{this};

  WEB_UI_CONTROLLER_TYPE_DECL();
};

}  // namespace paper

#endif  // CHROME_BROWSER_UI_WEBUI_PAPER_PAPER_LAUNCHER_UI_H_
