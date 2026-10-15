#include "SettingsDialog.h"

#include "LoginItem.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(const Settings &settings, const QString &hotkeyError,
                               bool trayAvailable, QWidget *parent)
    : QDialog(parent)
    , m_original(settings)
    , m_hotkey(new QKeySequenceEdit(this))
    , m_closeToTray(new QCheckBox(tr("&Closing the window keeps favdir running in the tray"), this))
    , m_startAtSignIn(new QCheckBox(tr("&Start favdir when I sign in"), this))
{
    setWindowTitle(tr("Settings"));

    m_hotkey->setMaximumSequenceLength(1);
    m_hotkey->setKeySequence(QKeySequence::fromString(settings.hotkey, QKeySequence::PortableText));
    auto *clear = new QPushButton(tr("C&lear"), this);
    connect(clear, &QPushButton::clicked, m_hotkey, &QKeySequenceEdit::clear);
    auto *hotkeyRow = new QHBoxLayout;
    hotkeyRow->addWidget(m_hotkey, 1);
    hotkeyRow->addWidget(clear);

    auto *form = new QFormLayout;
    form->addRow(tr("Global &hotkey:"), hotkeyRow);
    if (!hotkeyError.isEmpty()) {
        auto *error = new QLabel(hotkeyError, this);
        error->setWordWrap(true);
        QPalette pal = error->palette();
        pal.setColor(QPalette::WindowText, QColor(0xc0, 0x30, 0x30));
        error->setPalette(pal);
        form->addRow(QString(), error);
    }
    auto *hint = new QLabel(tr("Shows or hides favdir from any application. "
                               "Use a combination with Ctrl, Alt or Win/⌘."),
                            this);
    hint->setWordWrap(true);
    form->addRow(QString(), hint);

    m_closeToTray->setChecked(settings.closeToTray && trayAvailable);
    m_closeToTray->setEnabled(trayAvailable);
    form->addRow(QString(), m_closeToTray);

    // The OS holds this state, so read it each time rather than storing it in Settings.
    if (LoginItem::isSupported()) {
        m_startAtSignIn->setChecked(LoginItem::isEnabled());
        form->addRow(QString(), m_startAtSignIn);
    } else {
        m_startAtSignIn->hide();
    }

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);
}

Settings SettingsDialog::settings() const
{
    Settings s;
    s.hotkey = m_hotkey->keySequence().toString(QKeySequence::PortableText);
    // Without a tray the checkbox is disabled; keep the stored choice for when one appears.
    s.closeToTray = m_closeToTray->isEnabled() ? m_closeToTray->isChecked() : m_original.closeToTray;
    return s;
}

bool SettingsDialog::startAtSignIn() const
{
    return m_startAtSignIn->isChecked();
}
