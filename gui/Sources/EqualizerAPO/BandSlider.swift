import SwiftUI

/// The complete 20-band graph. One Canvas draws all bands while transparent
/// hit areas keep each band independently draggable and resettable.
struct BandGraph: View {
    @Binding var values: [Double]
    let labels: [String]
    let onChange: (Int, Double) -> Void

    private let range: ClosedRange<Double> = -15 ... 15
    private let ticks: [Double] = [-15, -10, -5, 0, 5, 10, 15]

    var body: some View {
        VStack(spacing: 4) {
            valueLabels
            graph
            frequencyLabels
        }
    }

    private var valueLabels: some View {
        HStack(spacing: 2) {
            ForEach(values.indices, id: \.self) { index in
                Text(String(format: "%+.1f", values[index]))
                    .font(.system(size: 9, design: .monospaced))
                    .foregroundStyle(abs(values[index]) < 0.05 ? .secondary : .primary)
                    .frame(maxWidth: .infinity)
            }
        }
        .frame(height: 13)
    }

    private var graph: some View {
        GeometryReader { geo in
            let height = geo.size.height
            let count = max(values.count, 1)
            let spacing: CGFloat = 2
            let columnWidth = max(1, (geo.size.width - spacing * CGFloat(count - 1)) / CGFloat(count))

            ZStack {
                Canvas { context, size in
                    for index in values.indices {
                        let centerX = (columnWidth + spacing) * CGFloat(index) + columnWidth / 2
                        let zeroY = yPosition(for: 0, height: size.height)
                        let thumbY = yPosition(for: values[index], height: size.height)

                        let trackRect = CGRect(x: centerX - 3.5, y: 4, width: 7, height: max(0, size.height - 8))
                        context.fill(Path(roundedRect: trackRect, cornerRadius: 3.5), with: .color(.white.opacity(0.16)))

                        for tick in ticks {
                            let tickY = yPosition(for: tick, height: size.height)
                            let tickWidth: CGFloat = tick == 0 ? min(columnWidth - 4, 26) : min(columnWidth - 10, 12)
                            let tickHeight: CGFloat = tick == 0 ? 1.5 : 1
                            let tickRect = CGRect(x: centerX - tickWidth / 2, y: tickY - tickHeight / 2,
                                                  width: max(1, tickWidth), height: tickHeight)
                            context.fill(Path(tickRect), with: .color(tick == 0 ? .white.opacity(0.55) : .white.opacity(0.22)))
                        }

                        var fillPath = Path()
                        fillPath.move(to: CGPoint(x: centerX, y: zeroY))
                        fillPath.addLine(to: CGPoint(x: centerX, y: thumbY))
                        let accent = values[index] >= 0 ? Color.cyan : Color.purple
                        context.stroke(fillPath, with: .color(accent), style: StrokeStyle(lineWidth: 8, lineCap: .round))

                        let thumbRect = CGRect(x: centerX - 7.5, y: thumbY - 7.5, width: 15, height: 15)
                        context.fill(Path(ellipseIn: thumbRect), with: .color(accent))
                        context.stroke(Path(ellipseIn: thumbRect), with: .color(.white.opacity(0.9)), lineWidth: 1.5)
                    }
                }

                HStack(spacing: spacing) {
                    ForEach(values.indices, id: \.self) { index in
                        Color.clear
                            .frame(maxWidth: .infinity, maxHeight: .infinity)
                            .contentShape(Rectangle())
                            .gesture(DragGesture(minimumDistance: 0).onChanged { drag in
                                let normalized = 1 - min(1, max(0, drag.location.y / max(1, height)))
                                let next = range.lowerBound + normalized * (range.upperBound - range.lowerBound)
                                onChange(index, (next * 10).rounded() / 10)
                            })
                            .onTapGesture(count: 2) { onChange(index, 0) }
                            .help("\(labels[index]) Hz. Double-click to reset to 0 dB.")
                            .accessibilityElement(children: .ignore)
                            .accessibilityLabel("\(labels[index]) Hz")
                            .accessibilityValue(String(format: "%+.1f dB", values[index]))
                    }
                }
            }
        }
        .frame(height: 150)
    }

    private var frequencyLabels: some View {
        HStack(spacing: 2) {
            ForEach(labels, id: \.self) { label in
                Text(label)
                    .font(.system(size: 9))
                    .foregroundStyle(.secondary)
                    .frame(maxWidth: .infinity)
            }
        }
        .frame(height: 13)
    }

    private func yPosition(for db: Double, height: CGFloat) -> CGFloat {
        let normalized = (db - range.lowerBound) / (range.upperBound - range.lowerBound)
        return max(8, min(height - 8, (1 - normalized) * height))
    }
}
