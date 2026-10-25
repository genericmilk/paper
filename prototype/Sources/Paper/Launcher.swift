import SwiftUI

struct LauncherMatch: Identifiable, Equatable {
    enum Kind: String { case tab = "Open tabs", url = "Go to", history = "From your history", search = "Search" }

    let id = UUID()
    let kind: Kind
    let title: String
    let subtitle: String
    let url: URL
    var tabID: UUID?
}

/// Cmd+T / "+" / domain pill: a springy glass card in the middle of the
/// window. Open tabs first, then what you typed, history, then search.
struct Launcher: View {
    @Environment(BrowserModel.self) private var model
    let mode: LauncherMode
    /// The web page's frame in the window; the card centres on it.
    let pageFrame: CGRect

    @State private var text = ""
    @State private var selected = 0
    @State private var suggestions: [String] = []
    @State private var suggestTask: Task<Void, Never>?
    @State private var appeared = false
    @State private var squish = false
    @FocusState private var focused: Bool
    @Namespace private var selection

    private static let prompts = ["Where to?", "Search or type a URL", "Find a tab, a site, anything",
                                  "Go somewhere nice", "What are we looking for?"]
    @State private var prompt = prompts.randomElement()!

    var body: some View {
        // The input row sits just above the page's centre and results grow
        // downward from it, so the card doesn't jump as matches arrive.
        card
            .frame(width: cardWidth)
            .scaleEffect(squish ? 0.97 : (appeared ? 1 : 0.94), anchor: .top)
            .offset(x: pageFrame.midX - cardWidth / 2,
                    y: cardTop + (appeared ? 0 : -14))
            .opacity(appeared ? 1 : 0)
            .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .topLeading)
            .background {
                // Dim + click-away. A background, so it can't shift layout.
                // No dim when it's the empty window: there's nothing behind it.
                Color.black.opacity(appeared && !model.launcherIsPinned ? 0.12 : 0)
                    .ignoresSafeArea()
                    .onTapGesture { model.hideLauncher() }
            }
        .onAppear {
            text = model.launcherInitialText
            focused = true
            withAnimation(.spring(response: 0.42, dampingFraction: 0.68)) { appeared = true }
        }
        .onExitCommand { model.hideLauncher() }
    }

    private var cardWidth: CGFloat {
        min(640, max(360, pageFrame.width - 48))
    }

    private var cardTop: CGFloat {
        pageFrame.minY + max(24, pageFrame.height * 0.5 - 150)
    }

    private var card: some View {
        let matches = self.matches
        return VStack(spacing: 0) {
            HStack(spacing: 14) {
                Spark()
                TextField(prompt, text: $text)
                    .textFieldStyle(.plain)
                    .font(.system(size: 22, weight: .medium))
                    .focused($focused)
                    .onSubmit { open(matches, selected) }
                    .onKeyPress(.downArrow) { move(1, count: matches.count); return .handled }
                    .onKeyPress(.upArrow) { move(-1, count: matches.count); return .handled }
                    .onChange(of: text) { _, new in
                        selected = 0
                        fetchSuggestions(for: new)
                    }
                if mode == .editCurrent {
                    Text("THIS TAB")
                        .font(.system(size: 10, weight: .bold))
                        .tracking(0.6)
                        .foregroundStyle(.secondary)
                        .padding(.horizontal, 8).padding(.vertical, 4)
                        .background(Capsule().fill(.primary.opacity(0.07)))
                }
            }
            .padding(.horizontal, 22)
            .frame(height: 64)

            if !matches.isEmpty {
                Divider().opacity(0.5)
                ScrollView {
                    VStack(alignment: .leading, spacing: 2) {
                        ForEach(Array(matches.enumerated()), id: \.element.id) { index, match in
                            if index == 0 || matches[index - 1].kind != match.kind {
                                Text(match.kind.rawValue.uppercased())
                                    .font(.system(size: 10.5, weight: .semibold))
                                    .tracking(0.7)
                                    .foregroundStyle(.tertiary)
                                    .padding(.horizontal, 12)
                                    .padding(.top, 10).padding(.bottom, 2)
                            }
                            ResultRow(match: match, selected: index == selected, namespace: selection)
                                .onHover { if $0 { selected = index } }
                                .onTapGesture { open(matches, index) }
                        }
                    }
                    .padding(10)
                    .animation(.spring(response: 0.25, dampingFraction: 0.8), value: selected)
                }
                .frame(maxHeight: 420)
                .fixedSize(horizontal: false, vertical: true)
            }
        }
        .glassEffect(.regular, in: .rect(cornerRadius: 24))
        .shadow(color: .black.opacity(0.22), radius: 30, y: 18)
    }

    // MARK: Matching

    private var matches: [LauncherMatch] {
        let query = text.trimmingCharacters(in: .whitespaces)
        let q = query.lowercased()
        var result: [LauncherMatch] = []

        let openTabs = model.tabs + model.essentialTabs.values
        let tabMatches = openTabs
            .filter { $0 !== model.activeTab }
            .filter { q.isEmpty || $0.displayTitle.lowercased().contains(q)
                || ($0.url?.absoluteString.lowercased().contains(q) ?? false) }
            .prefix(q.isEmpty ? 6 : 3)
        for tab in tabMatches {
            guard let url = tab.url else { continue }
            result.append(LauncherMatch(kind: .tab, title: tab.displayTitle,
                                        subtitle: url.paperDomain, url: url, tabID: tab.id))
        }
        guard !q.isEmpty else { return result }

        if let url = Self.urlIfTyped(query) {
            result.append(LauncherMatch(kind: .url, title: url.absoluteString, subtitle: "", url: url))
        }
        for entry in model.history.search(q, limit: 5) where !result.contains(where: { $0.url == entry.url }) {
            result.append(LauncherMatch(kind: .history, title: entry.title.isEmpty ? entry.url.absoluteString : entry.title,
                                        subtitle: entry.url.paperDomain, url: entry.url))
        }
        for term in ([query] + suggestions.filter { $0.lowercased() != q }).prefix(5) {
            result.append(LauncherMatch(kind: .search, title: term, subtitle: "Google", url: Self.searchURL(term)))
        }
        // A typed URL beats everything; otherwise tabs, then history, then search.
        return result.sorted { rank($0.kind) < rank($1.kind) }
    }

    private func rank(_ kind: LauncherMatch.Kind) -> Int {
        switch kind {
        case .url: 0
        case .tab: 1
        case .history: 2
        case .search: 3
        }
    }

    static func urlIfTyped(_ text: String) -> URL? {
        let t = text.trimmingCharacters(in: .whitespaces)
        guard !t.contains(" "), !t.isEmpty else { return nil }
        if let url = URL(string: t), let scheme = url.scheme, ["http", "https", "file", "about"].contains(scheme) {
            return url
        }
        let looksLikeHost = t.contains(".") || t.hasPrefix("localhost")
        guard looksLikeHost, let url = URL(string: "https://\(t)"), url.host() != nil else { return nil }
        return url
    }

    static func searchURL(_ term: String) -> URL {
        var components = URLComponents(string: "https://www.google.com/search")!
        components.queryItems = [URLQueryItem(name: "q", value: term)]
        return components.url!
    }

    private func fetchSuggestions(for query: String) {
        suggestTask?.cancel()
        let q = query.trimmingCharacters(in: .whitespaces)
        guard q.count >= 2, Self.urlIfTyped(q) == nil else { suggestions = []; return }
        suggestTask = Task {
            try? await Task.sleep(for: .milliseconds(120))
            guard !Task.isCancelled else { return }
            var components = URLComponents(string: "https://suggestqueries.google.com/complete/search")!
            components.queryItems = [URLQueryItem(name: "client", value: "firefox"), URLQueryItem(name: "q", value: q)]
            guard let (data, _) = try? await URLSession.shared.data(from: components.url!),
                  let json = try? JSONSerialization.jsonObject(with: data) as? [Any],
                  let terms = json.dropFirst().first as? [String], !Task.isCancelled else { return }
            suggestions = Array(terms.prefix(4))
        }
    }

    // MARK: Actions

    private func move(_ delta: Int, count: Int) {
        guard count > 0 else { return }
        selected = (selected + delta + count) % count
    }

    private func open(_ matches: [LauncherMatch], _ index: Int) {
        let match: LauncherMatch
        if matches.indices.contains(index) {
            match = matches[index]
        } else if let url = Self.urlIfTyped(text) {
            match = LauncherMatch(kind: .url, title: "", subtitle: "", url: url)
        } else if !text.trimmingCharacters(in: .whitespaces).isEmpty {
            match = LauncherMatch(kind: .search, title: text, subtitle: "", url: Self.searchURL(text))
        } else {
            return
        }
        // A quick squish, then go.
        withAnimation(.spring(response: 0.12, dampingFraction: 0.6)) { squish = true }
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.09) {
            if let tabID = match.tabID,
               let tab = (model.tabs + model.essentialTabs.values).first(where: { $0.id == tabID }) {
                model.activate(tab)
                model.hideLauncher()
            } else {
                model.open(match.url, mode: mode)
            }
        }
    }
}

private struct ResultRow: View {
    let match: LauncherMatch
    let selected: Bool
    let namespace: Namespace.ID

    var body: some View {
        HStack(spacing: 12) {
            icon
                .frame(width: 30, height: 30)
                .background {
                    RoundedRectangle(cornerRadius: 9).fill(tileFill)
                }
                .scaleEffect(selected ? 1.1 : 1)
                .rotationEffect(.degrees(selected ? -4 : 0))
            VStack(alignment: .leading, spacing: 1) {
                Text(match.title).font(.system(size: 14, weight: .medium)).lineLimit(1)
                if !match.subtitle.isEmpty {
                    Text(match.subtitle).font(.system(size: 12)).foregroundStyle(.secondary).lineLimit(1)
                }
            }
            Spacer(minLength: 0)
            if selected {
                Text(match.kind == .tab ? "Switch" : "↩")
                    .font(.system(size: 12, weight: .semibold))
                    .foregroundStyle(match.kind == .tab ? Color.orange : Color.secondary)
                    .padding(.horizontal, match.kind == .tab ? 8 : 0).padding(.vertical, 3)
                    .background(Capsule().fill(match.kind == .tab ? Color.orange.opacity(0.14) : .clear))
                    .transition(.move(edge: .leading).combined(with: .opacity))
            }
        }
        .padding(.horizontal, 12)
        .frame(height: 46)
        .background {
            if selected {
                // One highlight that glides between rows.
                RoundedRectangle(cornerRadius: 12)
                    .fill(LinearGradient(colors: [.orange.opacity(0.22), .purple.opacity(0.14)],
                                         startPoint: .leading, endPoint: .trailing))
                    .matchedGeometryEffect(id: "selection", in: namespace)
            }
        }
        .contentShape(Rectangle())
    }

    @ViewBuilder private var icon: some View {
        if match.kind == .search {
            Image(systemName: "magnifyingglass")
                .font(.system(size: 14, weight: .bold))
                .foregroundStyle(.white)
        } else {
            Favicon(url: match.url, size: 18)
        }
    }

    private var tileFill: AnyShapeStyle {
        if match.kind == .search {
            return AnyShapeStyle(LinearGradient(colors: [.purple, .orange], startPoint: .topLeading, endPoint: .bottomTrailing))
        }
        let hue = Double(abs(match.url.paperDomain.hashValue) % 360) / 360
        return AnyShapeStyle(Color(hue: hue, saturation: 0.7, brightness: 0.95).opacity(0.22))
    }
}

/// A soft orb that breathes: a warm gradient slowly drifting between
/// orange, pink and violet while it gently swells and its glow rises and
/// falls. Calm enough to sit in the corner of your eye.
private struct Spark: View {
    var body: some View {
        TimelineView(.animation) { context in
            let t = context.date.timeIntervalSinceReferenceDate
            // Two slow, out-of-step waves so it never quite repeats.
            let breathe = (sin(t * 2 * .pi / 3.6) + 1) / 2      // 0…1 every 3.6s
            let drift = (sin(t * 2 * .pi / 7.0 + 1.3) + 1) / 2  // 0…1 every 7s
            Circle()
                .fill(
                    RadialGradient(
                        colors: [
                            Color(hue: 0.07 + 0.03 * drift, saturation: 0.75, brightness: 1),
                            Color(hue: 0.95 - 0.04 * drift, saturation: 0.65, brightness: 0.95),
                            Color(hue: 0.78 + 0.03 * breathe, saturation: 0.55, brightness: 0.9),
                        ],
                        center: UnitPoint(x: 0.35 + 0.3 * drift, y: 0.3 + 0.25 * breathe),
                        startRadius: 0, endRadius: 13)
                )
                .frame(width: 16, height: 16)
                .scaleEffect(0.92 + 0.08 * breathe)
                .shadow(color: Color(hue: 0.0 + 0.05 * drift, saturation: 0.7, brightness: 1)
                            .opacity(0.25 + 0.25 * breathe), radius: 4 + 4 * breathe)
        }
        .frame(width: 18, height: 18)
    }
}
