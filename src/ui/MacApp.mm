#include "MacApp.h"

#import <AppKit/AppKit.h>

#include <QApplication>
#include <QDialog>
#include <QEvent>
#include <QPointer>
#include <QTimer>
#include <QWidget>

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

namespace {

// The native windows of the modal dialogs on screen; weak, so closed ones drop out.
NSHashTable<NSWindow *> *dialogWindows()
{
    static NSHashTable<NSWindow *> *windows = [NSHashTable weakObjectsHashTable];
    return windows;
}

// The NSWindow of a widget's window, or nil when it has none yet. Never creates one: a native
// file dialog keeps its QFileDialog without a window of its own.
NSWindow *nativeWindow(QWidget *widget)
{
    QWidget *top = widget ? widget->window() : nullptr;
    if (!top || !top->windowHandle())
        return nil;
    // A bridged cast: ARC refuses to turn an integer into an object pointer directly.
    auto *view = (__bridge NSView *)reinterpret_cast<void *>(top->winId());
    return view ? view.window : nil;
}

// AppKit puts the window of a modal session at the modal panel level, above every other app's
// windows, including the Screenshot controls.
void lower(NSWindow *window)
{
    if (window.visible && window.level == NSModalPanelWindowLevel)
        window.level = NSNormalWindowLevel;
}

void attach(QDialog *dialog)
{
    NSWindow *window = nativeWindow(dialog);
    if (!window)
        return;
    [dialogWindows() addObject:window];
    lower(window);
    // As a child window it stays directly above its owner and comes forward with it, without
    // the raised level. A dialog of a dialog attaches to that dialog.
    QWidget *owner = dialog->parentWidget() ? dialog->parentWidget()->window() : nullptr;
    NSWindow *ownerWindow = owner && owner->isVisible() ? nativeWindow(owner) : nil;
    if (ownerWindow && ownerWindow != window && window.parentWindow != ownerWindow)
        [ownerWindow addChildWindow:window ordered:NSWindowAbove];
}

void detach(QDialog *dialog)
{
    NSWindow *window = nativeWindow(dialog);
    if (!window)
        return;
    if (window.parentWindow)
        [window.parentWindow removeChildWindow:window];
    [dialogWindows() removeObject:window];
}

// Attaches each modal dialog once its native window is on screen, and detaches it when it
// hides. Qt makes the native window while showing, so the attachment waits for the event loop.
class DialogFilter : public QObject
{
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        auto *dialog = qobject_cast<QDialog *>(watched);
        if (!dialog || !dialog->isWindow())
            return QObject::eventFilter(watched, event);
        if (event->type() == QEvent::Show) {
            // exec() marks a dialog modal with WA_ShowModal before it sets the modality. A
            // native dialog, such as the folder picker, shows instead of its QDialog.
            const bool modal = dialog->windowModality() != Qt::NonModal
                || dialog->testAttribute(Qt::WA_ShowModal);
            if (modal && !dialog->testAttribute(Qt::WA_DontShowOnScreen)) {
                QPointer<QDialog> child(dialog);
                QTimer::singleShot(0, this, [child] {
                    if (child && child->isVisible())
                        attach(child);
                });
            }
        } else if (event->type() == QEvent::Hide) {
            detach(dialog);
        }
        return QObject::eventFilter(watched, event);
    }
};

} // namespace

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

void moveToActiveSpace(QWidget *window)
{
    // The offscreen platform used by the tests has no NSWindow behind winId().
    if (QGuiApplication::platformName() != QLatin1String("cocoa"))
        return;
    NSWindow *native = nativeWindow(window);
    if (native)
        native.collectionBehavior |= NSWindowCollectionBehaviorMoveToActiveSpace;
}

void attachModalDialogs()
{
    qApp->installEventFilter(new DialogFilter(qApp));
    // AppKit raises the window of a modal session again when the session starts and when the
    // app activates or deactivates, so lower the dialogs after each of those.
    for (NSNotificationName name in @[NSApplicationDidBecomeActiveNotification,
                                      NSApplicationDidResignActiveNotification,
                                      NSWindowDidBecomeKeyNotification]) {
        [NSNotificationCenter.defaultCenter addObserverForName:name
                                                        object:nil
                                                         queue:nil
                                                    usingBlock:^(NSNotification *) {
                                                        for (NSWindow *w in dialogWindows().allObjects)
                                                            lower(w);
                                                    }];
    }
}

} // namespace MacApp
