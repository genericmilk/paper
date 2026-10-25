import AppKit
import SwiftUI

@main
struct PaperApp: App {
    @NSApplicationDelegateAdaptor private var appDelegate: AppDelegate
    @State private var model = BrowserModel()

    var body: some Scene {
        Window("Paper", id: "main") {
            BrowserWindow()
                .environment(model)
                .frame(minWidth: 720, minHeight: 480)
        }
        // Compact unified toolbar: traffic lights and Paper's controls share one
        // centre line, and the controls stay small.
        .windowToolbarStyle(.unifiedCompact(showsTitle: false))
        .defaultSize(width: 1360, height: 860)
        .commands { PaperCommands(model: model) }
    }
}

/// Lets a bare SwiftPM executable behave like a normal app (Dock icon, menu
/// bar, keyboard focus) when run without a bundle.
final class AppDelegate: NSObject, NSApplicationDelegate {
    func applicationDidFinishLaunching(_ notification: Notification) {
        NSApp.setActivationPolicy(.regular)
        NSApp.activate()
    }

    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool { true }
}

struct BrowserWindow: View {
    @Environment(BrowserModel.self) private var model
    /// Where the web page sits, so the launcher can centre on it.
    @State private var pageFrame: CGRect = .zero
    @State private var swipe: SwipeNavigator?

    var body: some View {
        ZStack {
            HStack(spacing: 0) {
                if model.sidebarOpen {
                    Sidebar()
                        .frame(width: model.sidebarWidth)
                        .overlay(alignment: .trailing) { SidebarResizeHandle() }
                        .transition(.move(edge: .leading).combined(with: .opacity))
                }
                page
                    .padding(.leading, model.sidebarOpen ? 0 : 8)
                    .onGeometryChange(for: CGRect.self) { $0.frame(in: .named(Self.space)) } action: {
                        pageFrame = $0
                    }
            }
            .padding([.trailing, .bottom], 8)
            .padding(.top, 2)
            // Collapsed: the sidebar peeks out over the page from the left edge.
            .overlay(alignment: .leading) {
                if !model.sidebarOpen {
                    PeekSidebar()
                }
            }

            if let mode = model.launcherMode {
                Launcher(mode: mode, pageFrame: pageFrame)
                    // Fresh state (text, selection, entrance) on every open.
                    .id(model.launcherSession)
                    .transition(.opacity)
            }
        }
        .coordinateSpace(.named(Self.space))
        .onAppear { if swipe == nil { swipe = SwipeNavigator(model: model) } }
        .toolbar { PaperToolbar(model: model) }
        .toolbar(removing: .title)
        .toolbarBackgroundVisibility(.hidden, for: .windowToolbar)
        // The window itself is the glass: everything sits on it.
        .containerBackground(.thinMaterial, for: .window)
    }

    static let space = "paper.window"

    private var page: some View {
        Group {
            if let tab = model.activeTab {
                PageView(tab: tab)
            } else {
                // Empty window: the pinned launcher sits over a plain page.
                Color.clear
            }
        }
        .background(Color(nsColor: .textBackgroundColor))
        .overlay { if let swipe { SwipeIndicator(navigator: swipe) } }
        .clipShape(RoundedRectangle(cornerRadius: 12, style: .continuous))
        .shadow(color: .black.opacity(0.12), radius: 6, y: 2)
    }
}

struct PaperCommands: Commands {
    let model: BrowserModel

    var body: some Commands {
        CommandGroup(replacing: .newItem) {
            Button("New Tab") { model.showLauncher(.newTab) }
                .keyboardShortcut("t")
            Button("Open Location…") { model.showLauncher(.editCurrent) }
                .keyboardShortcut("l")
            Button("Close Tab") { model.closeActiveTab() }
                .keyboardShortcut("w")
        }
        CommandGroup(after: .sidebar) {
            Button("Toggle Sidebar") { model.toggleSidebar() }
                .keyboardShortcut("s")
            Button("Reload Page") { model.activeTab?.webView.reload() }
                .keyboardShortcut("r")
            Button("Back") { model.activeTab?.webView.goBack() }
                .keyboardShortcut("[")
            Button("Forward") { model.activeTab?.webView.goForward() }
                .keyboardShortcut("]")
            Button("Picture in Picture") { model.activeTab?.togglePictureInPicture() }
                .keyboardShortcut("p", modifiers: [.command, .option])
        }
        CommandMenu("Essentials") {
            ForEach(Array(model.essentials.prefix(9).enumerated()), id: \.element.id) { index, essential in
                Button(essential.title) { model.openEssential(essential) }
                    .keyboardShortcut(KeyEquivalent(Character("\(index + 1)")))
            }
        }
    }
}

/// With the sidebar collapsed, nudging the pointer against the left edge slides
/// it out as a floating glass panel over the page. It tucks away again shortly
/// after the pointer leaves, so a quick pass over it doesn't flicker.
struct PeekSidebar: View {
    @Environment(BrowserModel.self) private var model
    @State private var peeking = false
    @State private var hideTask: Task<Void, Never>?

    var body: some View {
        ZStack(alignment: .leading) {
            // Hot zone: the window's left gutter plus a sliver of the page.
            Color.clear
                .frame(width: 14)
                .frame(maxHeight: .infinity)
                .contentShape(Rectangle())
                .onHover { inside in if inside { show() } }

            if peeking {
                Sidebar()
                    .frame(width: model.sidebarWidth)
                    .padding(.vertical, 6)
                    .glassEffect(.regular, in: .rect(cornerRadius: 16))
                    .shadow(color: .black.opacity(0.25), radius: 24, x: 6)
                    .padding(.leading, 6)
                    .padding(.vertical, 4)
                    .onHover { inside in inside ? show() : scheduleHide() }
                    .transition(.move(edge: .leading).combined(with: .opacity))
            }
        }
        .frame(maxHeight: .infinity, alignment: .leading)
        // Opening the launcher or pinning the sidebar ends the peek.
        .onChange(of: model.launcherMode != nil) { _, open in if open { hide() } }
    }

    private func show() {
        hideTask?.cancel()
        guard !peeking else { return }
        withAnimation(.spring(response: 0.3, dampingFraction: 0.82)) { peeking = true }
        Haptics.tick()
    }

    private func scheduleHide() {
        hideTask?.cancel()
        hideTask = Task {
            try? await Task.sleep(for: .milliseconds(280))
            guard !Task.isCancelled else { return }
            hide()
        }
    }

    private func hide() {
        hideTask?.cancel()
        withAnimation(.spring(response: 0.28, dampingFraction: 0.9)) { peeking = false }
    }
}

/// A thin strip on the sidebar's trailing edge that resizes it. Dragging past
/// the default width snaps to it with a tick; double-click resets.
struct SidebarResizeHandle: View {
    @Environment(BrowserModel.self) private var model
    @State private var startWidth: CGFloat?
    @State private var hovering = false

    private let snapDistance: CGFloat = 8

    var body: some View {
        Rectangle()
            .fill(.clear)
            .frame(width: 8)
            .overlay {
                // A faint line shows the grab point on hover / while dragging.
                Capsule()
                    .fill(.primary.opacity(hovering || startWidth != nil ? 0.18 : 0))
                    .frame(width: 2)
                    .padding(.vertical, 12)
            }
            .contentShape(Rectangle())
            .offset(x: 4)
            .pointerStyle(.frameResize(position: .trailing))
            .onHover { hovering = $0 }
            .animation(.easeOut(duration: 0.15), value: hovering)
            .gesture(
                // The sidebar starts at the window's leading edge, so its width
                // is simply where the pointer is: no deltas to accumulate.
                DragGesture(minimumDistance: 1, coordinateSpace: .named(BrowserWindow.space))
                    .onChanged { value in
                        if startWidth == nil { startWidth = model.sidebarWidth }
                        let range = BrowserModel.sidebarWidthRange
                        var width = min(max(value.location.x, range.lowerBound), range.upperBound)
                        // Magnetic default width.
                        let snapped = abs(width - BrowserModel.defaultSidebarWidth) < snapDistance
                        if snapped { width = BrowserModel.defaultSidebarWidth }
                        if snapped && model.sidebarWidth != width { Haptics.snap() }
                        // A bump at the limits, so you feel the stop.
                        if (width == range.lowerBound || width == range.upperBound) && model.sidebarWidth != width {
                            Haptics.bump()
                        }
                        model.sidebarWidth = width
                    }
                    .onEnded { _ in
                        startWidth = nil
                        model.commitSidebarWidth()
                    }
            )
            .onTapGesture(count: 2) {
                withAnimation(.spring(response: 0.32, dampingFraction: 0.8)) {
                    model.sidebarWidth = BrowserModel.defaultSidebarWidth
                }
                model.commitSidebarWidth()
                Haptics.snap()
            }
            .help("Drag to resize · double-click to reset")
    }
}
