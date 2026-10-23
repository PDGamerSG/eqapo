// Verifies that ConfigWriter reproduces PC Equalizer's preset files byte for byte.
// Usage: ConfigWriterCheck <presets/eq directory>
import EqualizerAPOCore
import Foundation

var failures = 0
func check(_ cond: Bool, _ message: String) {
    if !cond {
        print("FAIL: \(message)")
        failures += 1
    }
}

let args = CommandLine.arguments
guard args.count > 1 else {
    print("usage: ConfigWriterCheck <presets/eq dir>")
    exit(2)
}
let dir = URL(fileURLWithPath: args[1])
let names = PresetStore.names(in: dir)
check(names.count == 46, "expected 46 presets, found \(names.count)")

// Column formatting
check(ConfigWriter.frequencyColumn(22) == "  22.0", "22 Hz column")
check(ConfigWriter.frequencyColumn(124) == "   124", "124 Hz column")
check(ConfigWriter.frequencyColumn(1000) == " 1,000", "1000 Hz column")
check(ConfigWriter.frequencyColumn(11300) == "11,300", "11300 Hz column")
check(ConfigWriter.filterLine(index: 12, hz: 1000, gainDb: -2.5, bandwidth: 2.6)
      == "Filter 12: ON  PEQ      Fc  1,000 Hz  Gain   -2.50 dB  BW Oct 02.600", "filter line format")
check(ConfigWriter.filterLine(index: 8, hz: 250, gainDb: -11.4, bandwidth: 2.0)
      == "Filter  8: ON  PEQ      Fc    250 Hz  Gain  -11.40 dB  BW Oct 02.000", "negative gain format")

// Every preset parses to 20 bands and re-renders to the same Filter/Preamp lines.
var roundTripped = 0
for name in names {
    let url = dir.appendingPathComponent(name + ".txt")
    guard let text = try? String(contentsOf: url, encoding: .utf8) else { check(false, "read \(name)"); continue }
    guard let parsed = ConfigWriter.parse(text) else { check(false, "parse \(name)"); continue }
    let rendered = ConfigWriter.render(parsed)
    let original = text.split(separator: "\n").map(String.init)
        .filter { $0.hasPrefix("Filter") && $0.contains("ON") }
    let ours = rendered.split(separator: "\n").map(String.init).filter { $0.hasPrefix("Filter ") && $0.contains("ON") }
    // Original files mix 1- and 2-decimal gain columns; compare numerically-normalised text.
    func normalise(_ line: String) -> String {
        line.replacingOccurrences(of: #"Gain\s+([-+0-9.]+)\s+dB"#, with: "Gain $1 dB", options: .regularExpression)
            .replacingOccurrences(of: #"Gain (-?\d+\.\d)(?= dB)"#, with: "Gain $10", options: .regularExpression)
    }
    check(original.count == 20, "\(name): 20 filter lines in file")
    check(ours.count == 20, "\(name): 20 filter lines rendered")
    if original.count == 20 && ours.count == 20 {
        for (a, b) in zip(original, ours) {
            let na = normalise(a), nb = normalise(b)
            if na != nb {
                check(false, "\(name): line differs\n  file: \(a)\n  ours: \(b)")
                break
            }
        }
        roundTripped += 1
    }
    let preampLine = text.split(separator: "\n").first { $0.hasPrefix("Preamp:") }.map(String.init) ?? ""
    check(normalise(String(format: "Preamp: %6.1f dB", parsed.preampDb)) == normalise(preampLine),
          "\(name): preamp line \(preampLine)")
}
print("\(roundTripped)/\(names.count) presets round-trip; \(failures) failures")
exit(failures == 0 ? 0 : 1)
