#include "daemon/ConfigWatcher.h"

#import <CoreServices/CoreServices.h>
#import <Foundation/Foundation.h>

namespace eqapo {

ConfigWatcher::ConfigWatcher(dispatch_queue_t queue) : queue_(queue) {}

ConfigWatcher::~ConfigWatcher() { stop(); }

bool ConfigWatcher::start(const std::string& directory, std::function<void()> onChange, double debounceSeconds) {
    stop();
    onChange_ = std::move(onChange);
    debounce_ = debounceSeconds;
    @autoreleasepool {
        NSArray* paths = @[ [NSString stringWithUTF8String:directory.c_str()] ];
        FSEventStreamContext context{};
        context.info = this;
        FSEventStreamRef stream = FSEventStreamCreate(
            nullptr, reinterpret_cast<FSEventStreamCallback>(&ConfigWatcher::callback), &context,
            (__bridge CFArrayRef)paths, kFSEventStreamEventIdSinceNow, 0.05,
            kFSEventStreamCreateFlagFileEvents | kFSEventStreamCreateFlagNoDefer);
        if (!stream) return false;
        FSEventStreamSetDispatchQueue(stream, queue_);
        if (!FSEventStreamStart(stream)) {
            FSEventStreamInvalidate(stream);
            FSEventStreamRelease(stream);
            return false;
        }
        stream_ = stream;
    }
    return true;
}

void ConfigWatcher::callback(const void* streamRef, void* info, size_t, void*, const unsigned int*,
                             const unsigned long long*) {
    static_cast<ConfigWatcher*>(info)->fire();
}

void ConfigWatcher::fire() {
    // debounce: (re)arm a one-shot timer
    if (timer_) {
        dispatch_source_cancel(timer_);
        timer_ = nullptr;
    }
    timer_ = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, queue_);
    dispatch_source_set_timer(timer_, dispatch_time(DISPATCH_TIME_NOW, static_cast<int64_t>(debounce_ * NSEC_PER_SEC)),
                              DISPATCH_TIME_FOREVER, 10 * NSEC_PER_MSEC);
    dispatch_source_t t = timer_;
    dispatch_source_set_event_handler(t, ^{
      dispatch_source_cancel(t);
      if (timer_ == t) timer_ = nullptr;
      if (onChange_) onChange_();
    });
    dispatch_resume(timer_);
}

void ConfigWatcher::stop() {
    if (stream_) {
        FSEventStreamRef stream = static_cast<FSEventStreamRef>(stream_);
        FSEventStreamStop(stream);
        FSEventStreamInvalidate(stream);
        FSEventStreamRelease(stream);
        stream_ = nullptr;
    }
    if (timer_) {
        dispatch_source_cancel(timer_);
        timer_ = nullptr;
    }
}

}  // namespace eqapo
