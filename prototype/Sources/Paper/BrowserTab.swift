import Observation
import WebKit

/// One tab: a WKWebView plus the bits of its state the chrome shows.
@MainActor @Observable
final class BrowserTab: Identifiable {
    let id = UUID()
    let webView: WKWebView

    var title = ""
    var url: URL?
    var isLoading = false
    var progress = 0.0
    var canGoBack = false
    var canGoForward = false

    @ObservationIgnored private var observations: [NSKeyValueObservation] = []
    @ObservationIgnored private var delegate: TabDelegate?

    init(url: URL?, configuration: WKWebViewConfiguration? = nil, model: BrowserModel) {
        let webView = WKWebView(frame: .zero, configuration: configuration ?? BrowserTab.sharedConfiguration)
        // Paper does its own swipe navigation (SwipeNavigator) with feedback.
        webView.allowsBackForwardNavigationGestures = false
        webView.allowsMagnification = true
        webView.isInspectable = true
        self.webView = webView
        self.url = url

        let delegate = TabDelegate(tab: self, model: model)
        self.delegate = delegate
        webView.navigationDelegate = delegate
        webView.uiDelegate = delegate

        observe(\.title) { tab, web in tab.title = web.title ?? "" }
        observe(\.url) { tab, web in if let url = web.url { tab.url = url } }
        observe(\.isLoading) { tab, web in tab.isLoading = web.isLoading }
        observe(\.estimatedProgress) { tab, web in tab.progress = web.estimatedProgress }
        observe(\.canGoBack) { tab, web in tab.canGoBack = web.canGoBack }
        observe(\.canGoForward) { tab, web in tab.canGoForward = web.canGoForward }

        if let url {
            webView.load(URLRequest(url: url))
        }
    }

    var displayTitle: String {
        title.isEmpty ? (url?.paperDomain ?? "New Tab") : title
    }

    func load(_ url: URL) {
        self.url = url
        webView.load(URLRequest(url: url))
    }

    // MARK: Picture in picture

    /// True while a video from this tab is in PiP because Paper put it there,
    /// so coming back can return it inline (but a PiP you started yourself
    /// is left alone).
    @ObservationIgnored private(set) var inAutoPictureInPicture = false

    /// If a video is playing, float it into picture in picture.
    func autoPictureInPicture() {
        webView.requestMediaPlaybackState { [weak self] state in
            MainActor.assumeIsolated {
                guard let self, state == .playing else { return }
                self.webView.evaluateJavaScript(Self.enterPiPScript) { result, _ in
                    MainActor.assumeIsolated {
                        let outcome = result as? String ?? "error"
                        self.inAutoPictureInPicture = outcome == "entered"
                    }
                }
            }
        }
    }

    /// Undo autoPictureInPicture() when you come back to the tab.
    func endAutoPictureInPicture() {
        guard inAutoPictureInPicture else { return }
        inAutoPictureInPicture = false
        webView.evaluateJavaScript(Self.exitPiPScript)
    }

    /// Picks the biggest playing video (ads and previews are usually smaller)
    /// and hands it to WebKit's PiP. Respects pages that opt out.
    private static let enterPiPScript = """
    (() => {
      if (document.pictureInPictureElement) return 'already';
      // Skip hover previews (YouTube thumbnails etc.): small and muted.
      const isPreview = v => v.muted && v.getBoundingClientRect().width < 480;
      const playing = [...document.querySelectorAll('video')]
        .filter(v => !v.paused && !v.ended && v.readyState > 2 && v.videoWidth > 0
                     && !v.disablePictureInPicture && !isPreview(v));
      const video = playing.sort((a, b) =>
        b.videoWidth * b.videoHeight - a.videoWidth * a.videoHeight)[0];
      if (!video) return 'none';
      if (video.webkitSupportsPresentationMode?.('picture-in-picture')) {
        video.webkitSetPresentationMode('picture-in-picture');
        return 'entered';
      }
      if (video.requestPictureInPicture) {
        video.requestPictureInPicture().catch(() => {});
        return 'entered';
      }
      return 'unsupported';
    })()
    """

    /// The ⋯ menu's Picture in Picture: toggles the page's main video. Not
    /// marked as automatic, so coming back to the tab leaves it floating.
    func togglePictureInPicture() {
        webView.evaluateJavaScript(Self.togglePiPScript)
    }

    private static let togglePiPScript = """
    (() => {
      const videos = [...document.querySelectorAll('video')];
      const floating = videos.find(v => v.webkitPresentationMode === 'picture-in-picture');
      if (floating) { floating.webkitSetPresentationMode('inline'); return; }
      const video = videos
        .filter(v => v.readyState > 0 && v.videoWidth > 0 && !v.disablePictureInPicture)
        .sort((a, b) => b.videoWidth * b.videoHeight - a.videoWidth * a.videoHeight)[0];
      video?.webkitSetPresentationMode?.('picture-in-picture');
    })()
    """

    private static let exitPiPScript = """
    (() => {
      const video = document.pictureInPictureElement
        ?? [...document.querySelectorAll('video')]
             .find(v => v.webkitPresentationMode === 'picture-in-picture');
      if (!video) return;
      if (video.webkitSetPresentationMode) video.webkitSetPresentationMode('inline');
      else document.exitPictureInPicture?.();
    })()
    """

    private func observe<Value>(_ keyPath: KeyPath<WKWebView, Value>,
                                _ apply: @escaping @MainActor (BrowserTab, WKWebView) -> Void) {
        observations.append(webView.observe(keyPath, options: [.initial, .new]) { [weak self] web, _ in
            MainActor.assumeIsolated {
                guard let self else { return }
                apply(self, web)
            }
        })
    }

    static let sharedConfiguration: WKWebViewConfiguration = {
        let configuration = WKWebViewConfiguration()
        configuration.websiteDataStore = .default()
        // Sites sniff for "Safari"; without it some serve their worst page.
        configuration.applicationNameForUserAgent = "Version/26.0 Safari/605.1.15"
        configuration.preferences.isElementFullscreenEnabled = true
        // macOS WKWebView ships with picture in picture switched off (the
        // public switch is iOS-only). This is the WebKit preference Safari
        // uses; skip it if a future WebKit drops the setter.
        if configuration.preferences.responds(to: NSSelectorFromString("_setAllowsPictureInPictureMediaPlayback:")) {
            configuration.preferences.setValue(true, forKey: "allowsPictureInPictureMediaPlayback")
        }
        return configuration
    }()
}

/// Navigation + UI delegate: records history and turns target=_blank into tabs.
@MainActor
private final class TabDelegate: NSObject, WKNavigationDelegate, WKUIDelegate {
    weak var tab: BrowserTab?
    weak var model: BrowserModel?

    init(tab: BrowserTab, model: BrowserModel) {
        self.tab = tab
        self.model = model
    }

    func webView(_ webView: WKWebView, didFinish navigation: WKNavigation!) {
        guard let url = webView.url else { return }
        model?.history.record(url: url, title: webView.title ?? "")
    }

    func webView(_ webView: WKWebView,
                 createWebViewWith configuration: WKWebViewConfiguration,
                 for navigationAction: WKNavigationAction,
                 windowFeatures: WKWindowFeatures) -> WKWebView? {
        guard let model else { return nil }
        // WebKit requires the returned view to use the configuration it hands us.
        let tab = model.openTab(url: nil, configuration: configuration)
        return tab.webView
    }

    func webViewDidClose(_ webView: WKWebView) {
        guard let tab, let model else { return }
        model.close(tab)
    }
}

extension URL {
    /// The bare registrable-ish domain for the centre pill: "reddit.com" for
    /// https://old.reddit.com/r/foo. Good enough without a public suffix list
    /// for the common cases (keeps the last two labels, or three for co.uk-style).
    var paperDomain: String {
        guard let host = host(percentEncoded: false)?.lowercased(), !host.isEmpty else {
            return scheme == "file" ? "file" : "paper"
        }
        if host.allSatisfy({ $0.isNumber || $0 == "." }) || !host.contains(".") {
            return host
        }
        let labels = host.split(separator: ".")
        let secondLevel: Set<Substring> = ["co", "com", "org", "net", "ac", "gov", "edu"]
        let keep = labels.count >= 3 && secondLevel.contains(labels[labels.count - 2])
            && labels.last!.count == 2 ? 3 : 2
        return labels.suffix(keep).joined(separator: ".")
    }

    var faviconURL: URL? {
        guard let host = host() else { return nil }
        return URL(string: "https://www.google.com/s2/favicons?domain=\(host)&sz=64")
    }
}
