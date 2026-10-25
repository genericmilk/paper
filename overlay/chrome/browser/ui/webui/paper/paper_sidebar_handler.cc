// Copyright 2026 The Paper Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/paper/paper_sidebar_handler.h"

#include <algorithm>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/paper/paper_haptics.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"

namespace paper {

namespace {

// Deep enough for any sane folder tree, shallow enough to bound the message.
constexpr int kMaxBookmarkDepth = 12;

mojom::BookmarkNodePtr ToMojom(const bookmarks::BookmarkNode* node,
                               int depth) {
  auto result = mojom::BookmarkNode::New();
  result->id = node->id();
  result->title = base::UTF16ToUTF8(node->GetTitle());
  if (node->is_url()) {
    result->url = node->url();
  } else if (depth < kMaxBookmarkDepth) {
    for (const auto& child : node->children()) {
      result->children.push_back(ToMojom(child.get(), depth + 1));
    }
  }
  return result;
}

}  // namespace

void PerformMojomHaptic(mojom::Haptic pattern) {
  switch (pattern) {
    case mojom::Haptic::kGeneric:
      PerformHaptic(HapticPattern::kGeneric);
      break;
    case mojom::Haptic::kAlignment:
      PerformHaptic(HapticPattern::kAlignment);
      break;
    case mojom::Haptic::kLevelChange:
      PerformHaptic(HapticPattern::kLevelChange);
      break;
  }
}

PaperSidebarHandler::PaperSidebarHandler(
    content::WebContents* webui_contents,
    mojo::PendingRemote<mojom::SidebarPage> page,
    mojo::PendingReceiver<mojom::SidebarPageHandler> receiver)
    : webui_contents_(webui_contents),
      receiver_(this, std::move(receiver)),
      page_(std::move(page)) {
  if (PaperWindowController* c = controller()) {
    observation_.Observe(c);
  }
}

PaperSidebarHandler::~PaperSidebarHandler() = default;

PaperWindowController* PaperSidebarHandler::controller() {
  return PaperWindowController::FromHostedContents(webui_contents_);
}

mojom::SidebarStatePtr PaperSidebarHandler::BuildState() {
  auto state = mojom::SidebarState::New();
  PaperWindowController* c = controller();
  if (!c) {
    return state;
  }

  tabs::TabInterface* active = c->tab_strip()->GetActiveTab();

  for (const PaperWindowController::Essential& essential :
       c->GetEssentials()) {
    auto item = mojom::Essential::New();
    item->id = essential.id;
    item->title = essential.title;
    item->url = essential.url;
    tabs::TabInterface* tab = c->GetEssentialTab(essential.id);
    item->open = tab != nullptr;
    item->active = tab && tab == active;
    state->essentials.push_back(std::move(item));
  }

  if (bookmarks::BookmarkModel* model = c->bookmark_model();
      model && model->loaded()) {
    for (const auto& child : model->bookmark_bar_node()->children()) {
      state->bookmarks.push_back(ToMojom(child.get(), 0));
    }
  }

  for (tabs::TabInterface* tab : c->GetListedTabs()) {
    content::WebContents* contents = tab->GetContents();
    auto item = mojom::Tab::New();
    item->id = tab->GetHandle().raw_value();
    item->title = base::UTF16ToUTF8(contents->GetTitle());
    item->url = contents->GetVisibleURL();
    item->active = tab == active;
    item->loading = contents->IsLoading();
    state->tabs.push_back(std::move(item));
  }

  state->clear_armed = c->clear_armed();
  return state;
}

void PaperSidebarHandler::OnPaperStateChanged() {
  page_->StateChanged(BuildState());
}

void PaperSidebarHandler::GetState(GetStateCallback callback) {
  std::move(callback).Run(BuildState());
}

void PaperSidebarHandler::ActivateTab(int32_t tab_id) {
  if (PaperWindowController* c = controller()) {
    c->ActivateTab(tab_id);
  }
}

void PaperSidebarHandler::CloseTab(int32_t tab_id) {
  if (PaperWindowController* c = controller()) {
    c->CloseTab(tab_id);
  }
}

void PaperSidebarHandler::MoveTab(int32_t tab_id, int32_t to_index) {
  if (PaperWindowController* c = controller()) {
    c->MoveTab(tab_id, to_index);
  }
}

void PaperSidebarHandler::ClearTabs() {
  if (PaperWindowController* c = controller()) {
    c->ClearTabs();
  }
}

void PaperSidebarHandler::NewTab() {
  if (PaperWindowController* c = controller()) {
    c->NewTab();
  }
}

void PaperSidebarHandler::OpenEssential(const std::string& id) {
  if (PaperWindowController* c = controller()) {
    c->OpenEssential(id);
  }
}

void PaperSidebarHandler::AddEssentialFromTab(int32_t tab_id, int32_t index) {
  if (PaperWindowController* c = controller()) {
    c->AddEssentialFromTab(tab_id, index);
  }
}

void PaperSidebarHandler::RemoveEssential(const std::string& id) {
  if (PaperWindowController* c = controller()) {
    c->RemoveEssential(id);
  }
}

void PaperSidebarHandler::MoveEssential(const std::string& id,
                                        int32_t to_index) {
  if (PaperWindowController* c = controller()) {
    c->MoveEssential(id, to_index);
  }
}

void PaperSidebarHandler::OpenBookmark(int64_t id) {
  if (PaperWindowController* c = controller()) {
    c->OpenBookmark(id);
  }
}

void PaperSidebarHandler::BookmarkTab(int32_t tab_id,
                                      int64_t parent_id,
                                      int32_t index) {
  if (PaperWindowController* c = controller()) {
    c->BookmarkTab(tab_id, parent_id, index);
  }
}

void PaperSidebarHandler::MoveBookmark(int64_t id,
                                       int64_t parent_id,
                                       int32_t index) {
  PaperWindowController* c = controller();
  if (!c) {
    return;
  }
  bookmarks::BookmarkModel* model = c->bookmark_model();
  const bookmarks::BookmarkNode* node =
      bookmarks::GetBookmarkNodeByID(model, id);
  const bookmarks::BookmarkNode* parent =
      parent_id ? bookmarks::GetBookmarkNodeByID(model, parent_id)
                : model->bookmark_bar_node();
  // No permanent nodes, and no folder into itself or its own descendants.
  if (!node || !parent || !parent->is_folder() || node->is_permanent_node() ||
      parent == node || parent->HasAncestor(node)) {
    return;
  }
  index = std::clamp(index, 0, static_cast<int>(parent->children().size()));
  model->Move(node, parent, index);
}

void PaperSidebarHandler::AddBookmarkFolder(int64_t parent_id,
                                            const std::string& title) {
  PaperWindowController* c = controller();
  if (!c) {
    return;
  }
  bookmarks::BookmarkModel* model = c->bookmark_model();
  const bookmarks::BookmarkNode* parent =
      parent_id ? bookmarks::GetBookmarkNodeByID(model, parent_id)
                : model->bookmark_bar_node();
  if (parent && parent->is_folder()) {
    model->AddFolder(parent, parent->children().size(),
                     base::UTF8ToUTF16(title));
  }
}

void PaperSidebarHandler::RenameBookmark(int64_t id,
                                         const std::string& title) {
  PaperWindowController* c = controller();
  if (!c) {
    return;
  }
  bookmarks::BookmarkModel* model = c->bookmark_model();
  const bookmarks::BookmarkNode* node =
      bookmarks::GetBookmarkNodeByID(model, id);
  if (node && !node->is_permanent_node()) {
    model->SetTitle(node, base::UTF8ToUTF16(title),
                    bookmarks::metrics::BookmarkEditSource::kUser);
  }
}

void PaperSidebarHandler::RemoveBookmark(int64_t id) {
  PaperWindowController* c = controller();
  if (!c) {
    return;
  }
  bookmarks::BookmarkModel* model = c->bookmark_model();
  const bookmarks::BookmarkNode* node =
      bookmarks::GetBookmarkNodeByID(model, id);
  if (node && !node->is_permanent_node()) {
    model->Remove(node, bookmarks::metrics::BookmarkEditSource::kUser,
                  FROM_HERE);
  }
}

void PaperSidebarHandler::PerformHaptic(mojom::Haptic pattern) {
  PerformMojomHaptic(pattern);
}

}  // namespace paper
