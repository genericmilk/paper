// Copyright 2026 The Paper Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAPER_PAPER_GLASS_H_
#define CHROME_BROWSER_UI_VIEWS_PAPER_PAPER_GLASS_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace gfx {
class Canvas;
}

namespace paper {

// Height of every glass pill in the top bar; tall enough to hold stock
// ToolbarButtons (34 DIP) with a hairline of glass around them.
inline constexpr int kPillHeight = 36;
inline constexpr int kTopBarHeight = 48;

// Paints a liquid-glass capsule into `bounds`: translucent body, specular
// top sheen, a bright inner rim and a faint outer hairline. The window behind
// it is already macOS 26 glass (kGlassFrame), so this reads as a lens sitting
// on that material. `emphasis` in [0, 1] brightens it for hover/press.
void PaintGlassCapsule(gfx::Canvas* canvas,
                       const gfx::RectF& bounds,
                       bool dark,
                       float emphasis = 0.f);

// Same material for rounded rectangles (the launcher card, sidebar tiles).
void PaintGlassRoundRect(gfx::Canvas* canvas,
                         const gfx::RectF& bounds,
                         float radius,
                         bool dark,
                         float emphasis = 0.f);

// True when the view's color provider resolves to a dark surface.
bool IsDarkSurface(const views::View* view);

// A glass capsule that holds other views. Children are laid out left to right
// with even padding.
class PaperGlassPill : public views::View {
  METADATA_HEADER(PaperGlassPill, views::View)

 public:
  PaperGlassPill();
  PaperGlassPill(const PaperGlassPill&) = delete;
  PaperGlassPill& operator=(const PaperGlassPill&) = delete;
  ~PaperGlassPill() override;

  // views::View:
  void OnPaintBackground(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;
};

// Round icon button drawn with vector paths so it matches the pill's weight.
class PaperIconButton : public views::Button {
  METADATA_HEADER(PaperIconButton, views::Button)

 public:
  enum class Glyph { kSidebar, kPlus };

  PaperIconButton(Glyph glyph, PressedCallback callback);
  PaperIconButton(const PaperIconButton&) = delete;
  PaperIconButton& operator=(const PaperIconButton&) = delete;
  ~PaperIconButton() override;

  // views::Button:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void PaintButtonContents(gfx::Canvas* canvas) override;
  void StateChanged(ButtonState old_state) override;

 private:
  const Glyph glyph_;
};

}  // namespace paper

#endif  // CHROME_BROWSER_UI_VIEWS_PAPER_PAPER_GLASS_H_
