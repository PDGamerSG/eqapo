import SwiftUI

/// Menu bar only application (LSUIElement in Info.plist). The AppKit status
/// item owns a persistent popover so the complete SwiftUI view is constructed
/// during launch instead of on the first menu-bar click.
@main
struct EqapoApp: App {
    @NSApplicationDelegateAdaptor(EqapoApplicationDelegate.self) private var applicationDelegate

    var body: some Scene {
        Settings { EmptyView() }
    }
}
