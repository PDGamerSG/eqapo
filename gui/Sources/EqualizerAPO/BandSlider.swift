import SwiftUI

/// One vertical band: dB readout, slider, frequency label.
struct BandSlider: View {
    let label: String
    @Binding var value: Double
    let range: ClosedRange<Double> = -15 ... 15

    private let ticks: [Double] = [-15, -10, -5, 0, 5, 10, 15]

    var body: some View {
        VStack(spacing: 4) {
            Text(String(format: "%+.1f", value))
                .font(.system(size: 9, design: .monospaced))
                .foregroundStyle(abs(value) < 0.05 ? .secondary : .primary)
                .frame(width: 34)
            graph
                .frame(width: 30, height: 150)
            Text(label)
                .font(.system(size: 9))
                .foregroundStyle(.secondary)
                .frame(width: 34)
        }
        .contentShape(Rectangle())
        .onTapGesture(count: 2) { value = 0 }
        .help("\(label) Hz. Double-click to reset to 0 dB.")
    }

    private var graph: some View {
        GeometryReader { geo in
            let height = geo.size.height
            let centerX = geo.size.width / 2
            let zeroY = yPosition(for: 0, height: height)
            let thumbY = yPosition(for: value, height: height)

            ZStack {
                // A high-contrast track and zero line stay visible in both
                // light and dark macOS appearances.
                Capsule()
                    .fill(Color.white.opacity(0.16))
                    .frame(width: 7, height: max(0, height - 8))
                ForEach(ticks, id: \.self) { tick in
                    Rectangle()
                        .fill(tick == 0 ? Color.white.opacity(0.55) : Color.white.opacity(0.22))
                        .frame(width: tick == 0 ? 26 : 12, height: tick == 0 ? 1.5 : 1)
                        .position(x: centerX, y: yPosition(for: tick, height: height))
                }
                Path { path in
                    path.move(to: CGPoint(x: centerX, y: zeroY))
                    path.addLine(to: CGPoint(x: centerX, y: thumbY))
                }
                .stroke(value >= 0 ? Color.cyan : Color.purple, style: StrokeStyle(lineWidth: 8, lineCap: .round))
                Circle()
                    .fill(value >= 0 ? Color.cyan : Color.purple)
                    .overlay(Circle().stroke(Color.white.opacity(0.9), lineWidth: 1.5))
                    .shadow(color: (value >= 0 ? Color.cyan : Color.purple).opacity(0.65), radius: 5)
                    .frame(width: 15, height: 15)
                    .position(x: centerX, y: thumbY)
            }
            .contentShape(Rectangle())
            .gesture(DragGesture(minimumDistance: 0).onChanged { drag in
                let normalized = 1 - min(1, max(0, drag.location.y / height))
                value = range.lowerBound + normalized * (range.upperBound - range.lowerBound)
                value = (value * 10).rounded() / 10
            })
        }
        .accessibilityElement(children: .ignore)
        .accessibilityLabel("\(label) Hz")
        .accessibilityValue(String(format: "%+.1f dB", value))
    }

    private func yPosition(for db: Double, height: CGFloat) -> CGFloat {
        let normalized = (db - range.lowerBound) / (range.upperBound - range.lowerBound)
        return max(8, min(height - 8, (1 - normalized) * height))
    }
}
