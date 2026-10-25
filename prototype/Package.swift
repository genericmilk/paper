// swift-tools-version: 6.0
// Paper's WebKit prototype: the same chrome as the Chromium fork, in SwiftUI
// on top of WKWebView, so the UI can be tried today without a Chromium build.

import PackageDescription

let package = Package(
    name: "Paper",
    platforms: [.macOS("26.0")],
    targets: [
        .executableTarget(
            name: "Paper",
            path: "Sources/Paper",
            swiftSettings: [.swiftLanguageMode(.v5)]
        ),
    ]
)
