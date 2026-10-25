import SwiftUI
import WebKit

/// The only chrome: three liquid-glass groups in the native window toolbar.
/// Using the real toolbar keeps the traffic lights and the controls on one
/// centre line, and macOS draws the glass capsules and handles window drags.
struct PaperToolbar: ToolbarContent {
    let model: BrowserModel

    var body: some ToolbarContent {
        // Left strip, right after the traffic lights.
        ToolbarItemGroup(placement: .navigation) {
            Button {
                model.toggleSidebar()
            } label: {
                Label("Toggle Sidebar", systemImage: "sidebar.left")
            }
            .help("Toggle sidebar (⌘S)")

            Button {
                model.showLauncher(.newTab)
            } label: {
                Label("New Tab", systemImage: "plus")
            }
            .help("New tab (⌘T)")
        }

        // Centre: the bare domain, centred on the window.
        ToolbarItem(placement: .principal) {
            DomainPill(model: model)
        }

        // Right strip: extensions + menu.
        ToolbarItemGroup(placement: .primaryAction) {
            ExtensionsButton()
            PageMenu(model: model)
        }
    }
}

private struct DomainPill: View {
    let model: BrowserModel
    @State private var shimmer = false

    var body: some View {
        let tab = model.activeTab
        let domain = tab?.url?.paperDomain ?? "paper"
        Button {
            model.showLauncher(.editCurrent)
        } label: {
            Text(domain)
                .font(.system(size: 12.5, weight: .semibold))
                .lineLimit(1)
                .truncationMode(.head)
                .contentTransition(.numericText())
                .frame(minWidth: 150, maxWidth: 300)
                .overlay {
                    // A band of light sweeping across while the page loads.
                    if tab?.isLoading == true {
                        GeometryReader { geo in
                            LinearGradient(colors: [.clear, .white.opacity(0.3), .clear],
                                           startPoint: .leading, endPoint: .trailing)
                                .frame(width: 60)
                                .offset(x: shimmer ? geo.size.width : -60)
                                .onAppear {
                                    shimmer = false
                                    withAnimation(.easeInOut(duration: 0.9).repeatForever(autoreverses: true)) {
                                        shimmer = true
                                    }
                                }
                        }
                        .clipShape(Capsule())
                        .allowsHitTesting(false)
                    }
                }
        }
        .animation(.snappy, value: domain)
        .help(tab?.url?.absoluteString ?? "")
    }
}

private struct ExtensionsButton: View {
    @State private var showing = false

    var body: some View {
        Button {
            showing.toggle()
        } label: {
            Label("Extensions", systemImage: "puzzlepiece.extension")
        }
        .help("Extensions")
        .popover(isPresented: $showing, arrowEdge: .bottom) {
            VStack(alignment: .leading, spacing: 6) {
                Text("Extensions").font(.headline)
                Text("Pinned extensions live here in the Chromium build. The WebKit prototype doesn't load them yet.")
                    .font(.callout).foregroundStyle(.secondary)
            }
            .padding(14).frame(width: 260)
        }
    }
}

private struct PageMenu: View {
    let model: BrowserModel

    var body: some View {
        Menu {
            let tab = model.activeTab
            Button("Back") { tab?.webView.goBack() }.disabled(tab?.canGoBack != true)
            Button("Forward") { tab?.webView.goForward() }.disabled(tab?.canGoForward != true)
            Button("Reload") { tab?.webView.reload() }.disabled(tab == nil)
            Divider()
            Button("Copy Link") {
                guard let url = tab?.url else { return }
                NSPasteboard.general.clearContents()
                NSPasteboard.general.setString(url.absoluteString, forType: .string)
            }.disabled(tab?.url == nil)
            Button("Add to Essentials") {
                if let tab { model.makeEssential(fromTab: tab.id) }
            }.disabled(tab == nil || model.isEssentialTab(tab!))
            Button("Bookmark This Tab") {
                if let tab { model.bookmarkTab(tab.id, into: nil) }
            }.disabled(tab == nil || model.isEssentialTab(tab!))
            Button("Picture in Picture  ⌥⌘P") { tab?.togglePictureInPicture() }
                .disabled(tab == nil)
            Button("Open in Safari") {
                if let url = tab?.url { NSWorkspace.shared.open(url) }
            }.disabled(tab?.url == nil)
            Divider()
            Button("Zoom In") { tab?.webView.pageZoom += 0.1 }
            Button("Zoom Out") { tab?.webView.pageZoom -= 0.1 }
            Button("Actual Size") { tab?.webView.pageZoom = 1 }
        } label: {
            Label("More", systemImage: "ellipsis")
        }
        .menuIndicator(.hidden)
        .help("More")
    }
}
