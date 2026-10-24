import AppKit
import Darwin
import EqualizerAPOCore
import SwiftUI

/// Prevents a second menu-bar process when launchd and a manual app launch
/// happen at the same time. `flock` is released automatically if the process
/// crashes, so this never leaves a stale lock behind.
@MainActor
final class EqapoApplicationDelegate: NSObject, NSApplicationDelegate {
    private var lockFileDescriptor: Int32 = -1
    private let model = EQModel()
    private var statusItem: NSStatusItem?
    private var popover: NSPopover?

    func applicationWillFinishLaunching(_ notification: Notification) {
        let directory = EqapoPaths.root
        try? FileManager.default.createDirectory(at: directory, withIntermediateDirectories: true)
        let path = directory.appendingPathComponent("eqapo-ui.lock").path
        lockFileDescriptor = open(path, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR)
        guard lockFileDescriptor >= 0, flock(lockFileDescriptor, LOCK_EX | LOCK_NB) == 0 else {
            if lockFileDescriptor >= 0 { close(lockFileDescriptor) }
            lockFileDescriptor = -1
            DispatchQueue.main.async { NSApp.terminate(nil) }
            return
        }
    }

    func applicationDidFinishLaunching(_ notification: Notification) {
        let item = NSStatusBar.system.statusItem(withLength: NSStatusItem.squareLength)
        guard let button = item.button else { return }
        button.image = NSImage(systemSymbolName: "waveform.path.ecg", accessibilityDescription: "eqapo")
        button.image?.isTemplate = true
        button.toolTip = "eqapo"
        button.target = self
        button.action = #selector(togglePopover(_:))
        button.sendAction(on: [.leftMouseUp, .rightMouseUp])

        let host = NSHostingController(rootView: ContentView().environmentObject(model))
        host.view.frame = NSRect(x: 0, y: 0, width: 820, height: 500)
        let popover = NSPopover()
        popover.contentViewController = host
        popover.contentSize = NSSize(width: 820, height: 500)
        popover.behavior = .transient
        popover.animates = false
        self.statusItem = item
        self.popover = popover
    }

    @objc private func togglePopover(_ sender: Any?) {
        guard let button = statusItem?.button, let popover else { return }
        if popover.isShown {
            popover.performClose(sender)
        } else {
            popover.show(relativeTo: button.bounds, of: button, preferredEdge: .minY)
        }
    }

    func applicationWillTerminate(_ notification: Notification) {
        guard lockFileDescriptor >= 0 else { return }
        flock(lockFileDescriptor, LOCK_UN)
        close(lockFileDescriptor)
        lockFileDescriptor = -1
    }
}
