// Copyright 2026 The Paper Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/paper/paper_toolbar.h"

#include <algorithm>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/paper/paper_glass.h"
#include "chrome/browser/ui/views/paper/paper_hooks.h"
#include "chrome/browser/ui/views/paper/paper_window_controller.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/webui/paper/paper.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace paper {

namespace {

constexpr int kEdgeMargin = 8;
constexpr int kPillGap = 10;
constexpr int kRightPillPadding = 2;
constexpr int kRightItemGap = 0;
constexpr int kDomainPillMinWidth = 132;
constexpr int kDomainPillMaxWidth = 340;
constexpr int kDomainPillHorizontalPadding = 20;
constexpr int kShimmerBandWidth = 72;
constexpr base::TimeDelta kShimmerDuration = base::Milliseconds(850);

void ToggleSidebar(BrowserWindowInterface* browser) {
  if (auto* controller = PaperWindowController::From(browser)) {
    controller->ToggleSidebar();
  }
}

void ShowLauncher(BrowserWindowInterface* browser, mojom::LauncherMode mode) {
  if (auto* controller = PaperWindowController::From(browser)) {
    controller->ShowLauncher(mode);
  }
}

}  // namespace

std::u16string GetDisplayDomain(const GURL& url) {
  if (url.is_empty() || url.IsAboutBlank() ||
      url == GURL(chrome::kChromeUINewTabURL)) {
    return u"paper";
  }
  if (url.SchemeIs(content::kChromeUIScheme)) {
    return base::UTF8ToUTF16(url.host());
  }
  if (url.SchemeIsFile()) {
    return u"file";
  }
  std::string domain = net::registry_controlled_domains::GetDomainAndRegistry(
      url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  // IP addresses, localhost and intranet hosts have no registrable domain.
  if (domain.empty()) {
    domain = url.host();
  }
  return base::UTF8ToUTF16(domain);
}

// PaperDomainPill -------------------------------------------------------------

PaperDomainPill::PaperDomainPill(BrowserView* browser_view)
    : views::Button(base::BindRepeating(
          &ShowLauncher,
          base::Unretained(browser_view->browser()),
          mojom::LauncherMode::kEditCurrent)),
      browser_view_(browser_view) {
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  GetViewAccessibility().SetName(u"Address");

  label_ = AddChildView(std::make_unique<views::Label>());
  label_->SetFontList(gfx::FontList().DeriveWithSizeDelta(0).DeriveWithWeight(
      gfx::Font::Weight::MEDIUM));
  label_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  label_->SetElideBehavior(gfx::ELIDE_HEAD);
  label_->SetAutoColorReadabilityEnabled(false);
  label_->SetSubpixelRenderingEnabled(false);
  label_->SetCanProcessEventsWithinSubtree(false);

  shimmer_.SetThrobDuration(kShimmerDuration);
  shimmer_.SetTweenType(gfx::Tween::EASE_IN_OUT);

  TabStripModel* model = browser_view_->browser()->GetTabStripModel();
  model->AddObserver(this);
  Observe(model->GetActiveWebContents());
  UpdateText();
}

PaperDomainPill::~PaperDomainPill() = default;

gfx::Size PaperDomainPill::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  const int width = label_->GetPreferredSize().width() +
                    2 * kDomainPillHorizontalPadding;
  return gfx::Size(std::clamp(width, kDomainPillMinWidth, kDomainPillMaxWidth),
                   kPillHeight);
}

void PaperDomainPill::Layout(PassKey) {
  gfx::Rect bounds = GetLocalBounds();
  bounds.Inset(gfx::Insets::VH(0, kDomainPillHorizontalPadding / 2));
  label_->SetBoundsRect(bounds);
}

void PaperDomainPill::OnPaintBackground(gfx::Canvas* canvas) {
  float emphasis = 0.f;
  if (GetState() == STATE_HOVERED) {
    emphasis = 0.6f;
  } else if (GetState() == STATE_PRESSED) {
    emphasis = 1.f;
  }
  PaintGlassCapsule(canvas, gfx::RectF(GetLocalBounds()), IsDarkSurface(this),
                    emphasis);
}

void PaperDomainPill::PaintButtonContents(gfx::Canvas* canvas) {
  if (!shimmer_.is_animating()) {
    return;
  }
  // A soft band of light sweeping across the glass while the page loads.
  const gfx::RectF bounds(GetLocalBounds());
  const float radius = bounds.height() / 2.f;
  gfx::ScopedCanvas scoped(canvas);
  canvas->ClipPath(
      SkPath::RRect(SkRRect::MakeRectXY(gfx::RectFToSkRect(bounds), radius,
                                        radius)),
      /*do_anti_alias=*/true);

  const float travel = bounds.width() + kShimmerBandWidth;
  const int band_x = base::ClampRound(bounds.x() - kShimmerBandWidth +
                                      travel * shimmer_.GetCurrentValue());
  const SkColor peak =
      SkColorSetA(SK_ColorWHITE, IsDarkSurface(this) ? 0x30 : 0x80);
  const int half = kShimmerBandWidth / 2;
  cc::PaintFlags rising;
  rising.setShader(gfx::CreateGradientShader(
      gfx::Point(band_x, 0), gfx::Point(band_x + half, 0), SK_ColorTRANSPARENT,
      peak));
  canvas->DrawRect(gfx::RectF(band_x, 0, half, bounds.height()), rising);
  cc::PaintFlags falling;
  falling.setShader(gfx::CreateGradientShader(
      gfx::Point(band_x + half, 0), gfx::Point(band_x + 2 * half, 0), peak,
      SK_ColorTRANSPARENT));
  canvas->DrawRect(gfx::RectF(band_x + half, 0, half, bounds.height()),
                   falling);
}

void PaperDomainPill::StateChanged(ButtonState old_state) {
  views::Button::StateChanged(old_state);
  SchedulePaint();
}

void PaperDomainPill::OnThemeChanged() {
  views::Button::OnThemeChanged();
  label_->SetEnabledColor(IsDarkSurface(this)
                              ? SkColorSetA(SK_ColorWHITE, 0xEB)
                              : SkColorSetA(SK_ColorBLACK, 0xD0));
}

void PaperDomainPill::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (selection.active_tab_changed()) {
    Observe(tab_strip_model->GetActiveWebContents());
    UpdateText();
    UpdateLoading();
  }
}

void PaperDomainPill::PrimaryPageChanged(content::Page& page) {
  UpdateText();
}

void PaperDomainPill::DidStartLoading() {
  UpdateText();
  UpdateLoading();
}

void PaperDomainPill::DidStopLoading() {
  UpdateText();
  UpdateLoading();
}

void PaperDomainPill::AnimationProgressed(const gfx::Animation* animation) {
  SchedulePaint();
}

void PaperDomainPill::UpdateText() {
  const GURL url = web_contents() ? web_contents()->GetVisibleURL() : GURL();
  const std::u16string domain = GetDisplayDomain(url);
  if (domain == label_->GetText()) {
    return;
  }
  label_->SetText(domain);
  GetViewAccessibility().SetName(domain);
  PreferredSizeChanged();
}

void PaperDomainPill::UpdateLoading() {
  const bool loading = web_contents() && web_contents()->IsLoading();
  if (loading && !shimmer_.is_animating()) {
    shimmer_.StartThrobbing(/*cycles_til_stop=*/-1);
  } else if (!loading && shimmer_.is_animating()) {
    shimmer_.Reset();
    SchedulePaint();
  }
}

BEGIN_METADATA(PaperDomainPill)
END_METADATA

// PaperToolbarLayout ----------------------------------------------------------

PaperToolbarLayout::PaperToolbarLayout(views::View* left_pill,
                                       views::View* domain_pill,
                                       views::View* right_pill,
                                       std::vector<views::View*> right_items)
    : left_pill_(left_pill),
      domain_pill_(domain_pill),
      right_pill_(right_pill),
      right_items_(right_items.begin(), right_items.end()) {}

PaperToolbarLayout::~PaperToolbarLayout() = default;

void PaperToolbarLayout::Layout(views::View* host) {
  const int y = (host->height() - kPillHeight) / 2;

  for (views::View* child : host->children()) {
    if (child != left_pill_ && child != domain_pill_ && child != right_pill_ &&
        !base::Contains(right_items_, child)) {
      child->SetBoundsRect(gfx::Rect());
    }
  }

  // Left: sidebar + new tab.
  left_pill_->SetBounds(kEdgeMargin, y,
                        left_pill_->GetPreferredSize().width(), kPillHeight);

  // Right: pinned extensions, toolbar actions (downloads etc.), app menu,
  // packed right to left inside one pill.
  const int right_edge = host->width() - kEdgeMargin - kRightPillPadding;
  int x = right_edge;
  for (auto it = right_items_.rbegin(); it != right_items_.rend(); ++it) {
    views::View* item = *it;
    if (!item || !item->GetVisible()) {
      continue;
    }
    const gfx::Size size = item->GetPreferredSize();
    if (size.width() == 0) {
      item->SetBoundsRect(gfx::Rect());
      continue;
    }
    x -= size.width();
    item->SetBounds(x, (host->height() - size.height()) / 2, size.width(),
                    size.height());
    x -= kRightItemGap;
  }
  x += kRightItemGap;
  right_pill_->SetBounds(x - kRightPillPadding, y,
                         right_edge - x + 2 * kRightPillPadding, kPillHeight);

  // Centre: the domain, centred on the window (not the leftover space) and
  // only nudged off-centre when the side pills would collide with it.
  int center = host->width() / 2;
  if (const views::Widget* widget = host->GetWidget()) {
    gfx::Point point(widget->GetRootView()->width() / 2, 0);
    views::View::ConvertPointToTarget(widget->GetRootView(), host, &point);
    center = point.x();
  }
  const int min_x = left_pill_->bounds().right() + kPillGap;
  const int max_right = right_pill_->x() - kPillGap;
  const int width = std::min(domain_pill_->GetPreferredSize().width(),
                             std::max(0, max_right - min_x));
  const int domain_x =
      std::clamp(center - width / 2, min_x, std::max(min_x, max_right - width));
  domain_pill_->SetBounds(domain_x, y, width, kPillHeight);
}

gfx::Size PaperToolbarLayout::GetPreferredSize(const views::View* host) const {
  int width = 2 * kEdgeMargin + 2 * kPillGap +
              left_pill_->GetPreferredSize().width() +
              domain_pill_->GetPreferredSize().width();
  for (const views::View* item : right_items_) {
    if (item && item->GetVisible()) {
      width += item->GetPreferredSize().width();
    }
  }
  return gfx::Size(width, kTopBarHeight);
}

gfx::Size PaperToolbarLayout::GetPreferredSize(
    const views::View* host,
    const views::SizeBounds& available_size) const {
  return GetPreferredSize(host);
}

// InstallPaperToolbar ---------------------------------------------------------

// `extensions`, `pinned_actions` and `app_menu` stay where they are in the view
// tree and are only positioned inside the right pill, so extension popups and
// the app menu anchor exactly as they do in stock Chromium.

void InstallPaperToolbar(ToolbarView* toolbar,
                         BrowserView* browser_view,
                         views::View* extensions,
                         views::View* pinned_actions,
                         views::View* app_menu) {
  BrowserWindowInterface* browser = browser_view->browser();

  // The window's glass frame shows through; the pills are the only surfaces.
  toolbar->SetBackground(nullptr);

  // The omnibox stays alive for the features that query it, but hidden, so
  // nothing can focus it and swallow keystrokes. The launcher replaces it.
  if (LocationBarView* location_bar = toolbar->location_bar_view()) {
    location_bar->SetVisible(false);
  }

  // Painted first so the right-hand stock buttons sit on top of it.
  auto* right_pill = toolbar->AddChildViewAt(std::make_unique<PaperGlassPill>(),
                                             /*index=*/0);
  right_pill->SetCanProcessEventsWithinSubtree(false);

  auto left_pill = std::make_unique<PaperGlassPill>();
  auto* sidebar_button =
      left_pill->AddChildView(std::make_unique<PaperIconButton>(
          PaperIconButton::Glyph::kSidebar,
          base::BindRepeating(&ToggleSidebar, base::Unretained(browser))));
  sidebar_button->SetTooltipText(u"Toggle sidebar");
  sidebar_button->GetViewAccessibility().SetName(u"Toggle sidebar");
  auto* new_tab_button =
      left_pill->AddChildView(std::make_unique<PaperIconButton>(
          PaperIconButton::Glyph::kPlus,
          base::BindRepeating(&ShowLauncher, base::Unretained(browser),
                              mojom::LauncherMode::kNewTab)));
  new_tab_button->SetTooltipText(u"New tab");
  new_tab_button->GetViewAccessibility().SetName(u"New tab");
  auto* left = toolbar->AddChildViewAt(std::move(left_pill), /*index=*/1);

  auto* domain =
      toolbar->AddChildView(std::make_unique<PaperDomainPill>(browser_view));

  toolbar->SetLayoutManager(std::make_unique<PaperToolbarLayout>(
      left, domain, right_pill,
      std::vector<views::View*>{extensions, pinned_actions, app_menu}));
}

}  // namespace paper
