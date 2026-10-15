#include "GlobalHotkeyBackend.h"

#include <QCoreApplication>

namespace {

class NoBackend final : public GlobalHotkey::Backend
{
public:
    bool registerKey(Qt::Key, Qt::KeyboardModifiers, QString *error) override
    {
        *error = QCoreApplication::translate("GlobalHotkey",
                                             "global hotkeys are not supported on this platform.");
        return false;
    }

    void unregisterKey() override {}
};

} // namespace

std::unique_ptr<GlobalHotkey::Backend> GlobalHotkey::Backend::create(std::function<void()>)
{
    return std::make_unique<NoBackend>();
}
