// Copyright 2026 The Paper Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/paper/paper_glass.h"

#include <algorithm>

#include "base/numerics/safe_conversions.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/views/layout/box_layout.h"

namespace paper {

namespace {

constexpr int kPillPadding = 3;
constexpr int kIconButtonSize = 30;

SkColor WithAlpha(SkColor color, float alpha) {
  return SkColorSetA(color, static_cast<U8CPU>(std::clamp(alpha, 0.f, 1.f) *
                                               255.f));
}

}  // namespace

void PaintGlassRoundRect(gfx::Canvas* canvas,
                         const gfx::RectF& bounds,
                         float radius,
                         bool dark,
                         float emphasis) {
  // Body: a milky lens over the window glass. Dark mode keeps it subtle so the
  // material beneath still shows.
  cc::PaintFlags body;
  body.setAntiAlias(true);
  body.setStyle(cc::PaintFlags::kFill_Style);
  body.setColor(dark ? WithAlpha(SK_ColorWHITE, 0.08f + 0.06f * emphasis)
                     : WithAlpha(SK_ColorWHITE, 0.50f + 0.15f * emphasis));
  canvas->DrawRoundRect(bounds, radius, body);

  // Specular sheen across the top half, fading out by the middle.
  cc::PaintFlags sheen;
  sheen.setAntiAlias(true);
  sheen.setShader(gfx::CreateGradientShader(
      gfx::Point(0, base::ClampRound(bounds.y())), gfx::Point(0, base::ClampRound(bounds.CenterPoint().y())),
      WithAlpha(SK_ColorWHITE, dark ? 0.14f : 0.45f),
      WithAlpha(SK_ColorWHITE, 0.f)));
  canvas->DrawRoundRect(bounds, radius, sheen);

  // Inner rim: bright at the top edge, nearly gone at the bottom. That is the
  // cue that makes it read as a thick piece of glass rather than a flat chip.
  gfx::RectF rim_bounds = bounds;
  rim_bounds.Inset(0.5f);
  cc::PaintFlags rim;
  rim.setAntiAlias(true);
  rim.setStyle(cc::PaintFlags::kStroke_Style);
  rim.setStrokeWidth(1.f);
  rim.setShader(gfx::CreateGradientShader(
      gfx::Point(0, base::ClampRound(bounds.y())), gfx::Point(0, base::ClampRound(bounds.bottom())),
      WithAlpha(SK_ColorWHITE, dark ? 0.32f : 0.95f),
      WithAlpha(SK_ColorWHITE, dark ? 0.04f : 0.25f)));
  canvas->DrawRoundRect(rim_bounds, std::max(0.f, radius - 0.5f), rim);

  // Outer hairline so the pill separates from bright page-colored frames.
  cc::PaintFlags hairline;
  hairline.setAntiAlias(true);
  hairline.setStyle(cc::PaintFlags::kStroke_Style);
  hairline.setStrokeWidth(1.f);
  hairline.setColor(dark ? WithAlpha(SK_ColorBLACK, 0.35f)
                         : WithAlpha(SK_ColorBLACK, 0.08f));
  gfx::RectF outer = bounds;
  outer.Outset(0.5f);
  canvas->DrawRoundRect(outer, radius + 0.5f, hairline);
}

void PaintGlassCapsule(gfx::Canvas* canvas,
                       const gfx::RectF& bounds,
                       bool dark,
                       float emphasis) {
  PaintGlassRoundRect(canvas, bounds, bounds.height() / 2.f, dark, emphasis);
}

bool IsDarkSurface(const views::View* view) {
  const ui::ColorProvider* provider = view->GetColorProvider();
  return provider &&
         color_utils::IsDark(provider->GetColor(ui::kColorPrimaryBackground));
}

// PaperGlassPill --------------------------------------------------------------

PaperGlassPill::PaperGlassPill() {
  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets::VH(kPillPadding, kPillPadding + 2), /*between=*/2));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);
}

PaperGlassPill::~PaperGlassPill() = default;

void PaperGlassPill::OnPaintBackground(gfx::Canvas* canvas) {
  PaintGlassCapsule(canvas, gfx::RectF(GetLocalBounds()),
                    IsDarkSurface(this));
}

void PaperGlassPill::OnThemeChanged() {
  views::View::OnThemeChanged();
  SchedulePaint();
}

BEGIN_METADATA(PaperGlassPill)
END_METADATA

// PaperIconButton -------------------------------------------------------------

PaperIconButton::PaperIconButton(Glyph glyph, PressedCallback callback)
    : views::Button(std::move(callback)), glyph_(glyph) {
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  SetInstallFocusRingOnFocus(true);
}

PaperIconButton::~PaperIconButton() = default;

gfx::Size PaperIconButton::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(kIconButtonSize, kIconButtonSize);
}

void PaperIconButton::StateChanged(ButtonState old_state) {
  views::Button::StateChanged(old_state);
  SchedulePaint();
}

void PaperIconButton::PaintButtonContents(gfx::Canvas* canvas) {
  const bool dark = IsDarkSurface(this);
  const gfx::RectF bounds(GetLocalBounds());

  // Hover and press show a soft round well rather than a highlight box.
  if (GetState() == STATE_HOVERED || GetState() == STATE_PRESSED) {
    cc::PaintFlags well;
    well.setAntiAlias(true);
    well.setColor(WithAlpha(dark ? SK_ColorWHITE : SK_ColorBLACK,
                            GetState() == STATE_PRESSED ? 0.16f : 0.08f));
    canvas->DrawCircle(bounds.CenterPoint(), bounds.width() / 2.f, well);
  }

  cc::PaintFlags ink;
  ink.setAntiAlias(true);
  ink.setStyle(cc::PaintFlags::kStroke_Style);
  ink.setStrokeWidth(1.6f);
  ink.setStrokeCap(cc::PaintFlags::kRound_Cap);
  ink.setColor(dark ? WithAlpha(SK_ColorWHITE, 0.88f)
                    : WithAlpha(SK_ColorBLACK, 0.72f));

  const gfx::PointF c = bounds.CenterPoint();
  switch (glyph_) {
    case Glyph::kSidebar: {
      // A window with a left column: 16 x 13.
      const gfx::RectF frame(c.x() - 8.f, c.y() - 6.5f, 16.f, 13.f);
      canvas->DrawRoundRect(frame, 3.f, ink);
      canvas->DrawLine(gfx::PointF(frame.x() + 5.5f, frame.y()),
                       gfx::PointF(frame.x() + 5.5f, frame.bottom()), ink);
      break;
    }
    case Glyph::kPlus:
      canvas->DrawLine(gfx::PointF(c.x() - 6.f, c.y()),
                       gfx::PointF(c.x() + 6.f, c.y()), ink);
      canvas->DrawLine(gfx::PointF(c.x(), c.y() - 6.f),
                       gfx::PointF(c.x(), c.y() + 6.f), ink);
      break;
  }
}

BEGIN_METADATA(PaperIconButton)
END_METADATA

}  // namespace paper
