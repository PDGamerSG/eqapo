import Combine
import EqualizerAPOCore
import Foundation

/// Application state: the 20-band settings, presets, and the daemon status.
/// Every change is written to config.txt (debounced), which the daemon applies at once.
@MainActor
final class EQModel: ObservableObject {
    @Published var settings: EQSettings = .flat {
        didSet { if !suppressWrite { scheduleWrite() } }
    }
    @Published var enabled: Bool = true {
        didSet { if !suppressWrite { scheduleWrite() } }
    }
    @Published var presetNames: [String] = []
    @Published var effectNames: [String] = []
    @Published var status = DaemonStatus()
    @Published var lastMessage: String = ""
    // transient UI state (see ContentView for why it is not @State)
    @Published var showSave: Bool = false
    @Published var saveName: String = ""

    static let bandwidthChoices: [Double] = [0.5, 0.7, 1.0, 1.2, 1.5, 2.0, 2.2, 2.6, 3.0]

    private var suppressWrite = false
    private var writeTimer: Timer?
    private var statusTimer: Timer?

    init() {
        reloadPresetLists()
        loadFromConfig()
        refreshStatus()
        statusTimer = Timer.scheduledTimer(withTimeInterval: 2.0, repeats: true) { [weak self] _ in
            Task { @MainActor in self?.refreshStatus() }
        }
    }

    func reloadPresetLists() {
        presetNames = PresetStore.eqPresetNames()
        effectNames = PresetStore.effectNames()
    }

    /// Reads config.txt; keeps the sliders flat when it is not a 20-band file.
    func loadFromConfig() {
        suppressWrite = true
        defer { suppressWrite = false }
        guard let text = PresetStore.readConfig() else { return }
        if text.hasPrefix("# EqualizerAPO: equalizer switched off") {
            enabled = false
            return
        }
        if let parsed = ConfigWriter.parse(text) {
            settings = parsed
            enabled = true
        } else if let include = text.split(separator: "\n").first(where: { $0.hasPrefix("Include: presets/eq/") }) {
            // `eqapo preset apply` writes an include line; load that preset's bands.
            var name = String(include.dropFirst("Include: presets/eq/".count))
            if name.hasSuffix(".txt") { name.removeLast(4) }
            if let preset = PresetStore.loadEQPreset(name) { settings = preset }
        }
    }

    func applyPreset(_ name: String) {
        guard let preset = PresetStore.loadEQPreset(name) else {
            lastMessage = "Could not read preset \(name)"
            return
        }
        var next = preset
        next.effect = settings.effect
        settings = next
        enabled = true
    }

    func applyEffect(_ name: String?) {
        settings.effect = name
    }

    func setGain(_ band: Int, _ value: Double) {
        guard band < settings.gains.count else { return }
        settings.gains[band] = (value * 10).rounded() / 10
        if settings.name.hasPrefix("[build]") || presetNames.contains(settings.name) {
            settings.name = "Custom"
        }
    }

    func resetFlat() {
        var next = EQSettings.flat
        next.bandwidthOctaves = settings.bandwidthOctaves
        next.effect = settings.effect
        settings = next
    }

    func savePreset(named name: String) {
        var copy = settings
        copy.name = name
        do {
            try PresetStore.saveEQPreset(copy)
            settings.name = name
            reloadPresetLists()
            lastMessage = "Saved preset \(name)"
        } catch {
            lastMessage = "Could not save preset: \(error.localizedDescription)"
        }
    }

    private func scheduleWrite() {
        writeTimer?.invalidate()
        writeTimer = Timer.scheduledTimer(withTimeInterval: 0.05, repeats: false) { [weak self] _ in
            Task { @MainActor in self?.writeConfig() }
        }
    }

    func writeConfig() {
        do {
            let text = enabled ? ConfigWriter.render(settings) : ConfigWriter.disabledConfig
            try PresetStore.writeConfig(text)
        } catch {
            lastMessage = "Could not write config.txt: \(error.localizedDescription)"
        }
    }

    func refreshStatus() {
        status = DaemonStatus.read()
    }

    // MARK: daemon control

    func startDaemon() {
        let (code, output) = EqapoTool.run([EqapoTool.isInstalled ? "start" : "install"])
        lastMessage = code == 0 ? "Daemon started" : output.trimmingCharacters(in: .whitespacesAndNewlines)
        DispatchQueue.main.asyncAfter(deadline: .now() + 1.5) { [weak self] in self?.refreshStatus() }
    }

    func stopDaemon() {
        let (code, output) = EqapoTool.run(["stop"])
        lastMessage = code == 0 ? "Daemon stopped (audio passes through)" : output.trimmingCharacters(in: .whitespacesAndNewlines)
        DispatchQueue.main.asyncAfter(deadline: .now() + 1.0) { [weak self] in self?.refreshStatus() }
    }

    func openConfigFolder() {
        _ = EqapoTool.run(["config", "path"])
        NSWorkspaceOpen(EqapoPaths.configDir)
    }

    func editConfig() {
        NSWorkspaceOpen(EqapoPaths.configFile)
    }
}

import AppKit
func NSWorkspaceOpen(_ url: URL) { NSWorkspace.shared.open(url) }
