// Copyright 2026 The Paper Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAPER_PAPER_HOOKS_H_
#define CHROME_BROWSER_UI_VIEWS_PAPER_PAPER_HOOKS_H_

// The only Paper header that patched Chromium files include. It forward
// declares everything and pulls in no generated headers, so the patches don't
// need new GN deps. Everything here is implemented in //chrome/browser/ui.

#include <memory>

#include "chrome/browser/ui/views/paper/paper_features.h"

class BrowserView;
class BrowserWindowInterface;
class ToolbarView;

namespace content {
class WebUIBrowserInterfaceBrokerRegistry;
class WebUIConfigMap;
}  // namespace content

namespace views {
class View;
}

namespace paper {

// browser_command_controller.cc: Cmd+T, the menu's New Tab and Cmd+L open the
// launcher instead. Returns true if Paper handled `command_id`.
bool MaybeHandleBrowserCommand(BrowserWindowInterface* browser,
                               int command_id);

// browser_view.cc: the sidebar column (null for non-Paper windows).
std::unique_ptr<views::View> CreatePaperSidebar(BrowserView* browser_view);

// browser_view_tabbed_layout_impl.cc: pixels the sidebar pushes the page by.
int GetPaperSidebarVisibleWidth(const views::View* sidebar);

// toolbar_view.cc: replaces the stock toolbar layout with the three pills.
void InstallPaperToolbar(ToolbarView* toolbar,
                         BrowserView* browser_view,
                         views::View* extensions,
                         views::View* pinned_actions,
                         views::View* app_menu);

// chrome_web_ui_configs.cc and the desktop interface binders.
void RegisterWebUIConfigs(content::WebUIConfigMap& map);
void PopulateWebUIInterfaceBrokers(
    content::WebUIBrowserInterfaceBrokerRegistry& registry);

}  // namespace paper

#endif  // CHROME_BROWSER_UI_VIEWS_PAPER_PAPER_HOOKS_H_
