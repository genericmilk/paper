// Copyright 2026 The Paper Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/paper/paper_haptics.h"

#import <AppKit/AppKit.h>

namespace paper {

void PerformHaptic(HapticPattern pattern) {
  NSHapticFeedbackPattern ns_pattern = NSHapticFeedbackPatternGeneric;
  switch (pattern) {
    case HapticPattern::kGeneric:
      ns_pattern = NSHapticFeedbackPatternGeneric;
      break;
    case HapticPattern::kAlignment:
      ns_pattern = NSHapticFeedbackPatternAlignment;
      break;
    case HapticPattern::kLevelChange:
      ns_pattern = NSHapticFeedbackPatternLevelChange;
      break;
  }
  [NSHapticFeedbackManager.defaultPerformer
      performFeedbackPattern:ns_pattern
             performanceTime:NSHapticFeedbackPerformanceTimeNow];
}

}  // namespace paper
