#include "GlobalHotkeyBackend.h"

#include <QCoreApplication>

#include <Carbon/Carbon.h>

namespace {

constexpr OSType Signature = 'FvDr';
constexpr UInt32 HotkeyId = 1;

int keyCode(Qt::Key key)
{
    static const int letters[] = {
        kVK_ANSI_A, kVK_ANSI_B, kVK_ANSI_C, kVK_ANSI_D, kVK_ANSI_E, kVK_ANSI_F, kVK_ANSI_G,
        kVK_ANSI_H, kVK_ANSI_I, kVK_ANSI_J, kVK_ANSI_K, kVK_ANSI_L, kVK_ANSI_M, kVK_ANSI_N,
        kVK_ANSI_O, kVK_ANSI_P, kVK_ANSI_Q, kVK_ANSI_R, kVK_ANSI_S, kVK_ANSI_T, kVK_ANSI_U,
        kVK_ANSI_V, kVK_ANSI_W, kVK_ANSI_X, kVK_ANSI_Y, kVK_ANSI_Z,
    };
    static const int digits[] = {
        kVK_ANSI_0, kVK_ANSI_1, kVK_ANSI_2, kVK_ANSI_3, kVK_ANSI_4,
        kVK_ANSI_5, kVK_ANSI_6, kVK_ANSI_7, kVK_ANSI_8, kVK_ANSI_9,
    };
    static const int functionKeys[] = {
        kVK_F1,  kVK_F2,  kVK_F3,  kVK_F4,  kVK_F5,  kVK_F6,  kVK_F7,  kVK_F8,  kVK_F9,  kVK_F10,
        kVK_F11, kVK_F12, kVK_F13, kVK_F14, kVK_F15, kVK_F16, kVK_F17, kVK_F18, kVK_F19, kVK_F20,
    };

    if (key >= Qt::Key_A && key <= Qt::Key_Z)
        return letters[key - Qt::Key_A];
    if (key >= Qt::Key_0 && key <= Qt::Key_9)
        return digits[key - Qt::Key_0];
    if (key >= Qt::Key_F1 && key <= Qt::Key_F20)
        return functionKeys[key - Qt::Key_F1];

    switch (key) {
    case Qt::Key_Space: return kVK_Space;
    case Qt::Key_Return: return kVK_Return;
    case Qt::Key_Enter: return kVK_ANSI_KeypadEnter;
    case Qt::Key_Tab: return kVK_Tab;
    case Qt::Key_Backspace: return kVK_Delete;
    case Qt::Key_Delete: return kVK_ForwardDelete;
    case Qt::Key_Escape: return kVK_Escape;
    case Qt::Key_Home: return kVK_Home;
    case Qt::Key_End: return kVK_End;
    case Qt::Key_PageUp: return kVK_PageUp;
    case Qt::Key_PageDown: return kVK_PageDown;
    case Qt::Key_Left: return kVK_LeftArrow;
    case Qt::Key_Right: return kVK_RightArrow;
    case Qt::Key_Up: return kVK_UpArrow;
    case Qt::Key_Down: return kVK_DownArrow;
    case Qt::Key_Minus: return kVK_ANSI_Minus;
    case Qt::Key_Equal: return kVK_ANSI_Equal;
    case Qt::Key_BracketLeft: return kVK_ANSI_LeftBracket;
    case Qt::Key_BracketRight: return kVK_ANSI_RightBracket;
    case Qt::Key_Semicolon: return kVK_ANSI_Semicolon;
    case Qt::Key_Apostrophe: return kVK_ANSI_Quote;
    case Qt::Key_Comma: return kVK_ANSI_Comma;
    case Qt::Key_Period: return kVK_ANSI_Period;
    case Qt::Key_Slash: return kVK_ANSI_Slash;
    case Qt::Key_Backslash: return kVK_ANSI_Backslash;
    case Qt::Key_QuoteLeft: return kVK_ANSI_Grave;
    default: return -1;
    }
}

class MacBackend final : public GlobalHotkey::Backend
{
public:
    explicit MacBackend(std::function<void()> onActivated)
        : m_onActivated(std::move(onActivated))
    {
        const EventTypeSpec spec = {kEventClassKeyboard, kEventHotKeyPressed};
        InstallApplicationEventHandler(&MacBackend::handler, 1, &spec, this, &m_handler);
    }

    ~MacBackend() override
    {
        unregisterKey();
        if (m_handler)
            RemoveEventHandler(m_handler);
    }

    bool registerKey(Qt::Key key, Qt::KeyboardModifiers modifiers, QString *error) override
    {
        const int code = keyCode(key);
        if (code < 0) {
            *error = QCoreApplication::translate("GlobalHotkey", "this key is not supported.");
            return false;
        }
        // Qt swaps Control and Command on macOS: ControlModifier is the Command key.
        UInt32 mods = 0;
        if (modifiers & Qt::ControlModifier)
            mods |= cmdKey;
        if (modifiers & Qt::MetaModifier)
            mods |= controlKey;
        if (modifiers & Qt::AltModifier)
            mods |= optionKey;
        if (modifiers & Qt::ShiftModifier)
            mods |= shiftKey;

        const EventHotKeyID id = {Signature, HotkeyId};
        const OSStatus status = RegisterEventHotKey(static_cast<UInt32>(code), mods, id,
                                                    GetApplicationEventTarget(), 0, &m_hotkey);
        if (status != noErr) {
            m_hotkey = nullptr;
            *error = status == eventHotKeyExistsErr
                         ? QCoreApplication::translate("GlobalHotkey",
                                                       "another application already uses it.")
                         : QCoreApplication::translate("GlobalHotkey", "macOS refused it.");
            return false;
        }
        return true;
    }

    void unregisterKey() override
    {
        if (m_hotkey) {
            UnregisterEventHotKey(m_hotkey);
            m_hotkey = nullptr;
        }
    }

private:
    static OSStatus handler(EventHandlerCallRef, EventRef event, void *userData)
    {
        EventHotKeyID id;
        if (GetEventParameter(event, kEventParamDirectObject, typeEventHotKeyID, nullptr,
                              sizeof(id), nullptr, &id)
                == noErr
            && id.signature == Signature && id.id == HotkeyId) {
            static_cast<MacBackend *>(userData)->m_onActivated();
            return noErr;
        }
        return eventNotHandledErr;
    }

    std::function<void()> m_onActivated;
    EventHandlerRef m_handler = nullptr;
    EventHotKeyRef m_hotkey = nullptr;
};

} // namespace

std::unique_ptr<GlobalHotkey::Backend> GlobalHotkey::Backend::create(std::function<void()> onActivated)
{
    return std::make_unique<MacBackend>(std::move(onActivated));
}
