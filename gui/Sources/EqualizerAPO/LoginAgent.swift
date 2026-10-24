import Foundation
import Darwin

/// Owns the app's optional LaunchAgent. The daemon has its own agent; this one
/// only keeps the menu-bar UI available after the user logs in.
enum EqapoLoginAgent {
    static let label = "com.eqapo.gui"

    private static var plistURL: URL {
        FileManager.default.homeDirectoryForCurrentUser
            .appendingPathComponent("Library/LaunchAgents", isDirectory: true)
            .appendingPathComponent(label + ".plist")
    }

    private static var target: String { "gui/\(getuid())/\(label)" }

    static var isConfigured: Bool {
        FileManager.default.fileExists(atPath: plistURL.path)
    }

    static var isEnabled: Bool {
        guard isConfigured else { return false }
        return runLaunchctl(["print", target]).status == 0
    }

    static func setEnabled(_ enabled: Bool) throws {
        if enabled {
            try install()
        } else {
            try uninstall()
        }
    }

    /// Writes the login item without loading it into the current session. This
    /// is used when a user opens the app directly from a DMG, avoiding a second
    /// process while still enabling startup on the next login.
    static func ensureConfigured() throws {
        try writePlist()
    }

    private static func install() throws {
        try writePlist()

        // Boot out an older copy before bootstrapping the new plist. A short
        // retry handles the race while launchd releases a previous job.
        _ = runLaunchctl(["bootout", target])
        var lastError = "could not load the eqapo startup agent"
        for _ in 0 ..< 3 {
            let result = runLaunchctl(["bootstrap", "gui/\(getuid())", plistURL.path])
            if result.status == 0 { return }
            lastError = result.output
            usleep(100_000)
        }
        throw NSError(domain: "eqapo.login", code: 2,
                      userInfo: [NSLocalizedDescriptionKey: lastError.trimmingCharacters(in: .whitespacesAndNewlines)])
    }

    private static func writePlist() throws {
        guard let executable = Bundle.main.executableURL?.path,
              Bundle.main.bundleURL.pathExtension == "app" else {
            throw NSError(domain: "eqapo.login", code: 1,
                          userInfo: [NSLocalizedDescriptionKey: "eqapo must be installed in Applications before enabling startup."])
        }

        let directory = plistURL.deletingLastPathComponent()
        try FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        let plist: [String: Any] = [
            "Label": label,
            "ProgramArguments": [executable],
            "RunAtLoad": true,
            "KeepAlive": false,
            "ProcessType": "Interactive",
            "LimitLoadToSessionType": "Aqua",
        ]
        let data = try PropertyListSerialization.data(fromPropertyList: plist, format: .xml, options: 0)
        try data.write(to: plistURL, options: .atomic)
    }

    private static func uninstall() throws {
        _ = runLaunchctl(["bootout", target])
        if FileManager.default.fileExists(atPath: plistURL.path) {
            try FileManager.default.removeItem(at: plistURL)
        }
    }

    private static func runLaunchctl(_ arguments: [String]) -> (status: Int32, output: String) {
        let process = Process()
        process.executableURL = URL(fileURLWithPath: "/bin/launchctl")
        process.arguments = arguments
        let pipe = Pipe()
        process.standardOutput = pipe
        process.standardError = pipe
        do {
            try process.run()
            process.waitUntilExit()
            let data = pipe.fileHandleForReading.readDataToEndOfFile()
            return (process.terminationStatus, String(decoding: data, as: UTF8.self))
        } catch {
            return (127, error.localizedDescription)
        }
    }
}
