#include "GlobalHotkey.h"

#include "GlobalHotkeyBackend.h"

#include <QCoreApplication>

GlobalHotkey::GlobalHotkey(QObject *parent)
    : QObject(parent)
    , m_backend(Backend::create([this] { emit activated(); }))
{
}

GlobalHotkey::~GlobalHotkey() = default;

bool GlobalHotkey::setShortcut(const QKeySequence &sequence, QString *error)
{
    m_backend->unregisterKey();
    m_sequence = QKeySequence();
    if (sequence.isEmpty())
        return true;

    const QKeyCombination combo = sequence[0];
    QString reason;
    if (!m_backend->registerKey(combo.key(), combo.keyboardModifiers(), &reason)) {
        if (error) {
            *error = QCoreApplication::translate("GlobalHotkey",
                                                 "The global hotkey %1 could not be registered: %2")
                         .arg(sequence.toString(QKeySequence::NativeText), reason);
        }
        return false;
    }
    m_sequence = sequence;
    return true;
}
