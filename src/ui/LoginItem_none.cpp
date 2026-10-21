#include "LoginItem.h"

#include <QCoreApplication>

namespace LoginItem {

bool isSupported()
{
    return false;
}

bool isEnabled()
{
    return false;
}

bool setEnabled(bool, QString *error)
{
    *error = QCoreApplication::translate("LoginItem",
                                         "starting at sign-in is not supported on this platform.");
    return false;
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
