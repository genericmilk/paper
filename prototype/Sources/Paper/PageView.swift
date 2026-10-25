import SwiftUI
import WebKit

/// Hosts the active tab's WKWebView. Switching tabs swaps the view in place;
/// inactive web views stay alive (and keep their state) off-screen.
struct PageView: NSViewRepresentable {
    let tab: BrowserTab

    func makeNSView(context: Context) -> NSView {
        let container = NSView()
        container.wantsLayer = true
        return container
    }

    func updateNSView(_ container: NSView, context: Context) {
        guard container.subviews.first !== tab.webView else { return }
        container.subviews.forEach { $0.removeFromSuperview() }
        tab.webView.frame = container.bounds
        tab.webView.autoresizingMask = [.width, .height]
        container.addSubview(tab.webView)
        // Keyboard goes to the page you just switched to.
        DispatchQueue.main.async { container.window?.makeFirstResponder(tab.webView) }
    }
}

/// Site icon with a soft letter fallback while (or if) it fails to load.
struct Favicon: View {
    let url: URL?
    var size: CGFloat = 16

    var body: some View {
        AsyncImage(url: url?.faviconURL) { phase in
            if let image = phase.image {
                image.resizable().interpolation(.high)
            } else {
                let letter = url?.paperDomain.first.map { String($0).uppercased() } ?? "•"
                RoundedRectangle(cornerRadius: size * 0.28)
                    .fill(Color(hue: Double(abs((url?.host() ?? "").hashValue) % 360) / 360,
                                saturation: 0.55, brightness: 0.85).opacity(0.5))
                    .overlay(Text(letter).font(.system(size: size * 0.6, weight: .bold)).foregroundStyle(.white))
            }
        }
        .frame(width: size, height: size)
        .clipShape(RoundedRectangle(cornerRadius: size * 0.22))
    }
}
