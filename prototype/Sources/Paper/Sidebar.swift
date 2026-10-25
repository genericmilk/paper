import SwiftUI
import UniformTypeIdentifiers

/// What's being dragged, encoded as a string so SwiftUI's drag and drop can
/// carry it: "tab:<uuid>", "essential:<uuid>" or "bookmark:<uuid>".
enum DragItem {
    case tab(UUID), essential(UUID), bookmark(UUID)

    var payload: String {
        switch self {
        case .tab(let id): "tab:\(id)"
        case .essential(let id): "essential:\(id)"
        case .bookmark(let id): "bookmark:\(id)"
        }
    }

    init?(_ payload: String) {
        let parts = payload.split(separator: ":", maxSplits: 1).map(String.init)
        guard parts.count == 2, let id = UUID(uuidString: parts[1]) else { return nil }
        switch parts[0] {
        case "tab": self = .tab(id)
        case "essential": self = .essential(id)
        case "bookmark": self = .bookmark(id)
        default: return nil
        }
    }
}

struct Sidebar: View {
    @Environment(BrowserModel.self) private var model

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 0) {
                EssentialsGrid()
                    .padding(.bottom, 14)
                BookmarkList(nodes: model.bookmarks, depth: 0, parent: nil)
                ClearDivider()
                TabList()
            }
            .padding(.horizontal, 10)
            .padding(.top, 4)
            .padding(.bottom, 16)
        }
        .scrollIndicators(.never)
        // Dropping on empty sidebar space appends to the tab list.
        .contextMenu {
            Button("New Folder") { model.addFolder(named: "New Folder", into: nil) }
        }
    }
}

// MARK: Bookmarks

private struct BookmarkList: View {
    let nodes: [BookmarkNode]
    let depth: Int
    let parent: UUID?

    var body: some View {
        ForEach(nodes) { node in
            BookmarkRow(node: node, depth: depth, parent: parent)
        }
    }
}

private struct BookmarkRow: View {
    @Environment(BrowserModel.self) private var model
    @AppStorage private var expanded: Bool
    let node: BookmarkNode
    let depth: Int
    let parent: UUID?

    @State private var dropState: DropState = .none
    @State private var renaming = false
    @State private var draftTitle = ""
    @FocusState private var titleFocused: Bool

    private enum DropState { case none, before, into }

    init(node: BookmarkNode, depth: Int, parent: UUID?) {
        self.node = node
        self.depth = depth
        self.parent = parent
        _expanded = AppStorage(wrappedValue: false, "paper.folder.\(node.id)")
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 1) {
            SidebarRow(depth: depth, highlighted: dropState == .into) {
                if node.isFolder {
                    Image(systemName: "folder.fill")
                        .font(.system(size: 13))
                        .foregroundStyle(.secondary)
                        .frame(width: 16)
                } else {
                    Favicon(url: node.url)
                }
                if renaming {
                    TextField("Name", text: $draftTitle)
                        .textFieldStyle(.plain)
                        .focused($titleFocused)
                        .onSubmit(commitRename)
                        .onExitCommand { renaming = false }
                } else {
                    Text(node.title.isEmpty ? (node.url?.paperDomain ?? "Untitled") : node.title)
                        .fontWeight(node.isFolder ? .semibold : .regular)
                        .lineLimit(1)
                }
                Spacer(minLength: 0)
                if node.isFolder {
                    Image(systemName: "chevron.down")
                        .font(.system(size: 10, weight: .bold))
                        .foregroundStyle(.secondary)
                        .rotationEffect(.degrees(expanded ? 0 : -90))
                }
            } action: {
                if node.isFolder {
                    withAnimation(.spring(response: 0.32, dampingFraction: 0.8)) { expanded.toggle() }
                } else {
                    model.openBookmark(node)
                }
            }
            .overlay(alignment: .top) {
                if dropState == .before { InsertionBar().padding(.leading, CGFloat(depth) * 14 + 8) }
            }
            .draggable(DragItem.bookmark(node.id).payload) {
                DragChip {
                    Label(node.title, systemImage: node.isFolder ? "folder.fill" : "bookmark.fill")
                        .padding(.horizontal, 12).padding(.vertical, 8)
                }
                .onAppear { Haptics.tick() }
            }
            .dropDestination(for: String.self) { items, location in
                handleDrop(items, into: node.isFolder && location.y > 9 && location.y < 25)
            } isTargeted: { isTargeted in
                let next: DropState = isTargeted ? (node.isFolder ? .into : .before) : .none
                if next != dropState, next != .none {
                    next == .into ? Haptics.bump() : Haptics.snap()
                }
                dropState = next
            }
            .contextMenu {
                if node.isFolder {
                    Button("New Folder Inside") {
                        model.addFolder(named: "New Folder", into: node.id)
                        expanded = true
                    }
                } else {
                    Button("Open") { model.openBookmark(node) }
                }
                Button("Rename") {
                    draftTitle = node.title
                    renaming = true
                    titleFocused = true
                }
                Button("Delete", role: .destructive) {
                    withAnimation(.snappy) { model.removeBookmark(node.id) }
                }
            }

            if node.isFolder && expanded {
                BookmarkList(nodes: node.children, depth: depth + 1, parent: node.id)
                    .transition(.opacity.combined(with: .move(edge: .top)))
            }
        }
    }

    private func commitRename() {
        let title = draftTitle.trimmingCharacters(in: .whitespaces)
        if !title.isEmpty { model.renameBookmark(node.id, to: title) }
        renaming = false
    }

    /// Folders take drops into themselves; anything else inserts before this row.
    private func handleDrop(_ items: [String], into: Bool) -> Bool {
        let folder = node.isFolder ? node.id : parent
        let before = node.isFolder ? nil : node.id
        switch items.first.flatMap(DragItem.init) {
        case .bookmark(let id)?:
            withAnimation(.spring(response: 0.3, dampingFraction: 0.75)) {
                model.moveBookmark(id, into: folder, before: before)
            }
        case .tab(let id)?:
            model.bookmarkTab(id, into: folder)
        default:
            return false
        }
        if node.isFolder { expanded = true }
        Haptics.bump()
        return true
    }
}

// MARK: Clear + tabs

private struct ClearDivider: View {
    @Environment(BrowserModel.self) private var model
    @State private var wobble = 0.0

    var body: some View {
        HStack(spacing: 10) {
            Rectangle().fill(.primary.opacity(0.15)).frame(height: 1)
            if !model.tabs.isEmpty {
                Button {
                    withAnimation(.spring(response: 0.35, dampingFraction: 0.75)) {
                        model.clearTabs()
                    }
                } label: {
                    Text(model.clearArmed ? "Clear all" : "Clear")
                        .font(.system(size: 12, weight: .semibold))
                        .padding(.horizontal, 9).padding(.vertical, 3)
                        .foregroundStyle(model.clearArmed ? Color.red : Color.secondary)
                        .background(Capsule().fill(model.clearArmed ? Color.red.opacity(0.12) : .clear))
                        .contentShape(Capsule())
                }
                .buttonStyle(.plain)
                .rotationEffect(.degrees(wobble))
                .transition(.scale.combined(with: .opacity))
                .help(model.clearArmed ? "Close the current tab too" : "Close every tab except this one")
            }
        }
        .frame(height: 24)
        .padding(.vertical, 8)
        .onChange(of: model.clearArmed) { _, armed in
            guard armed else { return }
            // A little wobble says "click again and this one goes too".
            withAnimation(.spring(response: 0.12, dampingFraction: 0.2)) { wobble = -7 }
            withAnimation(.spring(response: 0.35, dampingFraction: 0.35).delay(0.1)) { wobble = 0 }
        }
    }
}

private struct TabList: View {
    @Environment(BrowserModel.self) private var model
    @State private var endTargeted = false

    var body: some View {
        VStack(alignment: .leading, spacing: 1) {
            ForEach(model.tabs) { tab in
                TabRow(tab: tab)
                    .transition(.asymmetric(insertion: .move(edge: .top).combined(with: .opacity),
                                            removal: .move(edge: .leading).combined(with: .opacity)))
            }
            SidebarRow(depth: 0) {
                Image(systemName: "plus").font(.system(size: 13, weight: .medium)).frame(width: 16)
                Text("New Tab")
                Spacer(minLength: 0)
            } action: {
                model.showLauncher(.newTab)
            }
            .foregroundStyle(.secondary)
            .overlay(alignment: .top) { if endTargeted { InsertionBar() } }
            .dropDestination(for: String.self) { items, _ in
                dropOnTabs(items, before: nil)
            } isTargeted: { targeted in
                endTargeted = targeted
                if targeted { Haptics.snap() }
            }
        }
    }

    private func dropOnTabs(_ items: [String], before: UUID?) -> Bool {
        switch items.first.flatMap(DragItem.init) {
        case .tab(let id)?:
            withAnimation(.spring(response: 0.3, dampingFraction: 0.75)) { model.moveTab(id, before: before) }
        case .essential(let id)?:
            if let essential = model.essentials.first(where: { $0.id == id }) {
                withAnimation(.snappy) { model.removeEssential(essential) }
            }
        default:
            return false
        }
        Haptics.bump()
        return true
    }
}

private struct TabRow: View {
    @Environment(BrowserModel.self) private var model
    let tab: BrowserTab
    @State private var hovering = false
    @State private var targeted = false

    var body: some View {
        let active = model.activeTab === tab
        SidebarRow(depth: 0, active: active) {
            Favicon(url: tab.url)
                .opacity(tab.isLoading ? 0.5 : 1)
                .scaleEffect(tab.isLoading ? 0.9 : 1)
                .animation(tab.isLoading ? .easeInOut(duration: 0.8).repeatForever() : .default,
                           value: tab.isLoading)
            Text(tab.displayTitle)
                .lineLimit(1)
                .mask(LinearGradient(stops: [.init(color: .black, location: 0.82),
                                             .init(color: .clear, location: 1)],
                                     startPoint: .leading, endPoint: .trailing))
            Spacer(minLength: 0)
            if hovering {
                Button {
                    withAnimation(.snappy) { model.close(tab) }
                } label: {
                    Image(systemName: "xmark")
                        .font(.system(size: 9, weight: .bold))
                        .frame(width: 18, height: 18)
                        .background(Circle().fill(.primary.opacity(0.08)))
                }
                .buttonStyle(.plain)
                .transition(.scale.combined(with: .opacity))
            }
        } action: {
            model.activate(tab)
        }
        .onHover { h in withAnimation(.spring(response: 0.2, dampingFraction: 0.7)) { hovering = h } }
        .overlay(alignment: .top) { if targeted { InsertionBar() } }
        .draggable(DragItem.tab(tab.id).payload) {
            DragChip {
                HStack(spacing: 8) {
                    Favicon(url: tab.url)
                    Text(tab.displayTitle).lineLimit(1)
                }
                .padding(.horizontal, 12).padding(.vertical, 8)
                .frame(maxWidth: 240)
            }
            .onAppear { Haptics.tick() }
        }
        .dropDestination(for: String.self) { items, _ in
            switch items.first.flatMap(DragItem.init) {
            case .tab(let id)?:
                withAnimation(.spring(response: 0.3, dampingFraction: 0.75)) { model.moveTab(id, before: tab.id) }
            case .essential(let id)?:
                if let essential = model.essentials.first(where: { $0.id == id }) {
                    withAnimation(.snappy) { model.removeEssential(essential) }
                }
            default:
                return false
            }
            Haptics.bump()
            return true
        } isTargeted: { isTargeted in
            targeted = isTargeted
            if isTargeted { Haptics.snap() }
        }
        .contextMenu {
            Button("Add to Essentials") { withAnimation(.snappy) { model.makeEssential(fromTab: tab.id) } }
            Button("Bookmark") { model.bookmarkTab(tab.id, into: nil) }
            Divider()
            Button("Close", role: .destructive) { withAnimation(.snappy) { model.close(tab) } }
        }
    }
}

// MARK: Pieces

/// A 34pt sidebar row with hover and active states.
private struct SidebarRow<Content: View>: View {
    let depth: Int
    var active = false
    var highlighted = false
    @ViewBuilder let content: Content
    let action: () -> Void
    @State private var hovering = false

    var body: some View {
        Button(action: action) {
            HStack(spacing: 10) { content }
                .font(.system(size: 13.5))
                .padding(.leading, 10 + CGFloat(depth) * 14)
                .padding(.trailing, 8)
                .frame(height: 34)
                .frame(maxWidth: .infinity, alignment: .leading)
                .contentShape(RoundedRectangle(cornerRadius: 10))
        }
        .buttonStyle(PressScaleStyle())
        .background {
            if active {
                RoundedRectangle(cornerRadius: 10)
                    .fill(.clear)
                    .glassEffect(.regular, in: .rect(cornerRadius: 10))
            } else if highlighted {
                RoundedRectangle(cornerRadius: 10)
                    .fill(.orange.opacity(0.18))
                    .strokeBorder(.orange.opacity(0.6), lineWidth: 1.5)
            } else if hovering {
                RoundedRectangle(cornerRadius: 10).fill(.primary.opacity(0.06))
            }
        }
        .scaleEffect(highlighted ? 1.02 : 1)
        .animation(.spring(response: 0.25, dampingFraction: 0.65), value: highlighted)
        .onHover { hovering = $0 }
    }
}

private struct PressScaleStyle: ButtonStyle {
    func makeBody(configuration: Configuration) -> some View {
        configuration.label
            .scaleEffect(configuration.isPressed ? 0.98 : 1)
            .animation(.spring(response: 0.2, dampingFraction: 0.6), value: configuration.isPressed)
    }
}

/// The glowing line that shows where a drop will land.
private struct InsertionBar: View {
    var vertical = false

    var body: some View {
        Capsule()
            .fill(LinearGradient(colors: [.orange, .pink], startPoint: .leading, endPoint: .trailing))
            .frame(width: vertical ? 3 : nil, height: vertical ? 32 : 2.5)
            .shadow(color: .orange.opacity(0.6), radius: 5)
            .transition(.scale.combined(with: .opacity))
            .allowsHitTesting(false)
    }
}

/// Floating glass card that follows the pointer while dragging.
private struct DragChip<Content: View>: View {
    @ViewBuilder let content: Content

    var body: some View {
        content
            .font(.system(size: 13))
            .glassEffect(.regular, in: .rect(cornerRadius: 12))
            .rotationEffect(.degrees(-2))
    }
}
