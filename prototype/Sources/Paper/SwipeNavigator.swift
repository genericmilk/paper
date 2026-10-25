import AppKit
import Observation
import SwiftUI
import WebKit

/// Two-finger horizontal swipes on the page go back / forward, with a glass
/// chevron that follows your fingers. The page keeps priority: if whatever is
/// under the pointer (the page or a carousel) can still scroll that way, the
/// swipe scrolls it instead.
@MainActor @Observable
final class SwipeNavigator {
    enum Direction { case back, forward }

    /// What the indicator shows. `progress` runs 0…1; at 1 a release navigates.
    private(set) var direction: Direction?
    private(set) var progress: CGFloat = 0
    /// Nowhere to go that way (no history): the chevron peeks in greyed out
    /// so the gesture is discoverable, but never fills.
    private(set) var unavailable = false

    @ObservationIgnored private weak var model: BrowserModel?
    @ObservationIgnored private var monitor: Any?

    // Per-gesture tracking.
    @ObservationIgnored private var tracking = false
    @ObservationIgnored private var decided = false
    @ObservationIgnored private var travelX: CGFloat = 0
    @ObservationIgnored private var travelY: CGFloat = 0
    /// Which ways the content under the pointer could still scroll, from JS.
    @ObservationIgnored private var pageCanScroll: (left: Bool, right: Bool)?
    @ObservationIgnored private var gestureID = 0

    /// Finger travel needed to commit.
    private let threshold: CGFloat = 140

    init(model: BrowserModel) {
        self.model = model
        monitor = NSEvent.addLocalMonitorForEvents(matching: .scrollWheel) { [weak self] event in
            MainActor.assumeIsolated { self?.handle(event) }
            return event  // Never swallow: the page still gets its scrolls.
        }
    }

    private func handle(_ event: NSEvent) {
        // Only precise (trackpad) gestures, and not the momentum after lift-off.
        guard event.hasPreciseScrollingDeltas, event.momentumPhase == [] else { return }

        switch event.phase {
        case .began:
            begin(event)
        case .changed:
            guard tracking else { return }
            change(event)
        case .ended, .cancelled:
            guard tracking else { return }
            end(commit: event.phase == .ended)
        default:
            break
        }
    }

    private func begin(_ event: NSEvent) {
        guard let tab = model?.activeTab, let point = pagePoint(for: event, in: tab.webView) else { return }
        tracking = true
        decided = false
        travelX = 0
        travelY = 0
        pageCanScroll = nil
        gestureID += 1
        let id = gestureID
        Task {
            let result = try? await tab.webView.callAsyncJavaScript(
                Self.scrollableScript, arguments: ["x": point.x, "y": point.y],
                in: nil, contentWorld: .defaultClient)
            guard id == self.gestureID, let dict = result as? [String: Bool] else { return }
            self.pageCanScroll = (dict["left"] ?? false, dict["right"] ?? false)
        }
    }

    private func change(_ event: NSEvent) {
        // Fingers-right is positive whatever the natural-scrolling setting
        // (checked on a real trackpad: synthetic events report the opposite).
        let fingerX = event.isDirectionInvertedFromDevice ? -event.scrollingDeltaX : event.scrollingDeltaX
        travelX += fingerX
        travelY += abs(event.scrollingDeltaY)

        if !decided {
            // Wait for a clear horizontal intent before claiming the gesture.
            guard abs(travelX) > 12 || travelY > 12 else { return }
            decided = true
            guard abs(travelX) > travelY * 1.5 else { return cancel() }
        }
        guard let model, let tab = model.activeTab else { return cancel() }

        let wanted: Direction = travelX > 0 ? .back : .forward
        // Fingers right reveals what's to the left: blocked if that can scroll.
        let blocked = wanted == .back ? (pageCanScroll?.left ?? true) : (pageCanScroll?.right ?? true)
        let possible = wanted == .back ? tab.canGoBack : tab.canGoForward
        guard !blocked else {
            // The page scrolls instead; get out of the way.
            if direction != nil { withAnimation(.spring(response: 0.25)) { direction = nil; progress = 0 } }
            return
        }
        guard possible else {
            unavailable = true
            if direction != wanted { direction = wanted }
            progress = min(abs(travelX) / threshold, 1) * 0.4
            return
        }
        unavailable = false

        let newProgress = min(abs(travelX) / threshold, 1.25)
        if progress < 1 && newProgress >= 1 { Haptics.snap() }
        if direction != wanted { direction = wanted }
        progress = newProgress
    }

    private func end(commit: Bool) {
        tracking = false
        if commit, !unavailable, progress >= 1, let direction, let tab = model?.activeTab {
            Haptics.bump()
            switch direction {
            case .back: tab.webView.goBack()
            case .forward: tab.webView.goForward()
            }
        }
        cancel()
    }

    private func cancel() {
        tracking = false
        withAnimation(.spring(response: 0.3, dampingFraction: 0.8)) {
            direction = nil
            progress = 0
            unavailable = false
        }
    }

    /// The event's position in the web view's viewport (top-left origin), or
    /// nil if the pointer isn't over the page.
    private func pagePoint(for event: NSEvent, in webView: WKWebView) -> CGPoint? {
        guard let window = webView.window else { return nil }
        let inWindow: NSPoint
        if let eventWindow = event.window {
            guard eventWindow === window else { return nil }
            inWindow = event.locationInWindow
        } else {
            // Window-less events (posted to the app directly) are in screen space.
            inWindow = window.convertPoint(fromScreen: event.locationInWindow)
        }
        var point = webView.convert(inWindow, from: nil)
        guard webView.bounds.contains(point) else { return nil }
        if !webView.isFlipped { point.y = webView.bounds.height - point.y }
        return point
    }

    /// Can the element under (x, y), or any scrollable ancestor or the page,
    /// still scroll left / right?
    private static let scrollableScript = """
    const room = el => ({
      left: el.scrollLeft > 0,
      right: el.scrollLeft + el.clientWidth < el.scrollWidth - 1,
    });
    let left = false, right = false;
    for (let el = document.elementFromPoint(x, y); el && el !== document.documentElement; el = el.parentElement) {
      const overflow = getComputedStyle(el).overflowX;
      if ((overflow === 'auto' || overflow === 'scroll') && el.scrollWidth > el.clientWidth) {
        const r = room(el); left ||= r.left; right ||= r.right;
      }
    }
    const page = room(document.scrollingElement ?? document.documentElement);
    return {left: left || page.left, right: right || page.right};
    """
}

/// The chevron that slides in from the page edge during a swipe.
struct SwipeIndicator: View {
    let navigator: SwipeNavigator

    var body: some View {
        let progress = min(navigator.progress, 1)
        let ready = navigator.progress >= 1
        ZStack {
            if let direction = navigator.direction {
                Image(systemName: direction == .back ? "chevron.left" : "chevron.right")
                    .font(.system(size: 17, weight: .bold))
                    .foregroundStyle(ready ? AnyShapeStyle(.white)
                                     : AnyShapeStyle(navigator.unavailable ? .tertiary : .primary))
                    .frame(width: 44, height: 44)
                    .background {
                        Circle()
                            .fill(LinearGradient(colors: [.orange, .pink], startPoint: .topLeading,
                                                 endPoint: .bottomTrailing))
                            .opacity(ready ? 0.9 : 0)
                    }
                    .glassEffect(.regular, in: .circle)
                    .scaleEffect(0.6 + 0.4 * progress + (ready ? 0.08 : 0))
                    .opacity(Double(0.3 + 0.7 * progress))
                    .offset(x: (direction == .back ? 1 : -1) * (-30 + 54 * progress))
                    .frame(maxWidth: .infinity, maxHeight: .infinity,
                           alignment: direction == .back ? .leading : .trailing)
                    .transition(.opacity)
                    .animation(.spring(response: 0.25, dampingFraction: 0.6), value: ready)
            }
        }
        .allowsHitTesting(false)
    }
}
