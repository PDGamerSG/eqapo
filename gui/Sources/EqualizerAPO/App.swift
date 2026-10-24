import SwiftUI

/// Menu bar only application (LSUIElement in Info.plist). The window-style
/// MenuBarExtra hosts the 20-band equalizer.
@main
struct EqapoApp: App {
    @NSApplicationDelegateAdaptor(EqapoSingleInstance.self) private var singleInstance
    @StateObject private var model = EQModel()

    var body: some Scene {
        MenuBarExtra("eqapo", systemImage: "waveform.path.ecg") {
            ContentView().environmentObject(model)
        }
        .menuBarExtraStyle(.window)
    }
}
