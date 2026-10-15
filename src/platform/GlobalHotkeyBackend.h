#pragma once

#include "GlobalHotkey.h"

#include <functional>
#include <memory>

// The platform part of GlobalHotkey. Exactly one GlobalHotkey_<platform>.cpp defines create().
class GlobalHotkey::Backend
{
public:
    virtual ~Backend() = default;

    static std::unique_ptr<Backend> create(std::function<void()> onActivated);

    virtual bool registerKey(Qt::Key key, Qt::KeyboardModifiers modifiers, QString *error) = 0;
    virtual void unregisterKey() = 0;
};
