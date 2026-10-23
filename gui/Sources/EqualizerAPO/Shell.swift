import Foundation

/// Locates the `eqapo` command line tool and runs its sub-commands.
enum EqapoTool {
    static var path: String? {
        if let env = ProcessInfo.processInfo.environment["EQAPO_BIN"], FileManager.default.isExecutableFile(atPath: env) {
            return env
        }
        let bundled = Bundle.main.bundleURL.appendingPathComponent("Contents/MacOS/eqapo").path
        let candidates = [bundled, "/usr/local/bin/eqapo", "/opt/homebrew/bin/eqapo",
                          NSHomeDirectory() + "/bin/eqapo", NSHomeDirectory() + "/.local/bin/eqapo"]
        return candidates.first { FileManager.default.isExecutableFile(atPath: $0) }
    }

    /// Runs `eqapo <args>` and returns (exit code, combined output).
    @discardableResult
    static func run(_ args: [String]) -> (Int32, String) {
        guard let path = path else { return (127, "eqapo binary not found (set EQAPO_BIN or install it)") }
        let process = Process()
        process.executableURL = URL(fileURLWithPath: path)
        process.arguments = args
        let pipe = Pipe()
        process.standardOutput = pipe
        process.standardError = pipe
        do {
            try process.run()
        } catch {
            return (126, "\(error)")
        }
        let data = pipe.fileHandleForReading.readDataToEndOfFile()
        process.waitUntilExit()
        return (process.terminationStatus, String(decoding: data, as: UTF8.self))
    }

    static var isInstalled: Bool {
        let plist = NSHomeDirectory() + "/Library/LaunchAgents/com.eqapo.daemon.plist"
        return FileManager.default.fileExists(atPath: plist)
    }
}
