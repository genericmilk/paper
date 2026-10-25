// Copyright 2026 The Paper Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/paper/paper_sidebar_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/paper/paper_sidebar_handler.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/grit/paper_resources.h"
#include "chrome/grit/paper_resources_map.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "ui/webui/webui_util.h"

namespace paper {

void SetUpPaperDataSource(content::WebUI* web_ui,
                          std::string_view host,
                          int default_resource) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source =
      content::WebUIDataSource::CreateAndAdd(profile, std::string(host));
  webui::SetupWebUIDataSource(source, kPaperResources, default_resource);

  content::URLDataSource::Add(
      profile, std::make_unique<FaviconSource>(
                   profile, chrome::FaviconUrlFormat::kFavicon2));
  content::URLDataSource::Add(profile, std::make_unique<ThemeSource>(profile));
}

PaperSidebarUIConfig::PaperSidebarUIConfig()
    : DefaultTopChromeWebUIConfig(content::kChromeUIScheme, kPaperSidebarHost) {
}

PaperSidebarUI::PaperSidebarUI(content::WebUI* web_ui)
    : TopChromeWebUIController(web_ui) {
  SetUpPaperDataSource(web_ui, kPaperSidebarHost, IDR_PAPER_SIDEBAR_HTML);
}

PaperSidebarUI::~PaperSidebarUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(PaperSidebarUI)

void PaperSidebarUI::BindInterface(
    mojo::PendingReceiver<mojom::SidebarPageHandlerFactory> receiver) {
  factory_receiver_.reset();
  factory_receiver_.Bind(std::move(receiver));
}

void PaperSidebarUI::CreatePageHandler(
    mojo::PendingRemote<mojom::SidebarPage> page,
    mojo::PendingReceiver<mojom::SidebarPageHandler> receiver) {
  handler_ = std::make_unique<PaperSidebarHandler>(
      web_ui()->GetWebContents(), std::move(page), std::move(receiver));
}

}  // namespace paper
