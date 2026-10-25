// Copyright 2026 The Paper Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAPER_PAPER_HAPTICS_H_
#define CHROME_BROWSER_UI_VIEWS_PAPER_PAPER_HAPTICS_H_

namespace paper {

// Mirrors NSHapticFeedbackPattern. The trackpad only plays these while a
// finger is on it, which is exactly when drags and clicks happen.
enum class HapticPattern {
  // Light tick: sidebar open/close, starting a drag.
  kGeneric,
  // Crisp snap: a dragged item crosses into a new slot.
  kAlignment,
  // Firmer bump: drop, entering a folder, each Clear phase.
  kLevelChange,
};

void PerformHaptic(HapticPattern pattern);

}  // namespace paper

#endif  // CHROME_BROWSER_UI_VIEWS_PAPER_PAPER_HAPTICS_H_
