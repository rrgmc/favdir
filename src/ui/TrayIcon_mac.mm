#include "TrayIcon.h"

#include <QAction>
#include <QGuiApplication>
#include <QIcon>
#include <QImage>
#include <QMenu>
#include <QPointer>

#import <AppKit/AppKit.h>

// Rebuilds the native menu from the QMenu each time it opens, so it follows the actions' text
// and enabled state.
@interface FavdirTrayMenuDelegate : NSObject <NSMenuDelegate>
- (instancetype)initWithMenu:(QMenu *)menu;
@end

@implementation FavdirTrayMenuDelegate {
    QPointer<QMenu> _menu;
}

- (instancetype)initWithMenu:(QMenu *)menu
{
    if ((self = [super init]))
        _menu = menu;
    return self;
}

- (void)menuNeedsUpdate:(NSMenu *)nsMenu
{
    [nsMenu removeAllItems];
    if (!_menu)
        return;
    const QList<QAction *> actions = _menu->actions();
    for (qsizetype i = 0; i < actions.size(); ++i) {
        QAction *action = actions[i];
        if (!action->isVisible())
            continue;
        if (action->isSeparator()) {
            [nsMenu addItem:[NSMenuItem separatorItem]];
            continue;
        }
        // Native menus have no mnemonics.
        const QString title = QString(action->text()).remove(QLatin1Char('&'));
        NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:title.toNSString()
                                                      action:@selector(itemClicked:)
                                               keyEquivalent:@""];
        item.target = self;
        item.tag = i;
        item.enabled = action->isEnabled();
        [nsMenu addItem:item];
    }
}

- (void)itemClicked:(NSMenuItem *)item
{
    if (!_menu)
        return;
    const QList<QAction *> actions = _menu->actions();
    if (item.tag < 0 || item.tag >= actions.size())
        return;
    // Run the action after the menu has finished tracking.
    QMetaObject::invokeMethod(actions[item.tag], &QAction::trigger, Qt::QueuedConnection);
}

@end

namespace {

NSImage *toNSImage(const QIcon &icon)
{
    const NSSize size = NSMakeSize(18, 18);
    NSImage *image = [[NSImage alloc] initWithSize:size];
    for (const qreal scale : {1.0, 2.0}) {
        const QImage qimage = icon.pixmap(QSize(18, 18), scale).toImage();
        CGImageRef cgImage = qimage.toCGImage();
        if (!cgImage)
            continue;
        NSBitmapImageRep *rep = [[NSBitmapImageRep alloc] initWithCGImage:cgImage];
        CGImageRelease(cgImage);
        rep.size = size;
        [image addRepresentation:rep];
    }
    [image setTemplate:icon.isMask()];  // "template" is a C++ keyword
    return image;
}

} // namespace

class TrayIcon::Private
{
public:
    NSStatusItem *item = nil;
    NSMenu *menu = nil;
    FavdirTrayMenuDelegate *delegate = nil;
};

bool TrayIcon::isAvailable()
{
    // The offscreen platform used by the tests has no menu bar.
    return QGuiApplication::platformName() == QLatin1String("cocoa");
}

TrayIcon::TrayIcon(const QIcon &icon, const QString &toolTip, QMenu *menu, QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>())
{
    d->delegate = [[FavdirTrayMenuDelegate alloc] initWithMenu:menu];
    d->menu = [[NSMenu alloc] init];
    d->menu.autoenablesItems = NO;
    d->menu.delegate = d->delegate;

    d->item = [NSStatusBar.systemStatusBar statusItemWithLength:NSSquareStatusItemLength];
    d->item.button.image = toNSImage(icon);
    d->item.button.toolTip = toolTip.toNSString();
    d->item.menu = d->menu;
}

TrayIcon::~TrayIcon()
{
    [NSStatusBar.systemStatusBar removeStatusItem:d->item];
}

void TrayIcon::showWarning(const QString &, const QString &)
{
    // Qt 6.8 used the deprecated NSUserNotificationCenter here. The status bar still shows the
    // message.
}

void TrayIcon::setDockIconVisible(bool visible)
{
    // An accessory app has no Dock icon and no menu bar, like a menu bar app.
    const NSApplicationActivationPolicy policy = visible
                                                     ? NSApplicationActivationPolicyRegular
                                                     : NSApplicationActivationPolicyAccessory;
    if (NSApp.activationPolicy == policy)
        return;
    [NSApp setActivationPolicy:policy];
    // An app that was an accessory does not come to the front by itself.
    if (visible) {
        if (@available(macOS 14, *))
            [NSApp activate];
        else
            [NSApp activateIgnoringOtherApps:YES];
    }
}
