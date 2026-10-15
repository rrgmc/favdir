#pragma once

#include <QKeySequence>
#include <QObject>

#include <memory>

// A system-wide keyboard shortcut. Windows uses RegisterHotKey, macOS the Carbon hot key API
// (which needs no Accessibility permission). Elsewhere registration always fails.
class GlobalHotkey : public QObject
{
    Q_OBJECT

public:
    explicit GlobalHotkey(QObject *parent = nullptr);
    ~GlobalHotkey() override;

    // Replaces the current registration. An empty sequence just unregisters. Returns false and
    // sets `error` when the key cannot be mapped or is taken by another application.
    bool setShortcut(const QKeySequence &sequence, QString *error = nullptr);
    QKeySequence shortcut() const { return m_sequence; }

signals:
    void activated();

public:
    class Backend;

private:
    std::unique_ptr<Backend> m_backend;
    QKeySequence m_sequence;
};
