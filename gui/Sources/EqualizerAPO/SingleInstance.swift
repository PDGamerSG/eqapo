import AppKit
import Darwin
import EqualizerAPOCore

/// Prevents a second menu-bar process when launchd and a manual app launch
/// happen at the same time. `flock` is released automatically if the process
/// crashes, so this never leaves a stale lock behind.
final class EqapoSingleInstance: NSObject, NSApplicationDelegate {
    private var lockFileDescriptor: Int32 = -1

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

    func applicationWillTerminate(_ notification: Notification) {
        guard lockFileDescriptor >= 0 else { return }
        flock(lockFileDescriptor, LOCK_UN)
        close(lockFileDescriptor)
        lockFileDescriptor = -1
    }
}
