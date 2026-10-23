import Foundation

/// PC Equalizer's fixed 20 band centres in Hz.
public let pcEqualizerBands: [Double] = [22, 31, 44, 62, 88, 124, 176, 250, 354, 500, 700, 1000, 1400, 2000, 2800, 4000, 5660, 8000, 11300, 16000]

/// Everything the 20-band front end controls.
public struct EQSettings: Equatable, Codable {
    public var gains: [Double]         // dB per band, 20 entries
    public var bandwidthOctaves: Double
    public var preampDb: Double
    public var name: String            // preset name shown in the Note: line
    public var effect: String?         // delay/echo preset name (presets/delay/<effect>.txt)

    public init(gains: [Double] = Array(repeating: 0, count: 20), bandwidthOctaves: Double = 1.2,
                preampDb: Double = 0, name: String = "Custom", effect: String? = nil) {
        self.gains = gains
        self.bandwidthOctaves = bandwidthOctaves
        self.preampDb = preampDb
        self.name = name
        self.effect = effect
    }

    public static let flat = EQSettings(name: "[build] Default")
}

/// Writes and reads PC Equalizer's Equalizer APO configuration layout byte for byte, e.g.
/// `Filter 12: ON  PEQ      Fc  1,000 Hz  Gain   -2.50 dB  BW Oct 02.600`.
public enum ConfigWriter {
    /// Frequency column exactly as PC Equalizer prints it: `22.0`, `124`, `1,000`, `11,300`, right-aligned to 6.
    public static func frequencyColumn(_ hz: Double) -> String {
        let text: String
        if hz < 100 {
            text = String(format: "%.1f", hz)
        } else {
            let whole = Int(hz.rounded())
            if whole >= 1000 {
                text = "\(whole / 1000),\(String(format: "%03d", whole % 1000))"
            } else {
                text = String(whole)
            }
        }
        return String(repeating: " ", count: max(0, 6 - text.count)) + text
    }

    public static func filterLine(index: Int, hz: Double, gainDb: Double, bandwidth: Double) -> String {
        String(format: "Filter %2d: ON  PEQ      Fc %@ Hz  Gain %7.2f dB  BW Oct %06.3f",
               index, frequencyColumn(hz), gainDb, bandwidth)
    }

    public static func render(_ s: EQSettings, date: Date = Date()) -> String {
        let formatter = DateFormatter()
        formatter.locale = Locale(identifier: "en_US_POSIX")
        formatter.dateFormat = "yyyy-MM-dd HH:mm:ss"
        var lines: [String] = []
        lines.append("Filter Settings file")
        lines.append("")
        lines.append("Date: \(formatter.string(from: date))")
        lines.append("")
        lines.append("Note: \(s.name)")
        lines.append("")
        lines.append("Equaliser: FBQ2496")
        lines.append(String(format: "Preamp: %6.1f dB", s.preampDb))
        for (i, hz) in pcEqualizerBands.enumerated() {
            let gain = i < s.gains.count ? s.gains[i] : 0
            lines.append(filterLine(index: i + 1, hz: hz, gainDb: gain, bandwidth: s.bandwidthOctaves))
        }
        if let effect = s.effect, !effect.isEmpty {
            lines.append("")
            lines.append("Include: presets/delay/\(effect).txt")
        }
        return lines.joined(separator: "\n") + "\n"
    }

    /// Config written when the equalizer is switched off: audio passes through unchanged.
    public static let disabledConfig = "# EqualizerAPO: equalizer switched off (audio passes through unchanged)\n"

    /// Parses a PC Equalizer style file. Returns nil when the file does not contain 20 PEQ bands
    /// at the PC Equalizer frequencies (hand-written configurations are left alone).
    public static func parse(_ text: String) -> EQSettings? {
        var settings = EQSettings()
        var found = 0
        let filterRegex = try! NSRegularExpression(
            pattern: #"^\s*Filter\s*\d*:\s*ON\s+PEQ\s+Fc\s*([-+0-9.,]+)\s*Hz\s+Gain\s*([-+0-9.]+)\s*dB\s+BW\s+Oct\s*([0-9.]+)"#,
            options: [.anchorsMatchLines])
        for rawLine in text.split(whereSeparator: { $0 == "\n" || $0 == "\r\n" }) {
            let line = String(rawLine)
            let range = NSRange(line.startIndex..., in: line)
            if let m = filterRegex.firstMatch(in: line, range: range) {
                let freqText = String(line[Range(m.range(at: 1), in: line)!]).replacingOccurrences(of: ",", with: "")
                guard let hz = Double(freqText),
                      let gain = Double(line[Range(m.range(at: 2), in: line)!]),
                      let bw = Double(line[Range(m.range(at: 3), in: line)!]) else { continue }
                if let band = pcEqualizerBands.firstIndex(where: { abs($0 - hz) < 0.5 }) {
                    settings.gains[band] = gain
                    settings.bandwidthOctaves = bw
                    found += 1
                }
            } else if line.hasPrefix("Preamp:") {
                let value = line.dropFirst("Preamp:".count).replacingOccurrences(of: "dB", with: "")
                    .trimmingCharacters(in: .whitespaces)
                settings.preampDb = Double(value) ?? 0
            } else if line.hasPrefix("Note:") {
                settings.name = line.dropFirst("Note:".count).trimmingCharacters(in: .whitespaces)
            } else if line.hasPrefix("Include: presets/delay/") {
                var effect = String(line.dropFirst("Include: presets/delay/".count))
                if effect.hasSuffix(".txt") { effect.removeLast(4) }
                settings.effect = effect
            }
        }
        return found == 20 ? settings : nil
    }
}
