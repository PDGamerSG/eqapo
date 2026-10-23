import Foundation

/// File locations shared with the daemon (`~/Library/Application Support/eqapo`).
public enum EqapoPaths {
    public static var root: URL {
        if let override = ProcessInfo.processInfo.environment["EQAPO_HOME"], !override.isEmpty {
            return URL(fileURLWithPath: override)
        }
        return FileManager.default.homeDirectoryForCurrentUser
            .appendingPathComponent("Library/Application Support/eqapo")
    }
    public static var configDir: URL { root.appendingPathComponent("config") }
    public static var configFile: URL { configDir.appendingPathComponent("config.txt") }
    public static var eqPresetsDir: URL { configDir.appendingPathComponent("presets/eq") }
    public static var delayPresetsDir: URL { configDir.appendingPathComponent("presets/delay") }
    public static var statusFile: URL { root.appendingPathComponent("status.json") }
    public static var pidFile: URL { root.appendingPathComponent("eqapo.pid") }
}

public enum PresetStore {
    public static func names(in dir: URL) -> [String] {
        let items = (try? FileManager.default.contentsOfDirectory(atPath: dir.path)) ?? []
        return items.filter { $0.hasSuffix(".txt") }.map { String($0.dropLast(4)) }
            .sorted { $0.localizedCaseInsensitiveCompare($1) == .orderedAscending }
    }
    public static func eqPresetNames() -> [String] { names(in: EqapoPaths.eqPresetsDir) }
    public static func effectNames() -> [String] { names(in: EqapoPaths.delayPresetsDir) }

    public static func loadEQPreset(_ name: String) -> EQSettings? {
        let url = EqapoPaths.eqPresetsDir.appendingPathComponent(name + ".txt")
        guard let text = try? String(contentsOf: url, encoding: .utf8) else { return nil }
        var settings = ConfigWriter.parse(text)
        settings?.name = name
        return settings
    }

    public static func saveEQPreset(_ settings: EQSettings) throws {
        try FileManager.default.createDirectory(at: EqapoPaths.eqPresetsDir, withIntermediateDirectories: true)
        var copy = settings
        copy.effect = nil
        let url = EqapoPaths.eqPresetsDir.appendingPathComponent(settings.name + ".txt")
        try ConfigWriter.render(copy).write(to: url, atomically: true, encoding: .utf8)
    }

    /// Writes config.txt atomically; the daemon picks it up through FSEvents.
    public static func writeConfig(_ text: String) throws {
        try FileManager.default.createDirectory(at: EqapoPaths.configDir, withIntermediateDirectories: true)
        try text.write(to: EqapoPaths.configFile, atomically: true, encoding: .utf8)
    }

    public static func readConfig() -> String? {
        try? String(contentsOf: EqapoPaths.configFile, encoding: .utf8)
    }
}

/// Subset of the daemon's status.json.
public struct DaemonStatus {
    public var pid: Int = 0
    public var state: String = "stopped"
    public var error: String = ""
    public var deviceName: String = ""
    public var sampleRate: Double = 0
    public var outputChannels: Int = 0
    public var bufferFrames: Int = 0
    public var filterCount: Int = 0
    public var avgLoadPercent: Double = 0
    public var configErrors: [String] = []
    public var alive: Bool = false

    public init() {}

    public static func read() -> DaemonStatus {
        var s = DaemonStatus()
        guard let data = try? Data(contentsOf: EqapoPaths.statusFile),
              let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any] else { return s }
        s.pid = json["pid"] as? Int ?? 0
        s.state = json["state"] as? String ?? "stopped"
        s.error = json["error"] as? String ?? ""
        s.deviceName = (json["device"] as? [String: Any])?["name"] as? String ?? ""
        s.sampleRate = json["sampleRate"] as? Double ?? 0
        s.outputChannels = json["outputChannels"] as? Int ?? 0
        s.bufferFrames = json["bufferFrames"] as? Int ?? 0
        s.filterCount = (json["filters"] as? [Any])?.count ?? 0
        s.avgLoadPercent = json["avgLoadPercent"] as? Double ?? 0
        s.configErrors = json["configErrors"] as? [String] ?? []
        s.alive = s.pid > 0 && kill(pid_t(s.pid), 0) == 0
        if !s.alive { s.state = "stopped" }
        return s
    }
}
