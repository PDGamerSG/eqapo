import SwiftUI

/// Menu bar only application (LSUIElement in Info.plist). The window-style
/// MenuBarExtra hosts the 20-band equalizer.
@main
struct EqualizerAPOApp: App {
    @StateObject private var model = EQModel()

    var body: some Scene {
        MenuBarExtra("EqualizerAPO", systemImage: "slider.vertical.3") {
            ContentView().environmentObject(model)
        }
        .menuBarExtraStyle(.window)
    }
}
