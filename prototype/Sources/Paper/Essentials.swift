import SwiftUI
import UniformTypeIdentifiers

/// The Essentials grid. Tiles are laid out by hand (not LazyVGrid) so a drag
/// can reflow the others live:
/// - Drag a tile: it lifts and follows the pointer, neighbours spring out of the
///   way, and the trackpad ticks on every slot change.
/// - Pull it below the grid and let go: it goes back to being a normal tab.
/// - Drag a tab or bookmark in: a gap opens where it will land.
/// - The ＋ tile adds the current tab.
struct EssentialsGrid: View {
    @Environment(BrowserModel.self) private var model

    @State private var width: CGFloat = 240
    // Live drag of an existing tile.
    @State private var draggingID: UUID?
    @State private var liveOrder: [UUID] = []
    @State private var dragCenter: CGPoint = .zero
    @State private var grabOffset: CGSize = .zero
    @State private var releaseToTab = false
    // A tab/bookmark being dragged in from elsewhere in the sidebar.
    @State private var gapIndex: Int?
    @State private var renaming: Essential?

    static let space = "paper.essentials"
    /// Columns follow the sidebar's width: tiles stay at least `minTileWidth`
    /// wide, from 3 columns at the narrowest up to 6 when it's pulled wide.
    private var columns: Int {
        let fit = Int((width + spacing) / (minTileWidth + spacing))
        return min(6, max(3, fit))
    }
    private let minTileWidth: CGFloat = 64
    private let spacing: CGFloat = 6
    private let tileHeight: CGFloat = 36

    var body: some View {
        VStack(spacing: 6) {
            if model.essentials.isEmpty && gapIndex == nil {
                emptyState
            } else {
                grid
            }
            if releaseToTab {
                Label("Release to move to tabs", systemImage: "arrow.down.to.line")
                    .font(.system(size: 11, weight: .semibold))
                    .foregroundStyle(.orange)
                    .padding(.horizontal, 10).padding(.vertical, 4)
                    .background(Capsule().fill(.orange.opacity(0.14)))
                    .transition(.scale(scale: 0.8).combined(with: .opacity))
            }
        }
        .animation(.spring(response: 0.3, dampingFraction: 0.7), value: releaseToTab)
        // Measure the width the sidebar offers (not our own content), so the
        // grid can shrink as well as grow when the sidebar is resized.
        .frame(maxWidth: .infinity, alignment: .leading)
        .onGeometryChange(for: CGFloat.self) { $0.size.width } action: { width = $0 }
        .onDrop(of: [.utf8PlainText, .plainText], delegate: IncomingDrop(grid: self))
        .popover(item: $renaming) { essential in
            RenameEssential(essential: essential) { title in
                model.renameEssential(essential.id, to: title)
                renaming = nil
            }
        }
    }

    // MARK: Layout

    private var order: [UUID] {
        draggingID != nil ? liveOrder : model.essentials.map(\.id)
    }

    private var showsAddTile: Bool {
        draggingID == nil && gapIndex == nil && model.activeListedTab != nil && model.essentials.count < 12
    }

    private var slotCount: Int {
        order.count + (gapIndex == nil ? 0 : 1) + (showsAddTile ? 1 : 0)
    }

    private var tileWidth: CGFloat {
        (width - spacing * CGFloat(columns - 1)) / CGFloat(columns)
    }

    private var gridHeight: CGFloat {
        let rows = max(1, Int(ceil(Double(slotCount) / Double(columns))))
        return CGFloat(rows) * tileHeight + CGFloat(rows - 1) * spacing
    }

    private func center(ofSlot slot: Int) -> CGPoint {
        let col = slot % columns, row = slot / columns
        return CGPoint(x: CGFloat(col) * (tileWidth + spacing) + tileWidth / 2,
                       y: CGFloat(row) * (tileHeight + spacing) + tileHeight / 2)
    }

    /// The slot under `point`, clamped to `0...maxIndex`.
    private func slot(at point: CGPoint, maxIndex: Int) -> Int {
        let col = min(columns - 1, max(0, Int(point.x / (tileWidth + spacing))))
        let row = max(0, Int(point.y / (tileHeight + spacing)))
        return min(maxIndex, max(0, row * columns + col))
    }

    private var grid: some View {
        let essentials = Dictionary(uniqueKeysWithValues: model.essentials.map { ($0.id, $0) })
        return ZStack(alignment: .topLeading) {
            ForEach(Array(order.enumerated()), id: \.element) { index, id in
                if let essential = essentials[id] {
                    let slot = gapIndex.map { index >= $0 ? index + 1 : index } ?? index
                    let dragging = draggingID == id
                    EssentialTile(essential: essential, lifted: dragging, releasing: dragging && releaseToTab,
                                  onRename: { renaming = essential })
                        .frame(width: tileWidth, height: tileHeight)
                        .position(dragging ? dragCenter : center(ofSlot: slot))
                        .zIndex(dragging ? 1 : 0)
                        // Alongside the tile's tap: a click opens it, a drag moves it.
                        .simultaneousGesture(dragGesture(for: essential, slot: slot))
                }
            }
            if let gapIndex {
                // Where the incoming tab will land.
                RoundedRectangle(cornerRadius: EssentialTile.radius)
                    .strokeBorder(.orange.opacity(0.8), style: StrokeStyle(lineWidth: 1.5, dash: [4, 3]))
                    .background(RoundedRectangle(cornerRadius: EssentialTile.radius).fill(.orange.opacity(0.1)))
                    .frame(width: tileWidth, height: tileHeight)
                    .position(center(ofSlot: gapIndex))
                    .transition(.scale(scale: 0.6).combined(with: .opacity))
            }
            if showsAddTile {
                AddTile { addActiveTab() }
                    .frame(width: tileWidth, height: tileHeight)
                    .position(center(ofSlot: order.count))
                    .transition(.scale(scale: 0.6).combined(with: .opacity))
            }
        }
        .frame(maxWidth: .infinity, minHeight: gridHeight, maxHeight: gridHeight, alignment: .topLeading)
        .coordinateSpace(.named(Self.space))
        .animation(.spring(response: 0.32, dampingFraction: 0.72), value: order)
        .animation(.spring(response: 0.32, dampingFraction: 0.8), value: columns)
        .animation(.spring(response: 0.32, dampingFraction: 0.72), value: gapIndex)
        .animation(.spring(response: 0.32, dampingFraction: 0.72), value: showsAddTile)
    }

    private var emptyState: some View {
        RoundedRectangle(cornerRadius: EssentialTile.radius)
            .strokeBorder(style: StrokeStyle(lineWidth: 1.5, dash: [5, 4]))
            .foregroundStyle(Color.secondary.opacity(0.5))
            .frame(height: tileHeight)
            .overlay {
                if model.activeListedTab != nil {
                    Button("Add this tab to Essentials", systemImage: "plus") { addActiveTab() }
                        .buttonStyle(.plain)
                        .font(.caption.weight(.medium))
                        .foregroundStyle(.secondary)
                } else {
                    Text("Drag a tab here to keep it close").font(.caption).foregroundStyle(.secondary)
                }
            }
    }

    // MARK: Dragging tiles

    private func dragGesture(for essential: Essential, slot: Int) -> some Gesture {
        DragGesture(minimumDistance: 4, coordinateSpace: .named(Self.space))
            .onChanged { value in
                if draggingID == nil {
                    liveOrder = model.essentials.map(\.id)
                    let start = center(ofSlot: slot)
                    grabOffset = CGSize(width: value.startLocation.x - start.x,
                                        height: value.startLocation.y - start.y)
                    draggingID = essential.id
                    Haptics.tick()
                }
                dragCenter = CGPoint(x: value.location.x - grabOffset.width,
                                     y: value.location.y - grabOffset.height)

                // Pulled clearly below the grid: it'll become a tab on release.
                let releasing = value.location.y > gridHeight + 22
                if releasing != releaseToTab {
                    releaseToTab = releasing
                    Haptics.bump()
                }
                guard !releasing else { return }

                let target = self.slot(at: value.location, maxIndex: liveOrder.count - 1)
                if let current = liveOrder.firstIndex(of: essential.id), current != target {
                    liveOrder.remove(at: current)
                    liveOrder.insert(essential.id, at: target)
                    Haptics.snap()
                }
            }
            .onEnded { _ in
                let toTab = releaseToTab
                let finalOrder = liveOrder
                withAnimation(.spring(response: 0.34, dampingFraction: 0.7)) {
                    if toTab {
                        model.moveEssentialToTabs(essential)
                    } else {
                        model.setEssentialOrder(finalOrder)
                    }
                    draggingID = nil
                    releaseToTab = false
                }
                Haptics.bump()
            }
    }

    private func addActiveTab() {
        guard let tab = model.activeListedTab else { return }
        withAnimation(.spring(response: 0.34, dampingFraction: 0.7)) {
            model.makeEssential(fromTab: tab.id)
        }
        Haptics.bump()
    }

    // MARK: Drops from the tab list / bookmarks

    fileprivate func hoverIncoming(at location: CGPoint) {
        let index = slot(at: location, maxIndex: model.essentials.count)
        if index != gapIndex {
            gapIndex = index
            Haptics.snap()
        }
    }

    fileprivate func endIncoming() {
        gapIndex = nil
    }

    fileprivate func acceptIncoming(_ payload: String, at index: Int) {
        withAnimation(.spring(response: 0.34, dampingFraction: 0.7)) {
            switch DragItem(payload) {
            case .tab(let id)?:
                model.makeEssential(fromTab: id, at: index)
            case .bookmark(let id)?:
                model.makeEssential(fromBookmark: id, at: index)
            default:
                break
            }
            gapIndex = nil
        }
        Haptics.bump()
    }
}

/// Opens a gap in the grid under the pointer while a tab is dragged over it.
private struct IncomingDrop: DropDelegate {
    let grid: EssentialsGrid

    func validateDrop(info: DropInfo) -> Bool {
        info.hasItemsConforming(to: [.utf8PlainText, .plainText])
    }

    func dropUpdated(info: DropInfo) -> DropProposal? {
        grid.hoverIncoming(at: info.location)
        return DropProposal(operation: .move)
    }

    func dropExited(info: DropInfo) {
        grid.endIncoming()
    }

    func performDrop(info: DropInfo) -> Bool {
        let index = grid.slotIndexForDrop(at: info.location)
        guard let provider = info.itemProviders(for: [.utf8PlainText, .plainText]).first else {
            grid.endIncoming()
            return false
        }
        _ = provider.loadObject(ofClass: NSString.self) { object, _ in
            let payload = (object as? NSString).map(String.init) ?? ""
            DispatchQueue.main.async { grid.acceptIncoming(payload, at: index) }
        }
        return true
    }
}

extension EssentialsGrid {
    fileprivate func slotIndexForDrop(at location: CGPoint) -> Int {
        gapIndex ?? slot(at: location, maxIndex: model.essentials.count)
    }
}

// MARK: Tiles

private struct EssentialTile: View {
    @Environment(BrowserModel.self) private var model
    let essential: Essential
    let lifted: Bool
    let releasing: Bool
    let onRename: () -> Void
    @State private var hovering = false

    static let radius: CGFloat = 11

    var body: some View {
        let tab = model.essentialTab(for: essential)
        let active = tab != nil && model.activeTab === tab
        ZStack {
            // Active: the site's own colours, blurred into a glow inside the glass.
            if active {
                FaviconGlow(url: essential.url)
                    .transition(.opacity)
            }
            Favicon(url: essential.url, size: 18)
                .saturation(tab == nil ? 0.75 : 1)
                .opacity(tab == nil ? 0.8 : 1)
                .shadow(color: .black.opacity(active ? 0.25 : 0), radius: 3, y: 1)
            // A small dot marks essentials whose tab is open in the background.
            if tab != nil && !active {
                Circle().fill(.primary.opacity(0.45)).frame(width: 3, height: 3)
                    .offset(y: 14)
            }
        }
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .clipShape(RoundedRectangle(cornerRadius: EssentialTile.radius))
        .contentShape(RoundedRectangle(cornerRadius: EssentialTile.radius))
        .glassEffect(.regular.interactive(), in: .rect(cornerRadius: EssentialTile.radius))
        .overlay {
            if active {
                RoundedRectangle(cornerRadius: EssentialTile.radius)
                    .strokeBorder(.white.opacity(0.22), lineWidth: 1)
            }
        }
        .animation(.easeOut(duration: 0.25), value: active)
        .saturation(releasing ? 0.2 : 1)
        .scaleEffect(lifted ? (releasing ? 0.88 : 1.1) : (hovering ? 1.04 : 1))
        .rotationEffect(.degrees(lifted && !releasing ? -3 : 0))
        .shadow(color: .black.opacity(lifted ? 0.28 : 0), radius: lifted ? 12 : 0, y: lifted ? 8 : 0)
        .animation(.spring(response: 0.28, dampingFraction: 0.6), value: hovering)
        .animation(.spring(response: 0.28, dampingFraction: 0.6), value: lifted)
        .animation(.spring(response: 0.28, dampingFraction: 0.6), value: releasing)
        .onHover { hovering = $0 }
        .onTapGesture { model.openEssential(essential) }
        .help(essential.title)
        .contextMenu {
            Button("Open") { model.openEssential(essential) }
            Button("Rename…", action: onRename)
            Button("Set to Current Page") { model.setEssentialToCurrentPage(essential.id) }
                .disabled(model.activeTab?.url == nil)
            Button("Copy Link") {
                NSPasteboard.general.clearContents()
                NSPasteboard.general.setString(essential.url.absoluteString, forType: .string)
            }
            Divider()
            Button("Remove from Essentials", role: .destructive) {
                withAnimation(.snappy) { model.removeEssential(essential) }
            }
        }
    }
}

private struct AddTile: View {
    let action: () -> Void
    @State private var hovering = false

    var body: some View {
        Image(systemName: "plus")
            .font(.system(size: 14, weight: .semibold))
            .foregroundStyle(.secondary)
            .frame(maxWidth: .infinity, maxHeight: .infinity)
            .background {
                RoundedRectangle(cornerRadius: EssentialTile.radius)
                    .strokeBorder(.secondary.opacity(hovering ? 0.6 : 0.3),
                                  style: StrokeStyle(lineWidth: 1.5, dash: [4, 3]))
            }
            .contentShape(RoundedRectangle(cornerRadius: EssentialTile.radius))
            .scaleEffect(hovering ? 1.04 : 1)
            .animation(.spring(response: 0.28, dampingFraction: 0.6), value: hovering)
            .onHover { hovering = $0 }
            .onTapGesture(perform: action)
            .help("Add this tab to Essentials")
    }
}

private struct RenameEssential: View {
    let essential: Essential
    let commit: (String) -> Void
    @State private var title = ""
    @FocusState private var focused: Bool

    var body: some View {
        HStack(spacing: 8) {
            Favicon(url: essential.url, size: 18)
            TextField("Name", text: $title)
                .textFieldStyle(.roundedBorder)
                .frame(width: 180)
                .focused($focused)
                .onSubmit {
                    let trimmed = title.trimmingCharacters(in: .whitespaces)
                    commit(trimmed.isEmpty ? essential.title : trimmed)
                }
        }
        .padding(12)
        .onAppear {
            title = essential.title
            focused = true
        }
    }
}

/// The favicon scaled far past the tile and blurred until only its colours
/// remain: a soft, site-tinted wash for the active essential.
private struct FaviconGlow: View {
    let url: URL

    var body: some View {
        GeometryReader { geo in
            AsyncImage(url: url.faviconURL) { phase in
                if let image = phase.image {
                    image
                        .resizable()
                        .aspectRatio(contentMode: .fill)
                        .frame(width: geo.size.width * 1.4, height: geo.size.width * 1.4)
                        .position(x: geo.size.width / 2, y: geo.size.height / 2)
                        .blur(radius: 22)
                        .saturation(1.8)
                        .opacity(0.85)
                }
            }
        }
        .allowsHitTesting(false)
    }
}
