// Copyright 2026 The Paper Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PAPER_PAPER_SIDEBAR_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_PAPER_PAPER_SIDEBAR_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/paper/paper_window_controller.h"
#include "chrome/browser/ui/webui/paper/paper.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class WebContents;
}

namespace paper {

class PaperSidebarHandler : public mojom::SidebarPageHandler,
                            public PaperWindowController::Observer {
 public:
  PaperSidebarHandler(
      content::WebContents* webui_contents,
      mojo::PendingRemote<mojom::SidebarPage> page,
      mojo::PendingReceiver<mojom::SidebarPageHandler> receiver);
  PaperSidebarHandler(const PaperSidebarHandler&) = delete;
  PaperSidebarHandler& operator=(const PaperSidebarHandler&) = delete;
  ~PaperSidebarHandler() override;

  // mojom::SidebarPageHandler:
  void GetState(GetStateCallback callback) override;
  void ActivateTab(int32_t tab_id) override;
  void CloseTab(int32_t tab_id) override;
  void MoveTab(int32_t tab_id, int32_t to_index) override;
  void ClearTabs() override;
  void NewTab() override;
  void OpenEssential(const std::string& id) override;
  void AddEssentialFromTab(int32_t tab_id, int32_t index) override;
  void RemoveEssential(const std::string& id) override;
  void MoveEssential(const std::string& id, int32_t to_index) override;
  void OpenBookmark(int64_t id) override;
  void BookmarkTab(int32_t tab_id, int64_t parent_id, int32_t index) override;
  void MoveBookmark(int64_t id, int64_t parent_id, int32_t index) override;
  void AddBookmarkFolder(int64_t parent_id, const std::string& title) override;
  void RenameBookmark(int64_t id, const std::string& title) override;
  void RemoveBookmark(int64_t id) override;
  void PerformHaptic(mojom::Haptic pattern) override;

  // PaperWindowController::Observer:
  void OnPaperStateChanged() override;

 private:
  PaperWindowController* controller();
  mojom::SidebarStatePtr BuildState();

  const raw_ptr<content::WebContents> webui_contents_;
  mojo::Receiver<mojom::SidebarPageHandler> receiver_;
  mojo::Remote<mojom::SidebarPage> page_;
  base::ScopedObservation<PaperWindowController,
                          PaperWindowController::Observer>
      observation_{this};
};

// Shared with the launcher handler.
void PerformMojomHaptic(mojom::Haptic pattern);

}  // namespace paper

#endif  // CHROME_BROWSER_UI_WEBUI_PAPER_PAPER_SIDEBAR_HANDLER_H_
