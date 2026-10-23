#pragma once
#include <dispatch/dispatch.h>

#include <functional>
#include <string>

namespace eqapo {

// FSEvents watcher over the configuration directory tree. Changes are
// coalesced and reported once per burst on `queue`.
class ConfigWatcher {
public:
    explicit ConfigWatcher(dispatch_queue_t queue);
    ~ConfigWatcher();
    bool start(const std::string& directory, std::function<void()> onChange, double debounceSeconds = 0.1);
    void stop();

private:
    static void callback(const void* streamRef, void* info, size_t numEvents, void* eventPaths,
                         const unsigned int* eventFlags, const unsigned long long* eventIds);
    void fire();

    dispatch_queue_t queue_;
    void* stream_ = nullptr;  // FSEventStreamRef
    std::function<void()> onChange_;
    dispatch_source_t timer_ = nullptr;
    double debounce_ = 0.1;
};

}  // namespace eqapo
