#include "ActionsDialog.h"

#include "ActionTableEditor.h"
#include "actions/DefaultActions.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>

ActionsDialog::ActionsDialog(const QList<Action> &actions, QWidget *parent)
    : QDialog(parent)
    , m_editor(new ActionTableEditor(ActionTableEditor::Mode::Library, this))
{
    setWindowTitle(tr("Action library"));
    resize(720, 420);
    m_editor->setActions(actions);

    auto *intro = new QLabel(
        tr("Actions marked <b>All folders</b> appear on every folder; a folder can hide them. "
           "The others are optional: add them to a folder with F2 or its right-click menu. "
           "Buttons are numbered in this order."),
        this);
    intro->setWordWrap(true);
    auto *help = new QLabel(ActionTableEditor::helpText(), this);
    help->setWordWrap(true);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    QPushButton *defaults = buttons->addButton(tr("Add &defaults"), QDialogButtonBox::ResetRole);
    connect(defaults, &QPushButton::clicked, this, [this] {
        // Appends the default actions whose names are missing, so nothing is lost.
        QList<Action> current = m_editor->actions();
        for (const Action &d : DefaultActions::create()) {
            const bool present = std::any_of(current.cbegin(), current.cend(),
                                             [&d](const Action &a) { return a.name == d.name; });
            if (!present)
                current.append(d);
        }
        m_editor->setActions(current);
    });
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(intro);
    layout->addWidget(m_editor, 1);
    layout->addWidget(help);
    layout->addWidget(buttons);
}

QList<Action> ActionsDialog::actions() const
{
    return m_editor->actions();
}
