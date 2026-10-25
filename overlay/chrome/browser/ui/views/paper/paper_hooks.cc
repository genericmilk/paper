// Copyright 2026 The Paper Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/paper/paper_hooks.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/views/paper/paper_window_controller.h"
#include "chrome/browser/ui/webui/paper/paper.mojom.h"
#include "chrome/browser/ui/webui/paper/paper_launcher_ui.h"
#include "chrome/browser/ui/webui/paper/paper_sidebar_ui.h"
#include "content/public/browser/web_ui_browser_interface_broker_registry.h"
#include "content/public/browser/webui_config_map.h"

namespace paper {

bool MaybeHandleBrowserCommand(BrowserWindowInterface* browser,
                               int command_id) {
  PaperWindowController* controller = PaperWindowController::From(browser);
  if (!controller) {
    return false;
  }
  switch (command_id) {
    case IDC_NEW_TAB:
      controller->ShowLauncher(mojom::LauncherMode::kNewTab);
      return true;
    case IDC_FOCUS_LOCATION:
      controller->ShowLauncher(mojom::LauncherMode::kEditCurrent);
      return true;
    default:
      return false;
  }
}

void RegisterWebUIConfigs(content::WebUIConfigMap& map) {
  map.AddWebUIConfig(std::make_unique<PaperLauncherUIConfig>());
  map.AddWebUIConfig(std::make_unique<PaperSidebarUIConfig>());
}

void PopulateWebUIInterfaceBrokers(
    content::WebUIBrowserInterfaceBrokerRegistry& registry) {
  registry.ForWebUI<PaperSidebarUI>().Add<mojom::SidebarPageHandlerFactory>();
  registry.ForWebUI<PaperLauncherUI>().Add<mojom::LauncherPageHandlerFactory>();
}

}  // namespace paper
