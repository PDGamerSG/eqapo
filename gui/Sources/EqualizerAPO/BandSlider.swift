import SwiftUI

/// One vertical band: dB readout, slider, frequency label.
struct BandSlider: View {
    let label: String
    @Binding var value: Double
    let range: ClosedRange<Double> = -15 ... 15

    var body: some View {
        VStack(spacing: 4) {
            Text(String(format: "%+.1f", value))
                .font(.system(size: 9, design: .monospaced))
                .foregroundStyle(abs(value) < 0.05 ? .secondary : .primary)
                .frame(width: 34)
            GeometryReader { geo in
                Slider(value: $value, in: range, step: 0.1)
                    .rotationEffect(.degrees(-90))
                    .frame(width: geo.size.height, height: geo.size.width)
                    .position(x: geo.size.width / 2, y: geo.size.height / 2)
            }
            .frame(width: 22, height: 150)
            Text(label)
                .font(.system(size: 9))
                .foregroundStyle(.secondary)
                .frame(width: 34)
        }
        .contentShape(Rectangle())
        .onTapGesture(count: 2) { value = 0 }
        .help("\(label) Hz. Double-click to reset to 0 dB.")
    }
}
