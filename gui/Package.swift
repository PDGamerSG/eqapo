// swift-tools-version:5.9
import PackageDescription

let package = Package(
    name: "EqualizerAPO",
    platforms: [.macOS(.v14)],
    targets: [
        // Pure logic shared by the app and the check tool: PC Equalizer config format, presets, paths.
        .target(name: "EqualizerAPOCore", path: "Sources/EqualizerAPOCore"),
        // Menu bar application.
        .executableTarget(
            name: "EqualizerAPO",
            dependencies: ["EqualizerAPOCore"],
            path: "Sources/EqualizerAPO",
            resources: [.process("Assets")],
            linkerSettings: [.linkedFramework("SwiftUI"), .linkedFramework("AppKit")]),
        // Self-check run by make_app.sh (no XCTest in the Command Line Tools).
        .executableTarget(
            name: "ConfigWriterCheck",
            dependencies: ["EqualizerAPOCore"],
            path: "Sources/ConfigWriterCheck"),
    ]
)
