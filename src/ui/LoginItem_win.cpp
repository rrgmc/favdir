#include "LoginItem.h"

#include <QCoreApplication>
#include <QDir>
#include <QSettings>

namespace {

// The same key and value as the installer's finish page option in CMakeLists.txt.
const QString kRunKey = QStringLiteral(
    "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run");
const QString kValueName = QStringLiteral("favdir");

} // namespace

namespace LoginItem {

bool isSupported()
{
    return true;
}

bool isEnabled()
{
    return QSettings(kRunKey, QSettings::NativeFormat).contains(kValueName);
}

bool setEnabled(bool enabled, QString *error)
{
    QSettings run(kRunKey, QSettings::NativeFormat);
    if (enabled) {
        const QString exe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
        run.setValue(kValueName, QStringLiteral("\"%1\" --hidden").arg(exe));
    } else {
        run.remove(kValueName);
    }
    run.sync();
    if (run.status() != QSettings::NoError) {
        *error = QCoreApplication::translate("LoginItem", "the registry could not be written.");
        return false;
    }
    return true;
}

bool needsApproval()
{
    return false;
}

void openSystemSettings() {}

void whenLaunched(std::function<void(bool)> callback)
{
    callback(false);
}

} // namespace LoginItem
