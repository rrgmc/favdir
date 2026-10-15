#include "MacApp.h"

#import <AppKit/AppKit.h>

@interface FavdirReopenHandler : NSObject
- (instancetype)initWithCallback:(std::function<void()>)callback;
- (void)handleReopen:(NSAppleEventDescriptor *)event
      withReplyEvent:(NSAppleEventDescriptor *)reply;
@end

@implementation FavdirReopenHandler {
    std::function<void()> _callback;
}

- (instancetype)initWithCallback:(std::function<void()>)callback
{
    if ((self = [super init]))
        _callback = std::move(callback);
    return self;
}

- (void)handleReopen:(NSAppleEventDescriptor *)event
      withReplyEvent:(NSAppleEventDescriptor *)reply
{
    _callback();
}

@end

namespace MacApp {

void whenReopened(std::function<void()> callback)
{
    static FavdirReopenHandler *handler = nil;
    handler = [[FavdirReopenHandler alloc] initWithCallback:std::move(callback)];
    // AppKit installs its own reopen handler while launching, so replace it afterwards.
    __block id observer = [NSNotificationCenter.defaultCenter
        addObserverForName:NSApplicationDidFinishLaunchingNotification
                    object:nil
                     queue:nil
                usingBlock:^(NSNotification *) {
                    [NSNotificationCenter.defaultCenter removeObserver:observer];
                    [NSAppleEventManager.sharedAppleEventManager
                        setEventHandler:handler
                            andSelector:@selector(handleReopen:withReplyEvent:)
                          forEventClass:kCoreEventClass
                             andEventID:kAEReopenApplication];
                }];
}

} // namespace MacApp
