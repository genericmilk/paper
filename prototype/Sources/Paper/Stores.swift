import Foundation
import Observation

struct Essential: Codable, Identifiable, Hashable {
    var id = UUID()
    var title: String
    var url: URL
}

struct BookmarkNode: Codable, Identifiable, Hashable {
    var id = UUID()
    var title: String
    /// nil for folders.
    var url: URL?
    var children: [BookmarkNode] = []

    var isFolder: Bool { url == nil }
}

struct HistoryEntry: Codable, Hashable {
    var url: URL
    var title: String
    var visits: Int
    var lastVisit: Date
}

/// Everything Paper keeps between launches, in one JSON file.
struct PersistedState: Codable {
    var essentials: [Essential]
    var bookmarks: [BookmarkNode]
    var history: [HistoryEntry]
    var sidebarOpen: Bool
    /// Optional so state files from before resizing still decode.
    var sidebarWidth: Double?
    var openTabs: [URL]

    static let seed = PersistedState(
        essentials: [
            Essential(title: "Facebook", url: URL(string: "https://www.facebook.com")!),
            Essential(title: "X", url: URL(string: "https://x.com")!),
            Essential(title: "YouTube", url: URL(string: "https://www.youtube.com")!),
            Essential(title: "Bluesky", url: URL(string: "https://bsky.app")!),
            Essential(title: "Reddit", url: URL(string: "https://www.reddit.com")!),
            Essential(title: "Instagram", url: URL(string: "https://www.instagram.com")!),
            Essential(title: "LinkedIn", url: URL(string: "https://www.linkedin.com")!),
        ],
        bookmarks: [
            BookmarkNode(title: "Pull Requests", children: [
                BookmarkNode(title: "GitHub pulls", url: URL(string: "https://github.com/pulls")!),
                BookmarkNode(title: "Review requests", url: URL(string: "https://github.com/pulls/review-requested")!),
            ]),
        ],
        history: [],
        sidebarOpen: true,
        openTabs: [URL(string: "https://github.com")!]
    )
}

enum Persistence {
    private static var fileURL: URL {
        // PAPER_STATE_DIR points a second copy (demos, screenshots) at its own state.
        let dir = ProcessInfo.processInfo.environment["PAPER_STATE_DIR"].map { URL(fileURLWithPath: $0) }
            ?? FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
                .appendingPathComponent("PaperPrototype", isDirectory: true)
        try? FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
        return dir.appendingPathComponent("state.json")
    }

    static func load() -> PersistedState {
        guard let data = try? Data(contentsOf: fileURL),
              let state = try? JSONDecoder().decode(PersistedState.self, from: data) else {
            return .seed
        }
        return state
    }

    static func save(_ state: PersistedState) {
        guard let data = try? JSONEncoder().encode(state) else { return }
        try? data.write(to: fileURL, options: .atomic)
    }
}

/// Visit history, ranked by frecency for the launcher.
@MainActor @Observable
final class HistoryStore {
    private(set) var entries: [HistoryEntry]
    @ObservationIgnored var onChange: (() -> Void)?

    init(entries: [HistoryEntry]) {
        self.entries = entries
    }

    func record(url: URL, title: String) {
        guard url.scheme == "https" || url.scheme == "http" else { return }
        if let i = entries.firstIndex(where: { $0.url == url }) {
            entries[i].visits += 1
            entries[i].lastVisit = .now
            if !title.isEmpty { entries[i].title = title }
        } else {
            entries.append(HistoryEntry(url: url, title: title, visits: 1, lastVisit: .now))
        }
        if entries.count > 5000 {
            entries.sort { $0.lastVisit > $1.lastVisit }
            entries.removeLast(entries.count - 5000)
        }
        onChange?()
    }

    func search(_ query: String, limit: Int) -> [HistoryEntry] {
        let q = query.lowercased()
        guard !q.isEmpty else { return [] }
        return entries
            .filter { $0.title.lowercased().contains(q) || $0.url.absoluteString.lowercased().contains(q) }
            .sorted { score($0, q) > score($1, q) }
            .prefix(limit)
            .map { $0 }
    }

    private func score(_ entry: HistoryEntry, _ q: String) -> Double {
        let ageDays = max(0, -entry.lastVisit.timeIntervalSinceNow / 86_400)
        let recency = 1 / (1 + ageDays / 7)
        let hostBoost = (entry.url.host()?.lowercased().hasPrefix(q) ?? false) ? 3.0 : 1.0
        return Double(entry.visits) * recency * hostBoost
    }
}
