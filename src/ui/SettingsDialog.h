#pragma once

#include "model/FolderStore.h"

#include <QDialog>

class QCheckBox;
class QKeySequenceEdit;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    // `hotkeyError` describes why the current hotkey could not be registered, if it failed.
    SettingsDialog(const Settings &settings, const QString &hotkeyError, bool trayAvailable,
                   QWidget *parent = nullptr);

    Settings settings() const;

    // Whether favdir should start at sign-in. Only meaningful when LoginItem::isSupported().
    bool startAtSignIn() const;

private:
    Settings m_original;
    QKeySequenceEdit *m_hotkey;
    QCheckBox *m_closeToTray;
    QCheckBox *m_startAtSignIn;
};
