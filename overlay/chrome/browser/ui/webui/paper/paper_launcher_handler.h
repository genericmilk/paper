// Copyright 2026 The Paper Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PAPER_PAPER_LAUNCHER_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_PAPER_PAPER_LAUNCHER_HANDLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/webui/paper/paper.mojom.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;

namespace content {
class WebContents;
}

namespace paper {

class PaperWindowController;

// Feeds the launcher: open tabs first (they're the cheapest place to go), then
// Chromium's real omnibox results (history, what-you-typed, search
// suggestions) from an AutocompleteController owned by this handler.
class PaperLauncherHandler : public mojom::LauncherPageHandler,
                             public AutocompleteController::Observer {
 public:
  PaperLauncherHandler(
      content::WebContents* webui_contents,
      Profile* profile,
      mojo::PendingRemote<mojom::LauncherPage> page,
      mojo::PendingReceiver<mojom::LauncherPageHandler> receiver);
  PaperLauncherHandler(const PaperLauncherHandler&) = delete;
  PaperLauncherHandler& operator=(const PaperLauncherHandler&) = delete;
  ~PaperLauncherHandler() override;

  void NotifyShown(mojom::LauncherMode mode, const std::string& initial_text);

  // mojom::LauncherPageHandler:
  void Query(const std::string& text) override;
  void OpenMatch(int32_t index) override;
  void Dismiss() override;
  void PerformHaptic(mojom::Haptic pattern) override;

  // AutocompleteController::Observer:
  void OnResultChanged(AutocompleteController* controller,
                       bool default_match_changed) override;

 private:
  PaperWindowController* controller();
  void AppendTabMatches(const std::u16string& query, size_t limit);
  void SendResults();

  const raw_ptr<content::WebContents> webui_contents_;
  const raw_ptr<Profile> profile_;
  mojo::Receiver<mojom::LauncherPageHandler> receiver_;
  mojo::Remote<mojom::LauncherPage> page_;

  std::unique_ptr<AutocompleteController> autocomplete_;
  base::ScopedObservation<AutocompleteController,
                          AutocompleteController::Observer>
      autocomplete_observation_{this};

  mojom::LauncherMode mode_ = mojom::LauncherMode::kNewTab;
  std::string query_;
  std::vector<mojom::LauncherMatchPtr> matches_;
};

}  // namespace paper

#endif  // CHROME_BROWSER_UI_WEBUI_PAPER_PAPER_LAUNCHER_HANDLER_H_
