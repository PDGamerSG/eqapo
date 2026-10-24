import AppKit
import EqualizerAPOCore
import SwiftUI

struct ContentView: View {
    // Note: `@State` needs the SwiftUI macro plugin that ships only with Xcode; the
    // Command Line Tools toolchain lacks it, so transient view state lives in the model.
    @EnvironmentObject var model: EQModel

    private static let bandLabels: [String] = pcEqualizerBands.map { hz in
        hz >= 1000 ? String(format: "%gk", hz / 1000) : String(format: "%g", hz)
    }

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            header
            Divider()
            presetRow
            bandGrid
            controlsRow
            Divider()
            footer
        }
        .padding(14)
        .frame(width: 820)
        .sheet(isPresented: $model.showSave) { savePresetSheet }
    }

    // MARK: sections

    private var header: some View {
        HStack(spacing: 8) {
            logo
            Text("eqapo").font(.headline)
            Spacer()
            Toggle("Enabled", isOn: $model.enabled).toggleStyle(.switch)
        }
    }

    private var logo: some View {
        Group {
            if let url = Bundle.module.url(forResource: "eqapo-logo", withExtension: "png"),
               let image = NSImage(contentsOf: url) {
                Image(nsImage: image)
                    .resizable()
                    .interpolation(.high)
                    .scaledToFit()
            } else {
                Image(systemName: "waveform.path.ecg")
                    .font(.title2)
                    .foregroundStyle(.cyan)
            }
        }
        .frame(width: 30, height: 30)
        .padding(3)
        .background(
            LinearGradient(colors: [.blue.opacity(0.35), .purple.opacity(0.35)], startPoint: .topLeading, endPoint: .bottomTrailing),
            in: RoundedRectangle(cornerRadius: 8)
        )
        .accessibilityLabel("eqapo logo")
    }

    private var presetRow: some View {
        HStack(spacing: 12) {
            selector(label: "Preset", value: model.settings.name, items: model.presetNames) { model.applyPreset($0) }
            selector(label: "Effect", value: model.settings.effect ?? "None", items: ["None"] + model.effectNames) {
                model.applyEffect($0 == "None" ? nil : $0)
            }
            Spacer()
            Button("Save") { model.saveName = model.settings.name == "Custom" ? "" : model.settings.name; model.showSave = true }
            Button("Flat") { model.resetFlat() }
        }
    }

    private func selector(label: String, value: String, items: [String], onSelect: @escaping (String) -> Void) -> some View {
        HStack(spacing: 8) {
            Text(label).foregroundStyle(.secondary)
            Menu {
                ForEach(items, id: \.self) { item in
                    Button(item) { onSelect(item) }
                }
            } label: {
                HStack(spacing: 8) {
                    Text(value).lineLimit(1)
                    Spacer(minLength: 8)
                    Image(systemName: "chevron.up.chevron.down").font(.caption2)
                }
                .padding(.horizontal, 10)
                .frame(width: 238, height: 28, alignment: .leading)
                .background(Color.white.opacity(0.12), in: RoundedRectangle(cornerRadius: 7))
            }
            .menuStyle(.borderlessButton)
        }
    }

    private var bandGrid: some View {
        HStack(alignment: .bottom, spacing: 2) {
            ForEach(0 ..< pcEqualizerBands.count, id: \.self) { i in
                BandSlider(label: Self.bandLabels[i], value: Binding(
                    get: { model.settings.gains[i] },
                    set: { model.setGain(i, $0) }))
            }
        }
        .frame(maxWidth: .infinity)
        .opacity(model.enabled ? 1 : 0.45)
    }

    private var controlsRow: some View {
        HStack(spacing: 14) {
            Text("Preamp").foregroundStyle(.secondary)
            Slider(value: $model.settings.preampDb, in: -20 ... 20, step: 0.5).frame(width: 180)
            Text(String(format: "%+.1f dB", model.settings.preampDb))
                .font(.system(.body, design: .monospaced)).frame(width: 70, alignment: .leading)
            Spacer()
            Text("Bandwidth").foregroundStyle(.secondary)
            Picker("Bandwidth", selection: $model.settings.bandwidthOctaves) {
                ForEach(EQModel.bandwidthChoices, id: \.self) { Text(String(format: "%.1f oct", $0)).tag($0) }
            }
            .labelsHidden()
            .frame(width: 100)
        }
    }

    private var footer: some View {
        VStack(alignment: .leading, spacing: 6) {
            HStack(spacing: 8) {
                Circle().fill(statusColor).frame(width: 9, height: 9)
                Text(statusText).font(.callout)
                Spacer()
                if model.status.alive {
                    Button("Stop") { model.stopDaemon() }
                } else {
                    Button(EqapoTool.isInstalled ? "Start" : "Install & start") { model.startDaemon() }
                }
                Menu {
                    Button("Edit config.txt") { model.editConfig() }
                    Button("Open config folder") { model.openConfigFolder() }
                    Button("Reload presets") { model.reloadPresetLists() }
                    Divider()
                    Toggle("Start eqapo at login", isOn: Binding(
                        get: { model.launchAtLogin },
                        set: { model.setLaunchAtLogin($0) }))
                    Divider()
                    Button("Quit eqapo") { NSApp.terminate(nil) }
                } label: {
                    Image(systemName: "ellipsis.circle")
                }
                .menuStyle(.borderlessButton)
                .frame(width: 30)
            }
            if !model.lastMessage.isEmpty {
                Text(model.lastMessage).font(.caption).foregroundStyle(.secondary).lineLimit(2)
            }
            ForEach(model.status.configErrors, id: \.self) { err in
                Text(err).font(.caption).foregroundStyle(.red).lineLimit(2)
            }
        }
    }

    private var savePresetSheet: some View {
        VStack(alignment: .leading, spacing: 12) {
            Text("Save preset").font(.headline)
            TextField("Preset name", text: $model.saveName).frame(width: 260)
            HStack {
                Spacer()
                Button("Cancel") { model.showSave = false }
                Button("Save") {
                    let name = model.saveName.trimmingCharacters(in: .whitespaces)
                    if !name.isEmpty { model.savePreset(named: name) }
                    model.showSave = false
                }
                .keyboardShortcut(.defaultAction)
                .disabled(model.saveName.trimmingCharacters(in: .whitespaces).isEmpty)
            }
        }
        .padding(20)
    }

    // MARK: status helpers

    private var statusColor: Color {
        guard model.status.alive else { return .gray }
        return model.status.state == "running" ? .green : .orange
    }

    private var statusText: String {
        let s = model.status
        guard s.alive else { return "Daemon not running — audio is not equalized" }
        if s.state != "running" { return "Daemon: \(s.state) \(s.error)" }
        return String(format: "%@ · %.0f Hz · %d ch · %d frames · %d filters · %.1f%% CPU",
                      s.deviceName, s.sampleRate, s.outputChannels, s.bufferFrames, s.filterCount, s.avgLoadPercent)
    }
}
