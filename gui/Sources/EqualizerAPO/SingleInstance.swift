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
    private var eventMonitors: [Any] = []

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
        installPopoverDismissMonitors()
    }

    @objc private func togglePopover(_ sender: Any?) {
        guard let button = statusItem?.button, let popover else { return }
        if popover.isShown {
            popover.performClose(sender)
        } else {
            popover.show(relativeTo: button.bounds, of: button, preferredEdge: .minY)
        }
    }

    /// A transient popover does not consistently receive clicks delivered to
    /// another application when this menu-bar app is an LSUIElement. Monitor
    /// both local and global mouse events so it closes outside the panel.
    private func installPopoverDismissMonitors() {
        let mouseDowns: NSEvent.EventTypeMask = [.leftMouseDown, .rightMouseDown, .otherMouseDown]

        if let monitor = NSEvent.addLocalMonitorForEvents(matching: mouseDowns, handler: { [weak self] event in
            self?.dismissPopoverIfNeeded(for: event)
            return event
        }) {
            eventMonitors.append(monitor)
        }

        if let monitor = NSEvent.addGlobalMonitorForEvents(matching: mouseDowns, handler: { [weak self] event in
            self?.dismissPopoverIfNeeded(for: event)
        }) {
            eventMonitors.append(monitor)
        }
    }

    private func dismissPopoverIfNeeded(for event: NSEvent) {
        guard let popover, popover.isShown,
              let popoverFrame = popover.contentViewController?.view.window?.frame else { return }

        let point: NSPoint
        if let window = event.window {
            point = window.convertToScreen(NSRect(origin: event.locationInWindow, size: .zero)).origin
        } else {
            point = NSEvent.mouseLocation
        }

        if popoverFrame.contains(point) { return }
        if let button = statusItem?.button,
           let window = button.window,
           window.convertToScreen(button.frame).contains(point) {
            return
        }
        popover.close()
    }

    func applicationWillTerminate(_ notification: Notification) {
        eventMonitors.forEach(NSEvent.removeMonitor)
        eventMonitors.removeAll()
        guard lockFileDescriptor >= 0 else { return }
        flock(lockFileDescriptor, LOCK_UN)
        close(lockFileDescriptor)
        lockFileDescriptor = -1
    }
}
