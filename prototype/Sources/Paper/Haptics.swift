import AppKit

/// Trackpad feedback. These only play while a finger is on the trackpad,
/// which is exactly when drags and clicks happen.
enum Haptics {
    private static var lastAlignment = Date.distantPast

    /// Light tick: sidebar toggle, picking something up.
    static func tick() {
        perform(.generic)
    }

    /// Crisp snap: a dragged item crosses into a new slot. Rapid repeats are
    /// collapsed so a fast sweep ticks instead of buzzing.
    static func snap() {
        let now = Date()
        guard now.timeIntervalSince(lastAlignment) > 0.045 else { return }
        lastAlignment = now
        perform(.alignment)
    }

    /// Firmer bump: drop, entering a folder, each Clear phase.
    static func bump() {
        perform(.levelChange)
    }

    private static func perform(_ pattern: NSHapticFeedbackManager.FeedbackPattern) {
        NSHapticFeedbackManager.defaultPerformer.perform(pattern, performanceTime: .now)
    }
}
