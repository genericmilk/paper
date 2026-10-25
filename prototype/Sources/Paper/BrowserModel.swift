import Foundation
import Observation
import SwiftUI
import WebKit

enum LauncherMode {
    /// Cmd+T / "+": the choice opens in a new tab.
    case newTab
    /// Domain pill / Cmd+L: the choice navigates the active tab.
    case editCurrent
}

/// The whole window's state. Mirrors PaperWindowController in the Chromium
/// fork: tabs, essentials and the tabs backing them, bookmarks, two-phase
/// Clear, sidebar and launcher.
@MainActor @Observable
final class BrowserModel {
    /// Listed tabs (the bottom of the sidebar), in order.
    var tabs: [BrowserTab] = []
    /// Tabs backing essentials, by essential id. Not listed, never cleared.
    var essentialTabs: [UUID: BrowserTab] = [:]
    var activeTab: BrowserTab? {
        didSet {
            guard oldValue !== activeTab else { return }
            clearArmed = false
            // Leaving a playing video floats it; coming back docks it.
            oldValue?.autoPictureInPicture()
            activeTab?.endAutoPictureInPicture()
            // No tab at all: the launcher *is* the empty window.
            if activeTab == nil && ready && launcherMode == nil { showLauncher(.newTab) }
        }
    }

    var essentials: [Essential] { didSet { save() } }
    var bookmarks: [BookmarkNode] { didSet { save() } }
    let history: HistoryStore

    var sidebarOpen: Bool { didSet { save() } }
    /// Drag-resized width. Saved when a resize ends, not on every frame.
    var sidebarWidth: CGFloat = BrowserModel.defaultSidebarWidth

    static let defaultSidebarWidth: CGFloat = 260
    static let sidebarWidthRange: ClosedRange<CGFloat> = 200...420
    var clearArmed = false

    var launcherMode: LauncherMode?
    var launcherInitialText = ""
    var launcherSession = 0

    /// Saving is off until init finishes, so property observers firing during
    /// setup can't write a half-built state.
    @ObservationIgnored private var ready = false

    init() {
        let state = Persistence.load()
        essentials = state.essentials
        bookmarks = state.bookmarks
        history = HistoryStore(entries: state.history)
        sidebarOpen = state.sidebarOpen
        sidebarWidth = state.sidebarWidth.map { CGFloat($0) }
            .map { min(max($0, Self.sidebarWidthRange.lowerBound), Self.sidebarWidthRange.upperBound) }
            ?? Self.defaultSidebarWidth
        history.onChange = { [weak self] in self?.save() }

        for url in state.openTabs {
            tabs.append(BrowserTab(url: url, model: self))
        }
        activeTab = tabs.first
        if tabs.isEmpty {
            showLauncher(.newTab)
        }
        observeAppFocus()
        ready = true
    }

    // MARK: Tabs

    @discardableResult
    func openTab(url: URL?, configuration: WKWebViewConfiguration? = nil, activate: Bool = true) -> BrowserTab {
        let tab = BrowserTab(url: url, configuration: configuration, model: self)
        // New tabs go right after the active listed tab, like Arc.
        if let active = activeTab, let i = tabs.firstIndex(where: { $0 === active }) {
            tabs.insert(tab, at: i + 1)
        } else {
            tabs.append(tab)
        }
        if activate { activeTab = tab }
        clearArmed = false
        save()
        return tab
    }

    func activate(_ tab: BrowserTab) {
        activeTab = tab
    }

    func close(_ tab: BrowserTab) {
        if let (id, _) = essentialTabs.first(where: { $0.value === tab }) {
            essentialTabs[id] = nil
        }
        guard let i = tabs.firstIndex(where: { $0 === tab }) else {
            if activeTab === tab { activeTab = tabs.last }
            return
        }
        tabs.remove(at: i)
        if activeTab === tab {
            activeTab = tabs.indices.contains(i) ? tabs[i] : tabs.last ?? essentialTabs.values.first
        }
        save()
    }

    func closeActiveTab() {
        guard let active = activeTab else { return }
        close(active)
        if activeTab == nil { showLauncher(.newTab) }
    }

    func moveTab(_ id: UUID, before target: UUID?) {
        guard let from = tabs.firstIndex(where: { $0.id == id }) else { return }
        let tab = tabs.remove(at: from)
        let to = target.flatMap { t in tabs.firstIndex(where: { $0.id == t }) } ?? tabs.count
        tabs.insert(tab, at: to)
        save()
    }

    /// First click: everything but the tab you're on. While armed, the next
    /// click takes that one too. Essentials are never cleared.
    func clearTabs() {
        let hasOthers = tabs.contains { $0 !== activeTab }
        if !clearArmed && hasOthers {
            // The active tab doesn't change, so this stays armed.
            tabs.removeAll { $0 !== activeTab }
            clearArmed = true
        } else {
            clearArmed = false
            if let active = activeTab, let i = tabs.firstIndex(where: { $0 === active }) {
                tabs.remove(at: i)
                activeTab = essentialTabs.values.first
            }
            if activeTab == nil { showLauncher(.newTab) }
        }
        Haptics.bump()
        save()
    }

    // MARK: Essentials

    func essentialTab(for essential: Essential) -> BrowserTab? {
        essentialTabs[essential.id]
    }

    func isEssentialTab(_ tab: BrowserTab) -> Bool {
        essentialTabs.values.contains { $0 === tab }
    }

    func openEssential(_ essential: Essential) {
        if let tab = essentialTabs[essential.id] {
            if activeTab === tab, tab.url?.host() != essential.url.host() {
                // Clicking the active essential after wandering off takes you home.
                tab.load(essential.url)
            }
            activeTab = tab
            return
        }
        let tab = BrowserTab(url: essential.url, model: self)
        essentialTabs[essential.id] = tab
        activeTab = tab
    }

    func makeEssential(fromTab id: UUID, at index: Int? = nil) {
        guard let i = tabs.firstIndex(where: { $0.id == id }), let url = tabs[i].url else { return }
        let tab = tabs.remove(at: i)
        let essential = Essential(title: tab.displayTitle, url: url)
        essentials.insert(essential, at: min(index ?? essentials.count, essentials.count))
        essentialTabs[essential.id] = tab
    }

    func makeEssential(fromBookmark id: UUID, at index: Int? = nil) {
        guard let node = BookmarkTree.find(bookmarks, id: id), let url = node.url else { return }
        let essential = Essential(title: node.title, url: url)
        essentials.insert(essential, at: min(index ?? essentials.count, essentials.count))
    }

    func removeEssential(_ essential: Essential) {
        essentials.removeAll { $0.id == essential.id }
        // The tab survives as an ordinary listed tab.
        if let tab = essentialTabs.removeValue(forKey: essential.id) {
            tabs.append(tab)
        }
    }

    /// Commits a reorder from the essentials grid's live drag.
    func setEssentialOrder(_ ids: [UUID]) {
        let byID = Dictionary(uniqueKeysWithValues: essentials.map { ($0.id, $0) })
        let reordered = ids.compactMap { byID[$0] }
        guard reordered.count == essentials.count else { return }
        essentials = reordered
    }

    func renameEssential(_ id: UUID, to title: String) {
        guard let i = essentials.firstIndex(where: { $0.id == id }) else { return }
        essentials[i].title = title
    }

    /// Re-homes an essential on whatever the active tab is showing.
    func setEssentialToCurrentPage(_ id: UUID) {
        guard let i = essentials.firstIndex(where: { $0.id == id }),
              let url = activeTab?.url else { return }
        essentials[i].url = url
        essentials[i].title = activeTab?.displayTitle ?? essentials[i].title
    }

    /// The active tab, if it's a listed tab that could become an essential.
    var activeListedTab: BrowserTab? {
        guard let tab = activeTab, tabs.contains(where: { $0 === tab }), tab.url != nil else { return nil }
        return tab
    }

    /// Dragged out of the grid: it becomes an ordinary tab, opening one if the
    /// essential wasn't open, and that tab becomes active.
    func moveEssentialToTabs(_ essential: Essential) {
        let tab = essentialTabs[essential.id]
        removeEssential(essential)
        if let tab {
            activeTab = tab
        } else {
            openTab(url: essential.url)
        }
    }

    func moveEssential(_ id: UUID, before target: UUID?) {
        guard let from = essentials.firstIndex(where: { $0.id == id }) else { return }
        var list = essentials
        let item = list.remove(at: from)
        let to = target.flatMap { t in list.firstIndex(where: { $0.id == t }) } ?? list.count
        list.insert(item, at: to)
        essentials = list
    }

    // MARK: Bookmarks

    func openBookmark(_ node: BookmarkNode) {
        guard let url = node.url else { return }
        openTab(url: url)
    }

    func bookmarkTab(_ id: UUID, into folder: UUID?) {
        guard let tab = tabs.first(where: { $0.id == id }) ?? activeTab, let url = tab.url else { return }
        insertBookmark(BookmarkNode(title: tab.displayTitle, url: url), into: folder, before: nil)
    }

    func addFolder(named name: String, into parent: UUID?) {
        insertBookmark(BookmarkNode(title: name), into: parent, before: nil)
    }

    func renameBookmark(_ id: UUID, to title: String) {
        BookmarkTree.update(&bookmarks, id: id) { $0.title = title }
    }

    func removeBookmark(_ id: UUID) {
        _ = BookmarkTree.remove(&bookmarks, id: id)
    }

    /// Moves `id` into `folder` (nil = top level), before `before` if given.
    func moveBookmark(_ id: UUID, into folder: UUID?, before: UUID?) {
        guard id != folder, id != before else { return }
        if let folder, let node = BookmarkTree.find(bookmarks, id: id),
           BookmarkTree.find(node.children, id: folder) != nil {
            return  // Can't move a folder into its own descendant.
        }
        var tree = bookmarks
        guard let node = BookmarkTree.remove(&tree, id: id) else { return }
        BookmarkTree.insert(&tree, node: node, into: folder, before: before)
        bookmarks = tree
    }

    private func insertBookmark(_ node: BookmarkNode, into folder: UUID?, before: UUID?) {
        var tree = bookmarks
        BookmarkTree.insert(&tree, node: node, into: folder, before: before)
        bookmarks = tree
    }

    /// The folder containing `id`, or nil at the top level.
    func parentOfBookmark(_ id: UUID) -> UUID? {
        BookmarkTree.parent(of: id, in: bookmarks, parent: nil) ?? nil
    }

    // MARK: Launcher

    func showLauncher(_ mode: LauncherMode) {
        launcherInitialText = mode == .editCurrent ? (activeTab?.url?.absoluteString ?? "") : ""
        launcherSession += 1
        withAnimation(.spring(response: 0.32, dampingFraction: 0.72)) {
            launcherMode = mode
        }
    }

    /// True when there's no tab to fall back to, so the launcher stays up.
    var launcherIsPinned: Bool { activeTab == nil }

    func hideLauncher() {
        guard !launcherIsPinned else { return }
        withAnimation(.spring(response: 0.25, dampingFraction: 0.9)) {
            launcherMode = nil
        }
    }

    func open(_ url: URL, mode: LauncherMode) {
        if mode == .editCurrent, let tab = activeTab {
            tab.load(url)
        } else {
            openTab(url: url)
        }
        hideLauncher()
    }

    // MARK: Sidebar

    func commitSidebarWidth() {
        save()
    }

    func toggleSidebar() {
        withAnimation(.spring(response: 0.34, dampingFraction: 0.82)) {
            sidebarOpen.toggle()
        }
        Haptics.tick()
    }

    // MARK: Focus

    /// Switching to another app (or minimising) floats a playing video into
    /// picture in picture; coming back docks it again.
    private func observeAppFocus() {
        let center = NotificationCenter.default
        let leave: @Sendable (Notification) -> Void = { [weak self] _ in
            MainActor.assumeIsolated { self?.activeTab?.autoPictureInPicture() }
        }
        let back: @Sendable (Notification) -> Void = { [weak self] _ in
            MainActor.assumeIsolated { self?.activeTab?.endAutoPictureInPicture() }
        }
        focusObservers = [
            center.addObserver(forName: NSApplication.didResignActiveNotification, object: nil, queue: .main, using: leave),
            center.addObserver(forName: NSWindow.didMiniaturizeNotification, object: nil, queue: .main, using: leave),
            center.addObserver(forName: NSApplication.didBecomeActiveNotification, object: nil, queue: .main, using: back),
            center.addObserver(forName: NSWindow.didDeminiaturizeNotification, object: nil, queue: .main, using: back),
        ]
    }

    @ObservationIgnored private var focusObservers: [NSObjectProtocol] = []

    // MARK: Persistence

    private func save() {
        guard ready else { return }
        Persistence.save(PersistedState(
            essentials: essentials,
            bookmarks: bookmarks,
            history: history.entries,
            sidebarOpen: sidebarOpen,
            sidebarWidth: Double(sidebarWidth),
            openTabs: tabs.compactMap(\.url)
        ))
    }
}

/// Plain recursive helpers over the bookmark tree.
enum BookmarkTree {
    static func find(_ nodes: [BookmarkNode], id: UUID) -> BookmarkNode? {
        for node in nodes {
            if node.id == id { return node }
            if let found = find(node.children, id: id) { return found }
        }
        return nil
    }

    static func update(_ nodes: inout [BookmarkNode], id: UUID, _ change: (inout BookmarkNode) -> Void) {
        for i in nodes.indices {
            if nodes[i].id == id { change(&nodes[i]); return }
            update(&nodes[i].children, id: id, change)
        }
    }

    static func remove(_ nodes: inout [BookmarkNode], id: UUID) -> BookmarkNode? {
        if let i = nodes.firstIndex(where: { $0.id == id }) {
            return nodes.remove(at: i)
        }
        for i in nodes.indices {
            if let removed = remove(&nodes[i].children, id: id) { return removed }
        }
        return nil
    }

    static func insert(_ nodes: inout [BookmarkNode], node: BookmarkNode, into folder: UUID?, before: UUID?) {
        guard let folder else {
            let i = before.flatMap { b in nodes.firstIndex(where: { $0.id == b }) } ?? nodes.count
            nodes.insert(node, at: i)
            return
        }
        for i in nodes.indices {
            if nodes[i].id == folder {
                let at = before.flatMap { b in nodes[i].children.firstIndex(where: { $0.id == b }) }
                    ?? nodes[i].children.count
                nodes[i].children.insert(node, at: at)
                return
            }
            insert(&nodes[i].children, node: node, into: folder, before: before)
        }
    }

    static func parent(of id: UUID, in nodes: [BookmarkNode], parent: UUID?) -> UUID?? {
        for node in nodes {
            if node.id == id { return .some(parent) }
            if let found = self.parent(of: id, in: node.children, parent: node.id) { return found }
        }
        return nil
    }
}
